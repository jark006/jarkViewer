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
// ANS symbol encoding.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_ENC_SYMBOLS_ENC_H_
#define WP2_ENC_SYMBOLS_ENC_H_

#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>

#include "src/common/lossy/residuals.h"
#include "src/common/symbols.h"
#include "src/utils/ans.h"
#include "src/utils/ans_enc.h"
#include "src/utils/ans_utils.h"
#include "src/utils/quantizer.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

// Interface for a class that processes symbols (to record/write/count them...)
class SymbolManager {
 public:
  virtual ~SymbolManager() = default;

  // Processes the symbol of type 'sym' and value 'value' in the 'cluster'th
  // cluster. Returns the value.
  int32_t Process(uint32_t sym, uint32_t cluster, int32_t value, WP2_OPT_LABEL,
                  ANSEncBase* enc) {
    return ProcessInternal(sym, cluster, value, /*use_max_value=*/false,
                           /*max_value=*/0, label, enc, /*cost=*/nullptr);
  }

  int32_t Process(uint32_t sym, int32_t value, WP2_OPT_LABEL, ANSEncBase* enc) {
    return Process(sym, /*cluster=*/0, value, label, enc);
  }

  // Does the same as above except that it caps the value to be in
  // [0, max_value]. This overload can be costly on the decoder side as it
  // requires a binary search in [0, max_value]. This function is usually to
  // be used at the end of an interval where we know we can cap values because
  // they cannot be out of it.
  int32_t Process(uint32_t sym, uint32_t cluster, int32_t value,
                  uint32_t max_value, WP2_OPT_LABEL, ANSEncBase* enc) {
    return ProcessInternal(sym, cluster, value, /*use_max_value=*/true,
                           max_value, label, enc, /*cost=*/nullptr);
  }

  // Same as process, but if 'cost' is not nullptr, it is set to the cost of
  // storing this symbol.
  void ProcessWithCost(uint32_t sym, uint32_t cluster, int32_t value,
                       WP2_OPT_LABEL, ANSEncBase* enc, float* cost) {
    ProcessInternal(sym, cluster, value, /*use_max_value=*/false,
                    /*max_value=*/0, label, enc, cost);
  }
  void ProcessWithCost(uint32_t sym, uint32_t cluster, int32_t value,
                       uint32_t max_value, WP2_OPT_LABEL, ANSEncBase* enc,
                       float* cost) {
    ProcessInternal(sym, cluster, value, /*use_max_value=*/true, max_value,
                    label, enc, cost);
  }

  virtual const SymbolsInfo& symbols_info() const = 0;

 protected:
  virtual int32_t ProcessInternal(uint32_t sym, uint32_t cluster, int32_t value,
                                  bool use_max_value, uint32_t max_value,
                                  WP2_OPT_LABEL, ANSEncBase* enc,
                                  float* cost) = 0;
};

// Class for counting the usage of symbols.
// Values are stored as value - value_min, where value_min is the minimal value
// the symbol can have.
class SymbolRecorder : public SymbolManager {
 public:
  const SymbolsInfo& symbols_info() const override { return symbols_info_; }

  // Initializes and allocates memory. 'num_records' is an upper bound on the
  // number of records for a symbol with kAdaptiveWithAutoSpeed, so that storage
  // can be pre-allocated. If more symbols will need to be recorded, code will
  // fail.
  WP2Status Allocate(const SymbolsInfo& symbols_info, uint32_t num_records);
  WP2Status CopyFrom(const SymbolRecorder& other);
  void DeepClear();
  // Resets the stats for all the symbols without deallocating the memory.
  void ResetCounts();

  // Copies the dictionaries to a backup: useful for multi-pass.
  WP2Status MakeBackup();

  // Resets stats.
  WP2Status ResetRecord(bool reset_backup);

  const ANSDictionary& GetRecordedDict(uint32_t sym, uint32_t cluster) const;
  const Vector_u8& GetRecordedValues(uint32_t sym, uint32_t cluster) const;
  const ANSBinSymbol& GetABit(uint32_t sym, uint32_t cluster) const;
  const ANSAdaptiveSymbol& GetASymbol(uint32_t sym, uint32_t cluster) const;
  // Returns the current value updated with everything stored so far at default
  // speed.
  const ANSAdaptiveSymbol& GetASymbolWithSpeed(uint32_t sym,
                                               uint32_t cluster) const;

  // Returns the dictionary of recorded values from the previous pass (before
  // the last call to ResetRecord()).
  const ANSDictionary& GetDictPreviousPass(uint32_t sym,
                                           uint32_t cluster) const;

 protected:
  int32_t ProcessInternal(uint32_t sym, uint32_t cluster, int32_t value,
                          bool use_max_value, uint32_t max_value, WP2_OPT_LABEL,
                          ANSEncBase* enc, float*) override;

 private:
  ANSDictionary* GetDict(uint32_t sym, uint32_t cluster);
  ANSBinSymbol* GetABit(uint32_t sym, uint32_t cluster);
  ANSAdaptiveSymbol* GetASymbol(uint32_t sym, uint32_t cluster);

  SymbolsInfo symbols_info_;
  // For each symbol 'sym', the statistics for all clusters start at
  // index_[sym], either in dicts_, a_bits_, a_symbols_ or values_ depending on
  // the type.
  std::array<uint32_t, kSymbolNumMax> index_;
  // Index for ASymbol in the values_ vector.
  std::array<uint32_t, kSymbolNumMax> values_index_;
  // Dictionaries, for StorageMethod::kAuto
  ANSDictionaries dicts_;
  ANSDictionaries dicts_previous_pass_;
  // Adaptive bits, for StorageMethod::kAdaptive with range == 2
  ANSAdaptiveBits a_bits_;
  // Adaptive symbols, for StorageMethod::kAdaptive with range != 2
  ANSAdaptiveSymbols a_symbols_;
  // Full values, for StorageMethod::kAdaptiveWithAutoSpeed
  Vector<Vector_u8> values_;
};

// Class for using with an ANSEncCounter to count space taken. It's meant to be
// used with an ANSEncCounter. Then call GetCost() on the counter to
// know the size.
// The base SymbolCounter class does NOT update propabilities for adaptive
// bits/symbols. See also UpdatingSymbolCounter.
class SymbolCounter : public SymbolManager, public WP2Allocable {
 public:
  static constexpr uint32_t kMaxLossyClusters =
      kResidualClusters * ResidualIO::kNumSectors;

  explicit SymbolCounter(const SymbolRecorder* recorder)
      : recorder_(recorder), symbols_info_(recorder_->symbols_info()) {}

  const SymbolsInfo& symbols_info() const override { return symbols_info_; }

  // Allocates memory. Callers must decide in advance which symbols will be
  // used. Calls to Process() for symbols that were not passed in Allocate()
  // may fail.
  virtual WP2Status Allocate(std::initializer_list<uint32_t> syms);
  virtual void Clear();

 protected:
  int32_t ProcessInternal(uint32_t sym, uint32_t cluster, int32_t value_in,
                          bool use_max_value, uint32_t max_value, WP2_OPT_LABEL,
                          ANSEncBase* enc, float*) override;

  const SymbolRecorder* const recorder_;
  const SymbolsInfo& symbols_info_;
  static constexpr uint32_t kInvalidIndex =
      std::numeric_limits<uint32_t>::max();
};

// SymbolCounter that updates probabilities for adaptive bits/symbols. It's
// slower and uses more memory.
class UpdatingSymbolCounter : public SymbolCounter {
 public:
  explicit UpdatingSymbolCounter(const SymbolRecorder* recorder)
      : SymbolCounter(recorder) {}

  WP2Status CopyFrom(const UpdatingSymbolCounter& other);

  WP2Status Allocate(std::initializer_list<uint32_t> syms) override;
  void Clear() override;

 protected:
  int32_t ProcessInternal(uint32_t sym, uint32_t cluster, int32_t value_in,
                          bool use_max_value, uint32_t max_value, WP2_OPT_LABEL,
                          ANSEncBase* enc, float*) override;

 private:
  ANSBinSymbol* GetABit(uint32_t sym, uint32_t cluster);
  ANSAdaptiveSymbol* GetASymbol(uint32_t sym, uint32_t cluster);

  // For each symbol, the index in a_bits_/a_symbols_ where its storage for each
  // cluster starts.
  std::array<uint32_t, kSymbolNumMax> indices_;
  // For ABit/ASymbol, store how many can used at most in num_*_, if they are
  // initialized in *_initialized_ and all of them in *_.
  // TODO(vrabaud) Check the dimensions once we remove the clusters in the
  //               residual code.
  // Adaptive bits, for StorageMethod::kAdaptive with range == 2
  uint32_t num_a_bits_;
  std::array<bool, 3094> a_bit_initialized_;
  ANSAdaptiveBits a_bits_;
  // Adaptive symbols, for StorageMethod::kAdaptive with range != 2
  uint32_t num_a_symbols_;
  std::array<bool, 892> a_symbol_initialized_;
  ANSAdaptiveSymbols a_symbols_;
};

// Class simplifying the writing of symbols.
// When writing a symbol to the bitstream:
//  - it does not have to be written if it is trivial (it is always the same
//  value)
//  - it is written in a certain dictionary depending on its type
// WriteHeader() must be called for each symbol type before it can be
// written with Process().
struct SymbolWriterStatExtra {
  const ANSDictionary* dict;
  uint32_t mapping_size;
};
class SymbolWriter : public SymbolManager,
                     public SymbolIO<SymbolWriterStatExtra>,
                     public WP2Allocable {
 public:
  // Sets basic information about symbols.
  WP2Status Init(const SymbolsInfo& symbols_info, int effort) {
    WP2_CHECK_STATUS(SymbolIO<SymbolWriterStatExtra>::Init(symbols_info));
    effort_ = effort;
    return WP2_STATUS_OK;
  }

  const SymbolsInfo& symbols_info() const override { return symbols_info_; }

  // Allocates memory. Should be called after calling SymbolIO::Init().
  WP2Status Allocate() override;

  // Deep copy. 'copied_dicts' must be a copy of 'original_dicts'.
  WP2Status CopyFrom(const SymbolWriter& other,
                     const ANSDictionaries& original_dicts,
                     const ANSDictionaries& copied_dicts);

  // For the 'cluster'th cluster of symbol 'sym', sets the writing mode to
  // trivial.
  void AddTrivial(uint32_t sym, uint32_t cluster, bool is_symmetric,
                  int32_t value);
  // For the 'cluster'th cluster of symbol 'sym', (given a maximum value of
  // non-zero values of 'max_nnz', which can be the number of pixels for
  // examples) of statistics 'counts' (first element being the count for the
  // minimum value of the symbol), decides how to store it
  // (trivial/range/dictionary) and writes the meta-information used to later be
  // able to decode the symbols.
  // If not nullptr, dicts are filled with symbol stats.
  // If not nullptr, storage_cost contains the cost of storing the symbol with
  // the best method chosen by WriteHeader.
  // TODO(vrabaud) Implement 'storage_cost' for Adaptive symbols.
  WP2Status WriteHeader(uint32_t sym, uint32_t cluster, uint32_t max_nnz,
                        const uint32_t* counts, WP2_OPT_LABEL, ANSEncBase* enc,
                        ANSDictionaries* dicts, float* storage_cost = nullptr);
  // Same as above but uses counts from the provided SymbolRecorder.
  WP2Status WriteHeader(uint32_t sym, uint32_t cluster, uint32_t max_nnz,
                        const SymbolRecorder& syntax_recorder, WP2_OPT_LABEL,
                        ANSEncBase* enc, ANSDictionaries* dicts,
                        float* storage_cost = nullptr);
  // Same as above but writes headers for all clusters.
  WP2Status WriteHeader(uint32_t sym, uint32_t max_nnz,
                        const SymbolRecorder& syntax_recorder, WP2_OPT_LABEL,
                        ANSEncBase* enc, ANSDictionaries* dicts);

  // Fills the array 'is_maybe_used' with true when the symbol might be used,
  // false if we are sure it is not used.
  void GetPotentialUsage(uint32_t sym, uint32_t cluster, bool is_maybe_used[],
                         uint32_t size) const override;

 protected:
  // For the 'cluster'th cluster of symbol 'sym', sets the writing mode to
  // a range.
  // 'mapping' is used in case we don't use the raw values but a mapping. The
  // mapping is the original value - min of symbol so that it is always
  // positive.
  void AddRange(uint32_t sym, uint32_t cluster, bool is_symmetric,
                const uint16_t* mapping, uint32_t size, uint16_t max_range);
  // For the 'cluster'th cluster of symbol 'sym', sets the writing mode to
  // a dictionary of statistics represented by 'counts', 'quantized_counts' and
  // 'mapping'. mapping[] must be already sorted in strictly increasing order.
  WP2Status AddDict(uint32_t sym, uint32_t cluster, bool is_symmetric,
                    const uint32_t* counts, const uint32_t* quantized_counts,
                    const uint16_t* mapping, uint32_t size,
                    ANSDictionaries* dicts);
  // For the 'cluster'th cluster of symbol 'sym', sets the writing mode to
  // kPrefixCode of prefix size 'prefix_size' using the statistics represented
  // by 'counts', 'quantized_counts' and 'mapping'.
  // mapping[] must be already sorted in strictly increasing order.
  WP2Status AddPrefixCode(uint32_t sym, uint32_t cluster, bool is_symmetric,
                          const uint32_t* counts,
                          const uint32_t* quantized_counts,
                          const uint16_t* mapping, uint32_t size,
                          uint32_t prefix_size, ANSDictionaries* dicts);

  // Initializes the 'stat' object to use mapping.
  static void SetMapping(Stat* stat, const uint16_t mapping[], uint32_t size);

 private:
  // Writes the symbol of type 'sym' in the 'cluster'th cluster. 'WriteHeader'
  // needs to have been called before so that the SymbolWriter knows how to
  // store 'sym'. If 'cost' is not nullptr, it's set to the cost in bits of
  // storing 'sym'.
  int32_t ProcessInternal(uint32_t sym, uint32_t cluster, int32_t value_in,
                          bool use_max_value, uint32_t max_value, WP2_OPT_LABEL,
                          ANSEncBase* enc, float* cost) override;

  // Writes a histogram as defined by the quantizer.
  void WriteHistogram(const Quantizer::Config& config, uint32_t symbol_range,
                      uint32_t max_count, ANSEncBase* enc);
  // Fills the cached counts+mapping for the elements in [0, 'size'], given a
  // prefix code size and the possible range of the elements.
  uint32_t FillCachedPrefixCodeHistogram(uint32_t range, uint32_t size,
                                         uint32_t prefix_size);
  // Computes the cost of storing a histogram with prefix code. Returns true if
  // we find a configuration that beats the quantizer_'s best cost and
  // 'cost_max', false otherwise.
  bool ComputeCachedPrefixCodeHistogramCost(uint32_t range, uint32_t max_nnz,
                                            uint32_t size, uint32_t prefix_size,
                                            float cost_max);
  // return the number of non-zero counts (stored in mapping_[])
  uint32_t ConvertCountsToCachedHistogram(const uint32_t counts[], int min,
                                          int max, bool is_symmetric,
                                          uint32_t counts_total[]);

  // Returns the largest index such that mapping[index] is valid and index <=
  // max_index. If no such value exists, returns the first valid index above
  // max_index.
  static uint32_t FindLargestMappingIndex(const Stat& stat, uint32_t max_index);

 private:
  // Tmp variables for updating the SymbolWriter.
  Vector_u32 histogram_;
  // Contains the values - min of symbol.
  Vector_u16 mapping_;
  Vector_u32 histogram_prefix_code_;
  Vector_u16 mapping_prefix_code_;
  // Quantizer for raw counts and prefix code counts.
  Quantizer quantizer_;
  VectorNoCtor<OptimizeArrayStorageStat> stats_buffer_;
  int effort_;
};

}  // namespace WP2

#endif /* WP2_ENC_SYMBOLS_ENC_H_ */
