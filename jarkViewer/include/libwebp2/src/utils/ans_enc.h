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
// Asymmetric Numeral System - encoding part
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_UTILS_ANS_ENC_H_
#define WP2_UTILS_ANS_ENC_H_

#include <cassert>
#include <cstdint>

#include "src/utils/ans.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------
// Encoding

class ANSDictionary : public WP2Allocable {
 public:
  ~ANSDictionary() noexcept { Clear(); }
  // Allocates and copy data from 'dict'.
  WP2Status CopyFrom(const ANSDictionary& dict);

  // Initializes dictionary and allocate memory of holding records
  // of 'max_symbol'. The previous memory (if any) is released.
  WP2Status Init(uint32_t max_symbol);

  // Deallocates memory and reset the dictionary structure.
  void Clear();

  // Records use of a given symbol, 'count' times.
  void RecordSymbol(uint32_t symbol, uint32_t count = 1);
  // Number of recorded symbols.
  uint32_t Total() const { return total_; }

  // Returns the current cost in bits for the symbol. Symbol is not recorded.
  float SymbolCost(uint32_t symbol) const;
  float SymbolCost(uint32_t symbol, uint32_t max_symbol) const;
  // Returns the total cost for all symbols.
  float Cost() const;

  // Processes a closed dictionary to populate infos_[] prior to coding.
  // Fails if there are too many symbols (max_symbol_ > ANS_MAX_SYMBOLS).
  WP2Status ToCodingTable();

  // Resets counts (both regular and quantized).
  void ResetCounts();
  // Returns the current counts vector.
  const Vector_u32& Counts() const { return counts_; }

  // Sets counts to a specific value.
  // When recording a symbol afterwards, the quantized counts will not be
  // updated while the normal counts will be. The quantized_counts[] array
  // must have a size 'max_symbol_'.
  WP2Status SetQuantizedCounts(const uint32_t quantized_counts[]);
  // Returns true if quantized_counts have been set.
  bool IsQuantized() const { return !quantized_counts_.empty(); }

  uint32_t MaxSymbol() const { return max_symbol_; }
  const ANSSymbolInfo& GetInfo(uint32_t symbol) const { return infos_[symbol]; }

  uint32_t Log2TabSize() const { return log2_tab_size_; }

  // Returns whether this instance contains mostly the same data as 'other' and
  // thus is probably a clone.
  bool SeemsEquivalentTo(const ANSDictionary& other) const {
    return (max_symbol_ == other.max_symbol_ && total_ == other.total_ &&
            total_quantized_ == other.total_quantized_ &&
            counts_.size() == other.counts_.size() &&
            quantized_counts_.size() == other.quantized_counts_.size() &&
            log2_tab_size_ == other.log2_tab_size_ &&
            infos_.size() == other.infos_.size());
  }

 private:
  uint32_t max_symbol_;
  // Sum of non quantized counts.
  uint32_t total_;
  // Sum of quantized counts.
  uint32_t total_quantized_;
  // counts of symbols.
  Vector_u32 counts_;
  // quantized counts (non-empty if some quantized counts have been set)
  Vector_u32 quantized_counts_;
  // the following params must be set before calling AssembleToBitstream().
  uint32_t log2_tab_size_;  // final expect total of counts[], used for coding
  // (0="default to ANS_LOG_TAB_SIZE")
  // Must be in [0,16] range.

  ANSCodes infos_;  // used during final coding
};

//------------------------------------------------------------------------------
// Decorated container classes for holding pointers

// Class containing several ANSDictionary pointers.
class ANSDictionaries : public VectorNoCtor<ANSDictionary*> {
 public:
  ~ANSDictionaries() { DeepClear(); }

  // Frees everything (deletes the added pointers).
  void DeepClear() {
    for (const ANSDictionary* const d : *this) delete d;
    clear();
  }

  // Copies all dictionaries from 'dicts'.
  WP2Status CopyFrom(const ANSDictionaries& dicts);

  // Considering this ANSDictionaries to be an intact deep clone of 'original',
  // returns the equivalent of the 'dict_to_find' or null if none.
  ANSDictionary* GetEquivalent(const ANSDictionaries& original,
                               const ANSDictionary*  dict_to_find) const;

  // The dictionary is placed last, and can be immediately access using Back().
  WP2Status Add(uint32_t max_symbol);

  // Appends the dictionaries of 'in' into the invoking object.
  // As we are dealing with pointers, 'in' loses ownership and is cleared.
  WP2Status AppendAndClear(ANSDictionaries*  in);

  // Finalize the dictionaries.
  WP2Status ToCodingTable();
};

// Class containing several ANSBinSymbol pointers.
class ANSAdaptiveBits : public VectorNoCtor<ANSBinSymbol> {
 public:
  WP2Status CopyFrom(const ANSAdaptiveBits& dicts);
};

// Class containing several ANSAdaptiveSymbol pointers.
class ANSAdaptiveSymbols : public VectorNoCtor<ANSAdaptiveSymbol> {
 public:
  WP2Status Add(uint32_t max_symbol, const uint16_t*  cdf,
                uint16_t max_proba, bool resize_if_needed = true);
  WP2Status CopyFrom(const ANSAdaptiveSymbols& dicts);
};

//------------------------------------------------------------------------------
// Encoding structure for storing message and generating bitstream.

// Base class for ANS encoders.

class ANSEncBase {
 public:
  virtual ~ANSEncBase() = default;

  // Appends a binary symbol 'bit' with probability defined by the number of 0's
  // and total occurrences. Returns the value of 'bit'.
  virtual uint32_t PutBit(uint32_t bit, uint32_t num_zeros, uint32_t num_total,
                          WP2_OPT_LABEL) = 0;

  // Appends an adaptive binary symbol 'bit', updating 'stats' afterward.
  virtual uint32_t PutABit(uint32_t bit, ANSBinSymbol*  stats,
                           WP2_OPT_LABEL) = 0;

  // Appends a bool with a fifty-fifty probability. Takes one bit.
  inline bool PutBool(bool value, WP2_OPT_LABEL) {
    PutUValue(value ? 1 : 0, 1, label);
    return value;
  }

  // Appends a symbol 'symbol' to the message.
  // 'symbol' should be in range [0, ANS_MAX_SYMBOLS).
  // Returns the symbol.
  virtual uint32_t PutSymbol(uint32_t symbol, const ANSDictionary& dict,
                             WP2_OPT_LABEL) = 0;

  // Appends a symbol 'symbol' to the message using an adaptive small
  // dictionary. 'asym' is *NOT* updated after coding. 'symbol' should be in
  // range [0, APROBA_MAX_SYMBOL). Returns the symbol.
  virtual uint32_t PutSymbol(uint32_t symbol, const ANSAdaptiveSymbol& asym,
                             WP2_OPT_LABEL) = 0;

  // Variants with a max value.
  virtual uint32_t PutSymbol(uint32_t symbol, uint32_t max_symbol,
                             const ANSDictionary& dict, WP2_OPT_LABEL) = 0;
  virtual uint32_t PutSymbol(uint32_t symbol, uint32_t max_symbol,
                             const ANSAdaptiveSymbol& asym, WP2_OPT_LABEL) = 0;

  // Appends a symbol 'symbol' to the message using an adaptive small
  // dictionary. 'asym' is updated after coding. 'symbol' should be in
  // range [0, APROBA_MAX_SYMBOL). Returns the symbol.
  uint32_t PutASymbol(uint32_t symbol, ANSAdaptiveSymbol* const asym,
                      WP2_OPT_LABEL) {
    assert(asym != nullptr);
    PutSymbol(symbol, *asym, label);
    asym->Update(symbol);
    return symbol;
  }

  // Appends a 'value' uniformly distributed in the range [0..1 << bits),
  // with 'bits' in range [0, IO_BITS]. Returns the value.
  // If 'bits' == 0, a value of 0 is stored.
  virtual uint32_t PutUValue(uint32_t value, uint32_t bits, WP2_OPT_LABEL) = 0;

  // Same as PutUValue() with signed values in [ -2^(bits-1) .. 2^(bits-1) ).
  inline int32_t PutSUValue(int32_t value, uint32_t bits, WP2_OPT_LABEL) {
    const int32_t half_range = ((1 << (bits)) >> 1);
    assert(bits == 0 || (-half_range <= value && value < half_range));
    PutUValue((uint32_t)(value + half_range), bits, label);
    return value;
  }

  // Appends a 'value' uniformly distributed in [0..range), an interval fitting
  // in kANSMaxRangeBits bits. Returns the value, for convenience.
  virtual uint32_t PutRValue(uint32_t value, uint32_t range, WP2_OPT_LABEL) = 0;

  // Appends a 'value' uniformly distributed in [min..max], an interval fitting
  // in kANSMaxRangeBits bits. Returns the value, for convenience.
  inline int32_t PutRange(int32_t value, int32_t min, int32_t max,
                          WP2_OPT_LABEL) {
    assert(min <= value && value <= max);
    assert((max - min + 1) <= (int32_t)kANSMaxRange);
    PutRValue(value - min, max - min + 1, label);
    return value;
  }

  // Returns the estimated cost of the final stream in bits, without debug
  // information.
  virtual float GetCost() const = 0;
  // Returns the estimated cost of the final stream in bits.
  // This overload is a speed optimization: the cost for symbols will be
  // computed per dictionary, instead of per symbol.
  // There can be a small difference with GetCost() which does a cumulative
  // sum over floats.
  virtual float GetCost(const ANSDictionaries& dicts) const = 0;
  // Same as GetCost but also adds the cost of the few extra padding words
  float GetCostFull() const;
  float GetCostFull(const ANSDictionaries& dicts) const;

  // Returns the number of tokens that have been buffered.
  virtual uint32_t NumTokens() const = 0;

  // Returns the current status.
  virtual WP2Status GetStatus() const = 0;

  // Adds something to the prefix that will appear when adding bits.
  void AddDebugPrefix(const char prefix[]) {
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
    debug_prefix_ += prefix;
    debug_prefix_ += "/";
#else
    (void)prefix;
#endif
  }
  // Pops the latest addition to the prefix.
  void PopDebugPrefix() {
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
    assert(!debug_prefix_.empty());
    int i = debug_prefix_.size() - 2;
    while (i >= 0 && debug_prefix_[i] != '/') --i;
    debug_prefix_.erase(i + 1);
#endif
  }
  // Copies the prefix from another ANSEnc.
  void CopyDebugPrefix(const ANSEncBase& from) {
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
    debug_prefix_ = from.debug_prefix_;
#endif
    (void)from;
  }

 protected:
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
  std::string debug_prefix_;
#endif
};

struct ANSTokenPage;  // Forward declaration.
struct BufferPage;
class Writer;

class ANSEnc : public ANSEncBase {
 public:
  ANSEnc();
  ~ANSEnc() override;
  ANSEnc(ANSEnc&&) noexcept;
  ANSEnc(const ANSEnc&) = delete;
  ANSEnc& operator=(const ANSEnc&) = delete;

  // Deep copies the given ANSEnc into the current ANSEnc.
  WP2Status Clone(const ANSEnc& e);

  // Generates a bitstream from the current state. The bitstream is stored
  // internally as pages and can be retrieved using WriteBitstreamTo().
  // If clear_tokens is true, token page will be cleared (not even recycled)
  // during the process, leaving only the buffer pages afterward. Note that the
  // tokens should be preserved if calling GetCost().
  WP2Status AssembleToBitstream(bool clear_tokens = false);

  // Returns the size of the assembled bitstream, in bytes.
  // Only valid if AssembleToBitstream() has been called.
  uint32_t GetBitstreamSize() const;

  // Outputs the bitstream.
  WP2Status WriteBitstreamTo(Writer& writer) const;
  WP2Status WriteBitstreamTo(Vector_u8& bits) const;

  // Reclaims all memory, resets the encoder.
  // (to be called in case of error, for instance).
  void Reset();

  // Appends tokens from 'enc' into the invoking object, starting from
  // 'start_token' (0 indexed), and stopping after the given number of tokens.
  WP2Status AppendTokens(const ANSEnc& enc, uint32_t start_token,
                         uint32_t n_tokens);

 public:  // overridden methods
  uint32_t PutBit(uint32_t bit, uint32_t num_zeros, uint32_t num_total,
                  WP2_OPT_LABEL) override {
    const uint32_t proba = (num_zeros << PROBA_BITS) / num_total;
    const uint32_t res = PutBitInternal(bit, proba);
    Trace("%s: bit=%d num_zeros=%d num_total=%d", label, bit, num_zeros,
          num_total);
    return res;
  }
  uint32_t PutABit(uint32_t bit, ANSBinSymbol* const stats,
                   WP2_OPT_LABEL) override {
    const uint32_t res = PutABitInternal(bit, stats);
    Trace("%s: abit=%d", label, bit);
    return res;
  }
  uint32_t PutSymbol(uint32_t symbol, const ANSDictionary& dict,
                     WP2_OPT_LABEL) override {
    // TODO(vrabaud) Have it work with different log2_table.
    assert(dict.Log2TabSize() == ANS_LOG_TAB_SIZE);
    PutSymbolInternal(dict.GetInfo(symbol), /*is_adaptive=*/false);
    Trace("%s: symbol=%u", label, symbol);
    return symbol;
  }
  uint32_t PutSymbol(uint32_t symbol, const ANSAdaptiveSymbol& asym,
                     WP2_OPT_LABEL) override {
    PutSymbolInternal(asym.GetInfo(symbol), /*is_adaptive=*/true);
    Trace("%s: symbol=%u", label, symbol);
    return symbol;
  }
  uint32_t PutSymbol(uint32_t symbol, uint32_t max_symbol,
                     const ANSDictionary& dict, WP2_OPT_LABEL) override {
    return PutSymbolInternal(symbol, max_symbol, dict, label);
  }
  uint32_t PutSymbol(uint32_t symbol, uint32_t max_symbol,
                     const ANSAdaptiveSymbol& asym, WP2_OPT_LABEL) override {
    return PutSymbolInternal(symbol, max_symbol, asym, label);
  }
  uint32_t PutUValue(uint32_t value, uint32_t bits, WP2_OPT_LABEL) override {
    const uint32_t res = PutUValueInternal(value, bits);
    Trace("%s: value=0x%x bits=%u", label, value, bits);
    return res;
  }
  uint32_t PutRValue(uint32_t value, uint32_t range, WP2_OPT_LABEL) override {
    const uint32_t res = PutRValueInternal(value, range);
    Trace("%s: value=0x%x range=0x%x", label, value, range);
    return res;
  }
  float GetCost() const override;
  float GetCost(const ANSDictionaries& dicts) const override;

  WP2Status GetStatus() const override { return status_; }

  uint32_t NumTokens() const override;

 private:
  friend void swap(ANSEnc& e1, ANSEnc& e2);

  // internal PutXXX versions without label[]
  uint32_t PutBitInternal(uint32_t bit, uint32_t proba);
  uint32_t PutABitInternal(uint32_t bit, ANSBinSymbol*  stats);
  // is_adaptive indicates whether the symbol is adaptive or not.
  void PutSymbolInternal(const ANSSymbolInfo& info, bool is_adaptive);
  uint32_t PutUValueInternal(uint32_t value, uint32_t bits);
  uint32_t PutRValueInternal(uint32_t value, uint32_t range);
  template <class T>  // T must have a ::GetInfo(symbol)
  uint32_t PutSymbolInternal(uint32_t symbol, uint32_t max_symbol,
                             const T& dict, WP2_OPT_LABEL) {
    assert(symbol <= max_symbol);
    const ANSSymbolInfo info_max = dict.GetInfo(max_symbol);
    const ANSSymbolInfo info = dict.GetInfo(symbol).Rescale(info_max);
    PutSymbolInternal(info, /*is_adaptive=*/false);
    Trace("%s: symbol=%u", label, info.symbol);
    return info.symbol;
  }

  void EnqueueToken(uint32_t tok);

#if defined(WP2_ENC_DEC_MATCH)
  uint32_t PutDebugUValue(uint32_t value, uint32_t bits);
#endif

  template <typename... Ts>
  inline void Trace(const char* format, const char label[], Ts... extra) {
    (void)sizeof...(extra);
    (void)label;
    WP2Trace(format, debug_prefix_, label, extra...);
#if defined(WP2_ENC_DEC_MATCH)
#if defined(WP2_ENC_DEC_DEEP_MATCH)
    const std::string str = ANSString(debug_prefix_, label, extra...);
    PutDebugUValue(str.size(), 8);
    for (char c : str) PutDebugUValue(c, 8);
#else
    const uint16_t hash = ANSStringHash(debug_prefix_, label, extra...);
    if (GetStatus() == WP2_STATUS_OK) PutDebugUValue(hash, 16);
#endif  // defined(WP2_ENC_DEC_DEEP_MATCH)
#endif  // defined(WP2_ENC_DEC_MATCH)
  }

  // emits the lower bits of 's' and returns the new value
  inline uint32_t EmitWord(uint32_t s);

  void EmitBit(uint32_t bit, uint32_t p0);
  void EmitASymbol(uint32_t offset, uint32_t freq);
  void EmitUValue(uint32_t value, int bits);
  void EmitRValue(uint32_t value, uint32_t range);

 protected:
  WP2Status status_;
  uint32_t state_;  // state for the coder

  BufferPage* pages_;      // paginated output
  uint32_t num_pages_;     // number of pages (for a fast total_size eval)
  uint8_t* buffer_;        // current write-buffer pointing to the last pages_
  uint32_t buffer_pos_;    // write position, going down
  bool NewBufferPage();    // allocate a new buffer page in pages_
  void FreeBufferPages();  // clears the pages_

  // Stored message, composed of "tokens" which can be bits, uvalues or symbols.
  // Pointer to the most recent (last) page of tokens. Use tokens_->prev_ to
  // access previous pages.
  ANSTokenPage* tokens_;
  // total number of tokens in the message
  uint32_t nb_tokens_;
  bool NewTokenPage();    // allocates a new token page in tokens_
  void FreeTokenPages();  // clears the tokens_
};

void swap(ANSEnc& e1, ANSEnc& e2);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_UTILS_ANS_ENC_H_
