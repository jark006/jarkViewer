// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// -----------------------------------------------------------------------------
//
// Asymmetric Numeral System coder
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_UTILS_ANS_H_
#define WP2_UTILS_ANS_H_

#include <array>
#include <cassert>
#include <cstdint>

#include "src/dsp/dsp.h"
#include "src/wp2/base.h"
#include "src/wp2/debug.h"

#ifdef HAVE_CONFIG_H
#include "src/wp2/config.h"
#endif

#include "src/dsp/math.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"

#define WP2_OPT_LABEL const char label[]

namespace WP2 {

class DataSource;

#if defined(WP2_BITTRACE)
// For each label, store the amount of bits, and the number of occurrences.
// (can be useful whether WP2_BITTRACE is defined or not)
typedef std::map<const std::string, LabelStats> BitCounts;
#endif

// When uncommenting the following, the 'label' string used when inputting
// anything in the ANS encoder is also inserted, as a hash or as the whole
// string if WP2_ENC_DEC_DEEP_MATCH is defined. When decoding the ANS
// stream, it is asserted that the same string is used to make sure that the
// same information is read in the same order.
// #define WP2_ENC_DEC_MATCH

#define ANS_LOG_TAB_SIZE 14
#define ANS_TAB_SIZE (1u << ANS_LOG_TAB_SIZE)

// maximum numbers of symbols for dictionaries and tables
#define ANS_MAX_SYMBOLS 1024  // 10 bits

// bits have a probability within [0, PROBA_MAX]
#define PROBA_BITS 16
#define PROBA_MAX (1u << PROBA_BITS)

// how many bits used during I/O between state_ and bitstream
#define IO_BITS 16
#define IO_LIMIT_BITS 16
#define ANS_SIGNATURE (0xf3 * IO_LIMIT)  // Initial state, used as CRC.

static_assert(IO_BITS == 16, "IO_BITS should be == 16");
static constexpr uint32_t kANSPaddingCost = (IO_LIMIT_BITS + IO_BITS);

// Max number (inclusive) of bits for the range used during range-value coding.
static constexpr uint32_t kANSMaxRangeBits = 14;
// Maximum range that can be used. As a range is always > 0, it is stored -1. We
// can hence go up to (1u << kANSMaxRangeBits) included so that the storage fits
// on kANSMaxRangeBits bits.
static constexpr uint32_t kANSMaxRange = (1u << kANSMaxRangeBits);
// Max number (inclusive) of bits to use to describe a uniform value.
static constexpr uint32_t kANSMaxUniformBits = IO_BITS;

// Adaptive symbol with a small dictionary
// APROBA_BITS must be a little less precision than PROBA_BITS in order to fit
// the range in the ANSTokenInfoASym as 14+14 bits.
#define APROBA_BITS 14
#define APROBA_MAX (1 << APROBA_BITS)
#define APROBA_ADAPT_SPEED 4096  // must be strictly less than 32768
static constexpr uint32_t kANSAProbaInvalidSpeed = (1 << PROBA_BITS);

// some derived constants:
#define IO_BYTES (IO_BITS / 8)
#define IO_LIMIT (1ull << IO_LIMIT_BITS)

// TODO(vrabaud): make sure the ANS read/write functions are optimized/inlined.

//------------------------------------------------------------------------------
// Decoding

struct ANSSymbolInfo {  // Symbol information.
  uint16_t offset;      // offset to the first ANSSymbolInfo of that symbol
  uint16_t freq;        // frequency in [0, PROBA_MAX]
  uint16_t symbol;
  ANSSymbolInfo Rescale(const ANSSymbolInfo& max_symbol,
                        uint32_t tab_size = ANS_TAB_SIZE) const;
};
typedef VectorNoCtor<ANSSymbolInfo> ANSCodes;

// Struct for recording binary event and updating probability.
// TODO(maryla): merge this into ANSAdaptiveSymbol to make them simpler to use?
class ANSBinSymbol {
 public:
  explicit ANSBinSymbol(uint32_t p0 = 1, uint32_t p1 = 1);
  ANSBinSymbol(ANSBinSymbol&&) = default;
  ANSBinSymbol(const ANSBinSymbol& other) = default;
  ANSBinSymbol& operator=(const ANSBinSymbol&) = default;
  // update observation and return 'bit'
  inline uint32_t Update(uint32_t bit) {
    if (num_total_ < kMaxSum) UpdateCount(bit);
    return bit;
  }
  // Returns the number of occurrences 0 happened.
  uint32_t NumZeros() const { return num_zeros_; }
  // Returns the number of occurrences of the bit.
  uint32_t NumTotal() const { return num_total_; }

  // cost of storing the given 'bit'.
  float GetCost(uint32_t bit) const {
    if (num_zeros_ == 0 || num_zeros_ == num_total_) return 0;
    return WP2Log2(num_total_) -
           WP2Log2(bit ? num_total_ - num_zeros_ : num_zeros_);
  }

 private:
  // we are dealing with quite stationary sources, so updating the proba
  // past kMaxSum events is usually irrelevant.
  static constexpr uint32_t kMaxSum = 256u;
  uint16_t num_zeros_, num_total_;

  void UpdateCount(uint32_t bit);
};

// Decoder only:
// Generate a 'flat' spread table from a set of frequencies. The counts[]
// distributions is re-normalized such that its sum is equal to 'table_size'.
// 'codes' will be resized to 'table_size' if needed.
// Returns ok if there are no ill-defined counts.
WP2Status ANSCountsToSpreadTable(uint32_t counts[], uint32_t max_symbol,
                                 uint32_t table_size, ANSCodes& codes);

// Analyze counts[0..size) and renormalize it so that the total is equal to
// 'sum' exactly.
WP2Status ANSNormalizeCounts(uint32_t counts[], uint32_t size, uint32_t sum);

// Stores and adapts small APROBA_MAX_SYMBOL-dictionary
class ANSAdaptiveSymbol {
 public:
  enum class Method {
    // Adaptation is constant with a speed in kAdaptationSpeeds.
    kConstant,
    // Adaptation is exactly the AOM one, with a rate depending on how many
    // symbols got already written.
    kAOM,
    kNum
  };

  ANSAdaptiveSymbol() : method_(Method::kNum) {}

  // Initializes with a uniform distribution.
  void InitFromUniform(uint32_t max_symbol);
  // Initializes from an un-normalized pdf. Norm is APROBA_MAX.
  // Returns false if the distribution can't be normalized.
  WP2Status InitFromCounts(const uint32_t counts[], uint32_t max_symbol);
  // Initializes from a cdf, normalization to APROBA_MAX is performed if the CDF
  // is normalized to max_proba.
  WP2Status InitFromCDF(const uint16_t* cdf, uint32_t max_symbol,
                        uint32_t max_proba = APROBA_MAX);
  void CopyFrom(const ANSAdaptiveSymbol& other);

  // Adaptation speed goes from 0x3fffu (fast adaptation) to 0 (no adaptation).
  // Special case is speed 0xffffu (instant adaptation)
  void SetAdaptationSpeed(Method method,
                          uint32_t speed = kANSAProbaInvalidSpeed);

  // proba is in [0, APROBA_MAX) range
  ANSSymbolInfo GetSymbol(uint32_t proba) const {
    return GetInfo(ANSGetSymbol(max_symbol_, cumul_.data(), proba));
  }
  // Convenience representation of ranges as ANSSymbolInfo.
  // 'sym' must be in [0, max_symbol_) range.
  inline ANSSymbolInfo GetInfo(uint32_t sym) const {
    assert(sym < max_symbol_);
    ANSSymbolInfo info;
    info.symbol = (uint16_t)sym;
    info.offset = cumul_[sym];
    info.freq = cumul_[sym + 1] - info.offset;
    return info;
  }

  // Updates the cumulative distribution after coding symbol 'sym',
  // preserving the norm equal to APROBA_MAX.
  void Update(uint32_t sym);

  // Returns proba = (cumul[sym + 1] - cumul[sym]) / APROBA_MAX
  float GetProba(uint32_t sym) const {
    assert(sym < max_symbol_);
    const uint32_t w = cumul_[sym + 1] - cumul_[sym];
    return (1.f / APROBA_MAX) * w;
  }
  float GetCost(uint32_t sym) const {
    return kCostTable[cumul_[sym + 1] - cumul_[sym]];
  }
  float GetCost(uint32_t sym, uint32_t max_symbol) const {
    assert(sym <= max_symbol);
    return kCostTable[cumul_[sym + 1] - cumul_[sym]] -
           kCostTable[cumul_[max_symbol + 1] - cumul_[0]];
  }

  // debug
  void Print(bool use_percents = true) const;
  void PrintProbas(uint32_t norm = 0) const;

  // Computes the 'distance' between two cdf. Can be used to
  // monitor convergence to final stationary distribution.
  float ComputeDistance(const ANSAdaptiveSymbol& from) const;
  // Distance (in [0..1]) based on variance.
  float ComputeNonUniformDistance() const;

  // Will return the optimal speed index to use to reach the final distribution.
  // The index refers to kAdaptationSpeeds[].
  // If header_bits is not nullptr, and in case a fixed distribution is better
  // than an adaptive one (ie.: adaptation speed looks too fast), will return
  // the number of header-bits that can be spent to code the fixed distribution.
  // If fixed-proba is not advantageous, 0 is returned as header_bits.
  void FindBestAdaptationSpeed(const Vector_u8& syms, Method* method,
                               uint32_t* speed,
                               uint32_t* header_bits = nullptr) const;
  // Evaluates the cost of the sequence of symbols for the given
  // adaptation speed.
  float ScoreSequence(Method method, uint32_t speed,
                      const Vector_u8& syms) const;

  const uint16_t* GetCumul() const { return cumul_.data(); }
  uint32_t NumSymbols() const { return max_symbol_; }

  static inline float GetFreqCost(uint32_t freq) { return kCostTable[freq]; }

 protected:
  Method method_;
  uint32_t max_symbol_;  // At most APROBA_MAX_SYMBOL.
  uint32_t adapt_factor_ = APROBA_ADAPT_SPEED;
  // cumulative frequency. cumul[0] is always 0, cumul[last] is always
  // APROBA_MAX.
  std::array<uint16_t, APROBA_MAX_SYMBOL + 1> cumul_;

  void InitCDF();  // fills cdf_base_[] and cdf_var_[] according to cumul_[]
  std::array<uint16_t, APROBA_MAX_SYMBOL> cdf_base_;
  std::array<uint16_t, APROBA_MAX_SYMBOL * 2 - 1> cdf_var_;

  // precalc for -log2(GetProba())
  //  TODO(skal): Costs should be normalized to int16_t, not float.
  static const float kCostTable[APROBA_MAX + 1];
};

#if defined(WP2_ENC_DEC_MATCH)
#if defined(WP2_ENC_DEC_DEEP_MATCH)
template <typename... Ts>
inline std::string ANSString(const std::string& debug_prefix,
                             const char label[], Ts... extra) {
  std::string str = debug_prefix;
  str += label;
  const uint16_t extra_arr[] = {(const uint16_t)extra...};
  for (uint16_t e : extra_arr) str += ", " + std::to_string(e);
  return str;
}
#else
// djb2 string hash
template <typename... Ts>
inline uint16_t ANSStringHash(const std::string& debug_prefix,
                              const char label[], Ts... extra) {
  uint16_t hash = 5381;
  for (char c : debug_prefix) hash = ((hash << 5) + hash) + c;
  for (int i = 0; label[i] != 0; ++i) hash = ((hash << 5) + hash) + label[i];
  const uint16_t extra_arr[] = {(const uint16_t)extra...};
  for (uint16_t e : extra_arr) hash = ((hash << 5) + hash) + e;
  return hash;
}
#endif  // defined(WP2_ENC_DEC_DEEP_MATCH)
#endif  // defined(WP2_ENC_DEC_MATCH)

// Class for holding the decoding state.
class ANSDec {
 public:
  explicit ANSDec(DataSource* const data_source) { Init(data_source); }
  ANSDec(const ANSDec&) = delete;

  // initializes a new ANSDec object.
  void Init(DataSource* data_source);

  // Decodes a symbol, according to the spread table 'codes'.
  uint32_t ReadSymbol(const ANSSymbolInfo codes[], uint32_t log2_tab_size,
                      WP2_OPT_LABEL) {
    const uint32_t symbol = ReadSymbolInternal(codes, log2_tab_size);
    Trace("%s: symbol=%u", label, symbol);
    BitTrace(symbol, LabelStats::Type::Symbol, label);
    return symbol;
  }

  // Decodes a symbol, according to the info.
  // 'max_index' is the index in 'codes' for which we know for sure the read
  // value is not strictly superior.
  uint32_t ReadSymbol(const ANSSymbolInfo codes[], uint32_t log2_tab_size,
                      uint32_t max_index, WP2_OPT_LABEL) {
    const uint32_t symbol = ReadSymbolInternal(codes, log2_tab_size, max_index);
    Trace("%s: symbol=%u", label, symbol);
    BitTrace(symbol, LabelStats::Type::Symbol, label);
    return symbol;
  }

  // Decodes a symbol from small adaptive dictionary.
  // 'asym' probability is adapted.
  uint32_t ReadASymbol(ANSAdaptiveSymbol* const asym, WP2_OPT_LABEL) {
    const uint32_t symbol = ReadSymbol(*asym, label);
    asym->Update(symbol);
    return symbol;
  }
  // Decodes a symbol from small adaptive dictionary.
  // 'asym' probability is *NOT* adapted.
  uint32_t ReadSymbol(const ANSAdaptiveSymbol& asym, WP2_OPT_LABEL) {
    const uint32_t symbol = ReadSymbolInternal(asym);
    Trace("%s: symbol=%u", label, symbol);
    BitTrace(symbol, LabelStats::Type::ASymbol, label);
    return symbol;
  }
  uint32_t ReadSymbol(const ANSAdaptiveSymbol& asym, uint32_t max_index,
                      WP2_OPT_LABEL) {
    const uint32_t symbol = ReadSymbolInternal(asym, max_index);
    Trace("%s: symbol=%u", label, symbol);
    BitTrace(symbol, LabelStats::Type::ASymbol, label);
    return symbol;
  }

  // Decodes a binary symbol with probability defined by the number of 0's and
  // total occurrences.
  uint32_t ReadBit(uint32_t num_zeros, uint32_t num_total, WP2_OPT_LABEL) {
    const uint32_t bit = ReadBitInternal(num_zeros, num_total);
    Trace("%s: bit=%u num_zeros=%d num_total=%d", label, bit, num_zeros,
          num_total);
    BitTrace(bit, LabelStats::Type::Bit, label);
    return bit;
  }

  // Decodes an adaptive binary symbol with statistics 'stats'.
  // 'stats' is updated upon return.
  uint32_t ReadABit(ANSBinSymbol* const stats, WP2_OPT_LABEL) {
    const uint32_t bit = ReadABitInternal(stats);
    Trace("%s: abit=%u", label, bit);
    BitTrace(bit, LabelStats::Type::ABit, label);
    return bit;
  }

  // Decodes a bool with a fifty-fifty probability.
  inline bool ReadBool(WP2_OPT_LABEL) { return (ReadUValue(1, label) != 0); }

  // Decodes a uniform value known to be in range [0..1 << bits), with 'bits'
  // in [0, IO_BITS] range.
  // If 'bits' == 0, it returns 0.
  uint32_t ReadUValue(uint32_t bits, WP2_OPT_LABEL) {
    const uint32_t value = ReadUValueInternal(bits);
    Trace("%s: value=0x%x bits=%u", label, value, bits);
    BitTrace(value, LabelStats::Type::UValue, label);
    return value;
  }

  // Same as ReadUValue() with signed values in [ -2^(bits-1) .. 2^(bits-1) ).
  inline int32_t ReadSUValue(uint32_t bits, WP2_OPT_LABEL) {
    return (int32_t)ReadUValue(bits, label) - ((1 << (bits)) >> 1);
  }

  // Decodes a uniform value known to be in [0..range), an interval fitting in
  // kANSMaxRangeBits bits.
  uint32_t ReadRValue(uint32_t range, WP2_OPT_LABEL) {
    const uint32_t value = ReadRValueInternal(range);
    Trace("%s: value=0x%x range=0x%x", label, value, range);
    BitTrace(value, LabelStats::Type::RValue, label);
    return value;
  }

  // Decodes a uniform value known to be in [min..max], an interval fitting in
  // kANSMaxRangeBits bits.
  inline int32_t ReadRange(int32_t min, int32_t max, WP2_OPT_LABEL) {
    assert(min <= max);
    return ReadRValue(max - min + 1, label) + min;
  }

  // Returns true if no error occurred.
  WP2Status GetStatus() const { return status_; }

  // Add something to the prefix that will appear when adding bits.
  void AddDebugPrefix(const char prefix[]) {
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
    debug_prefix_ += prefix;
#if defined(WP2_BITTRACE)
    counters_[kPrefixStr + debug_prefix_].bits = 0.f;
    ++counters_[kPrefixStr + debug_prefix_].num_occurrences;
#endif
    debug_prefix_ += "/";
#else
    (void)prefix;
#endif
  }
  // Pop the latest addition to the prefix.
  void PopDebugPrefix() {
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
    assert(!debug_prefix_.empty());
    int i = debug_prefix_.size() - 2;
    while (i >= 0 && debug_prefix_[i] != '/') --i;
    debug_prefix_.erase(i + 1);
#endif
  }

 private:
  // Logs something and makes sure the label is the same.
  template <typename... Ts>
  inline void Trace(const char* format, const char label[], Ts... extra) {
    (void)sizeof...(extra);
    (void)label;
    WP2Trace(format, debug_prefix_, label, extra...);
#if defined(WP2_ENC_DEC_MATCH)
#if defined(WP2_ENC_DEC_DEEP_MATCH)
    const std::string str = ANSString(debug_prefix_, label, extra...);
    std::string str_read(ReadDebugUValue(8), '.');
    for (char& c : str_read) c = (char)ReadDebugUValue(8);
    if (str_read != str) assert(false);
#else
    const uint16_t hash = ANSStringHash(debug_prefix_, label, extra...);
    const uint16_t hash_read = ReadDebugUValue(16);
    if (hash_read != hash) assert(false);
#endif  // defined(WP2_ENC_DEC_DEEP_MATCH)
#endif  // defined(WP2_ENC_DEC_MATCH)
  }

  // internal ReadXXX versions without label[]
  uint32_t ReadSymbolInternal(const ANSSymbolInfo codes[],
                              uint32_t log2_tab_size);
  uint32_t ReadSymbolInternal(const ANSSymbolInfo codes[],
                              uint32_t log2_tab_size, uint32_t max_index);
  uint32_t ReadSymbolInternal(const ANSAdaptiveSymbol& asym);
  uint32_t ReadSymbolInternal(const ANSAdaptiveSymbol& asym,
                              uint32_t max_index);
  uint32_t ReadRValueInternal(uint32_t range);
  uint32_t ReadUValueInternal(uint32_t bits);
  uint32_t ReadABitInternal(ANSBinSymbol* stats);
  uint32_t ReadBitInternal(uint32_t num_zeros, uint32_t num_total);

  // Shift 'state' and read-in the lower bits. Return the new value.
  // Typically called as: 'state_ = ReadNextWord(state_);'
  inline uint32_t ReadNextWord(uint32_t state);

  DataSource* data_source_;
  uint32_t state_;
  WP2Status status_;
#if defined(WP2_ENC_DEC_MATCH)
  uint32_t ReadDebugUValue(uint32_t bits);
#endif
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
  std::string debug_prefix_;
#endif

 public:
  // Returns the number of bytes read from the bitstream.
  uint32_t GetNumUsedBytes() const;
  // Returns a lower bound of the number of bytes completely decoded between
  // 'num_bytes_before' and 'num_bytes_after'.
  static uint32_t GetMinNumUsedBytesDiff(uint32_t num_used_bytes_before,
                                         uint32_t num_used_bytes_after);

 protected:
#if defined(WP2_BITTRACE)
  double last_pos_ = 0.;
  double cur_pos_ = 0.;
  BitCounts counters_;
  // Next reads will be registered in 'counters_custom_' with this key.
  std::string counters_custom_key_;
  // Custom counter on which the user has write access (e.g. to
  // initialize/clean).
  BitCounts counters_custom_;
  // If true, merge custom bit traces until next Pop().
  bool merge_custom_until_pop_ = false;
  // Tells whether the padding has been added to counters_ yet.
  bool is_padding_counted_ = false;
  void BitTrace(uint32_t value, LabelStats::Type type, WP2_OPT_LABEL);

 public:
  // Precision of GetBitCount() compared to the actual used bits is at most
  // kANSPaddingCost or 0.16%.
  static constexpr double kBitCountAccuracy = 0.0016;

  // This string will be prepended to debug prefixes when added to the ANSDec
  // to keep track of their call number. It should be a string the user
  // will not use in a prefix.
  static constexpr const char* kPrefixStr = "____BITTRACE____";
  const BitCounts& GetBitTraces() const { return counters_; }
  BitCounts& GetBitTracesCustom() { return counters_custom_; }
  void ClearBitTracesCustom() { counters_custom_.clear(); }
  const std::string& GetBitTracesCustomKey() const {
    return counters_custom_key_;
  }
  void PushBitTracesCustomPrefix(const char* const suffix_of_prefix,
                                 bool merge_until_pop = false);
  void PopBitTracesCustomPrefix(const char* const suffix_of_prefix);
  double GetBitCount() const { return cur_pos_; }
#else
  static inline void BitTrace(uint32_t value, LabelStats::Type type,
                              WP2_OPT_LABEL) {
    (void)value;
    (void)type;
    (void)label;
  }

 public:
  void PushBitTracesCustomPrefix(const char* const, bool = false) {}
  void PopBitTracesCustomPrefix(const char* const) {}
  double GetBitCount() const { return 0.f; }
#endif
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_UTILS_ANS_H_
