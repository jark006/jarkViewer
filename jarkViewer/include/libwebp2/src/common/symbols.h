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
// Common code dealing with symbols.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_COMMON_SYMBOLS_H_
#define WP2_COMMON_SYMBOLS_H_

#include <cassert>
#include <cstdint>
#include <limits>

#include "src/common/constants.h"
#include "src/common/lossy/block_size.h"
#include "src/dsp/math.h"
#include "src/utils/ans.h"
#include "src/utils/ans_enc.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

//------------------------------------------------------------------------------

namespace WP2 {

// Maximum number of symbol types to use.
static constexpr uint32_t kSymbolNumMax = 41;
// Number of residual clusters, not including sectors.
// Y/UV/A * Method0/Method1 * transform.
static constexpr uint32_t kResidualClusters = 3 * kNumResidualStats * TRF_LAST;

// Class containing info about the size (in bits) of the different symbols
// (colors, cache index and LZ77 distance/length).
// WARNING! This is a *big* object. Don't use on the stack!
class SymbolsInfo {
 public:
  // Bound to use when a symbol is unused.
  static constexpr int32_t kInvalidBound = std::numeric_limits<int32_t>::max();

  enum class StorageMethod {
    // Automatically choose between dictionary, range, prefix coding.
    kAuto,
    // Adaptive bit using counts of 0/1 so far for probabilities.
    kAdaptiveBit,
    // Adaptive symbol (that can be of size 2, but uses the adaptive symbol
    // method).
    kAdaptiveSym,
    // Adaptive symbol, with automatically chosen adaptation speed (signaled
    // in the header). Only allowed for symbols that occur at most once per
    // block and whose values fit in a uint8_t, because we keep all symbols
    // in memory which might quickly add up.
    kAdaptiveWithAutoSpeed,
    kUnused
  };

  SymbolsInfo() = default;
  SymbolsInfo(const SymbolsInfo& other) = delete;
  SymbolsInfo& operator=(const SymbolsInfo& info) = delete;

  // TODO(maryla): CopyFrom is fairly expensive (this is a big object) and I
  //               think we currently use it in some places where we don't
  //               actually need a copy.
  WP2Status CopyFrom(const SymbolsInfo& other);

  // 'use_splits' is true if block geometry is encoded through splits, false if
  // encoded through block sizes.
  WP2Status InitLossy(uint32_t num_segments, PartitionSet partition_set,
                      bool has_alpha, bool use_aom_coeffs, bool use_splits);

  // Whether a symbol is used at all.
  inline bool IsUnused(uint32_t sym) const {
    return (Method(sym) == StorageMethod::kUnused);
  }
  // Invalidate the use of a given symbol.
  void SetUnused(uint32_t sym, uint32_t num_clusters = 0);

  void SetInfo(uint32_t sym, int32_t min, int32_t max, uint32_t num_clusters,
               StorageMethod method, bool probably_geometric = false);

  // Sets symbol range for all clusters. Indicates that symbol values are in
  // [min; max]. For now, only min=0 or min=-max is supported.
  // Overwrites any values previously set through
  // SetMinMax(sym,min,max) or SetMinMax(cluster,min,max).
  // The convention min==max==kInvalidBound means the symbol will never be used.
  void SetMinMax(uint32_t sym, int32_t min, int32_t max);
  // Sets symbol range for a specific cluster. Overrides any value previously
  // set through SetMinMax(sym,min,max) for that cluster.
  WP2Status SetMinMax(uint32_t sym, uint32_t cluster, int32_t min, int32_t max);

  void SetClusters(uint32_t sym, uint32_t num_clusters);

  WP2Status SetStartingProba(uint32_t sym, uint32_t cluster, uint32_t p0,
                             uint32_t p1);

  uint32_t StartingProbaP0(uint32_t sym, uint32_t cluster) const {
    assert(Method(sym) == StorageMethod::kAdaptiveBit &&
           Range(sym, cluster) == 2);
    return GetClusterInfo(sym, cluster).p0;
  }

  uint32_t StartingProbaP1(uint32_t sym, uint32_t cluster) const {
    assert(Method(sym) == StorageMethod::kAdaptiveBit &&
           Range(sym, cluster) == 2);
    return GetClusterInfo(sym, cluster).p1;
  }

  void GetInitialCDF(uint32_t sym, uint32_t cluster, const uint16_t** cdf,
                     uint16_t* max_proba) const;
  WP2Status SetInitialCDF(uint32_t sym, uint32_t cluster, const uint16_t* cdf,
                          uint16_t max_proba);

  // Range of the symbol of bounds [min,max]: max-min+1.
  inline uint32_t Range(uint32_t sym, uint32_t cluster) const {
    const ClusterInfo& info_cluster = GetClusterInfo(sym, cluster);
    if (info_cluster.min != kInvalidBound &&
        info_cluster.max != kInvalidBound) {
      return info_cluster.max - info_cluster.min + 1;
    }
    const SymbolInfo& info_sym = GetSymbolInfo(sym);
    if (info_sym.min != kInvalidBound && info_sym.max != kInvalidBound) {
      return info_sym.max - info_sym.min + 1;
    }
    return 0u;
  }

  inline int32_t Min(uint32_t sym, uint32_t cluster) const {
    const ClusterInfo& info_cluster = GetClusterInfo(sym, cluster);
    if (info_cluster.min != kInvalidBound &&
        info_cluster.max != kInvalidBound) {
      return info_cluster.min;
    }
    return GetSymbolInfo(sym).min;
  }

  inline int32_t Max(uint32_t sym, uint32_t cluster) const {
    const ClusterInfo& info_cluster = GetClusterInfo(sym, cluster);
    if (info_cluster.min != kInvalidBound &&
        info_cluster.max != kInvalidBound) {
      return info_cluster.max;
    }
    return GetSymbolInfo(sym).max;
  }

  inline StorageMethod Method(uint32_t sym) const {
    return GetSymbolInfo(sym).storage_method;
  }

  inline bool IsAdaptiveMethod(uint32_t sym) const {
    const StorageMethod method = Method(sym);
    return (method == StorageMethod::kAdaptiveSym ||
            method == StorageMethod::kAdaptiveWithAutoSpeed);
  }

  // Whether values for this symbol are likely to follow a geometric
  // distribution. This information can be used when estimating the cost
  // of storing this symbol.
  inline bool ProbablyGeometric(const uint32_t sym) const {
    return GetSymbolInfo(sym).probably_geometric;
  }

  inline uint32_t NumClusters(const uint32_t sym) const {
    return GetSymbolInfo(sym).num_clusters;
  }

  // Gets the sum of the max ranges of all the symbols.
  // TODO(maryla): not sure this is needed, it seems it's usually called for
  // symbols that have 1 cluster, in which case it's equivalent to RangeSum().
  uint32_t MaxRangeSum() const;
  // Gets the sum of the ranges of all the symbols/clusters.
  uint32_t RangeSum() const;
  // Gets the maximum range among all the symbols and clusters.
  uint32_t GetMaxRange() const;
  // Gets the maximum range for a given symbol among all clusters.
  uint32_t GetMaxRange(uint32_t sym) const;

  // maximum number of symbols potentially used
  uint32_t Size() const { return num_symbols_; }

 protected:
  struct ClusterInfo {
    // Overrides SymbolInfo.min if min <= max.
    int min = kInvalidBound;
    int max = kInvalidBound;
    // Starting probabilities for StorageMethod::kAdaptiveBit.
    uint32_t p0 = 1, p1 = 1;
    // Pointer to a reference cdf to use.
    const uint16_t* cdf = nullptr;
    // Max proba in the CDF.
    uint16_t max_proba;
  };

  struct SymbolInfo {
    StorageMethod storage_method = StorageMethod::kAuto;
    int min = kInvalidBound;
    int max = kInvalidBound;
    // Index of start of per-cluster info in cluster_info_.
    int cluster_info_index = kNoPerClusterInfo;
    uint32_t num_clusters = 0;
    // Whether values are likely to follow a geometric distribution.
    bool probably_geometric = false;

    static constexpr int kNoPerClusterInfo = -1;

    bool MinMaxIsValid(int new_min, int new_max) const;
  };

  inline SymbolInfo* GetSymbolInfo(uint32_t sym) { return &symbols_info_[sym]; }

  inline const SymbolInfo& GetSymbolInfo(uint32_t sym) const {
    return symbols_info_[sym];
  }

  // Returns the ClusterInfo for the given cluster/sym, initializing the
  // per-cluster info storage if needed.
  ClusterInfo* GetOrMakeClusterInfo(uint32_t sym, uint32_t cluster);

  const ClusterInfo& GetClusterInfo(uint32_t sym, uint32_t cluster) const;

  static const ClusterInfo kDefaultClusterInfo;

 protected:
  uint32_t num_symbols_ = 0;
  SymbolInfo symbols_info_[kSymbolNumMax];

  Vector<ClusterInfo> cluster_info_;
  uint32_t next_cluster_info_index_ = 0;
};

//------------------------------------------------------------------------------

// Split the symbol 'v' into a 'prefix' (corresponding to the first
// 'prefix_size' bits), the number of extra bits, and the value contained in
// those extra bits. This prefix coding is used in deflate (cf rfc1951) and is
// close to an exponential Golomb.
struct PrefixCode {
  PrefixCode(uint32_t v, uint32_t prefix_size) {
    if (v < (2u << prefix_size)) {
      prefix = v;
      extra_bits_num = 0;
      extra_bits_value = 0;
      return;
    }
    const uint32_t highest_bit_index = WP2Log2Floor(v);
    extra_bits_num = highest_bit_index - prefix_size;
    const uint32_t other_highest_bits =
        (v - (1 << highest_bit_index)) >> extra_bits_num;
    // Store highest bit position and the next highest bits into an int.
    prefix = ((extra_bits_num + 1) << prefix_size) + other_highest_bits;
    extra_bits_value = v & ((1 << extra_bits_num) - 1);
  }
  // Given a 'prefix', returns the number of extra bits required to complete
  // it.
  static uint32_t NumExtraBits(uint32_t prefix, uint32_t prefix_size) {
    if (prefix < (2u << prefix_size)) return 0;
    return (prefix >> prefix_size) - 1;
  }
  // Given a 'prefix' and 'extra_bits_value', combine the two to reconstruct
  // the final value.
  static uint32_t Merge(uint32_t prefix, uint32_t prefix_size,
                        uint32_t extra_bits_value) {
    if (prefix < (2u << prefix_size)) return prefix;
    const uint32_t extra_bits_num = (prefix >> prefix_size) - 1;
    uint32_t offset = 1 << prefix_size;
    offset += prefix & ((1 << prefix_size) - 1);
    offset <<= extra_bits_num;
    return offset + extra_bits_value;
  }

  uint32_t prefix;
  uint32_t extra_bits_num;
  uint32_t extra_bits_value;
};

// Read/write a value in [min, max] using prefix coding.
// 'prefix_size' must be 1 or 2 (number of leading bits coded separately).
void WritePrefixCode(int32_t value, int32_t min, int32_t max,
                     uint32_t prefix_size, ANSEncBase* enc, WP2_OPT_LABEL);
int32_t ReadPrefixCode(int32_t min, int32_t max, uint32_t prefix_size,
                       ANSDec* dec, WP2_OPT_LABEL);

//------------------------------------------------------------------------------

// Class common to decoder to read/write statistics in an image.
// It is made common to have a similar behavior when analyzing tiles.
template <class EXTRA>
class SymbolIO {
 public:
  virtual ~SymbolIO() = default;

  // Allocates data after ranges/cluster sizes have been specified.
  virtual WP2Status Allocate() {
    const uint32_t size = symbols_info_.RangeSum();
    uint32_t size_contexts = 0;
    for (uint32_t sym = 0; sym < symbols_info_.Size(); ++sym) {
      size_contexts += symbols_info_.NumClusters(sym);
    }
    WP2_CHECK_ALLOC_OK(mappings_buffer_.resize(size));
    WP2_CHECK_ALLOC_OK(all_stats_.resize(size_contexts));
    // Prepare the mappings to point to the right spot in the buffer.
    auto* mapping_buffer = mappings_buffer_.data();
    for (uint32_t sym = 0, ind = 0; sym < symbols_info_.Size(); ++sym) {
      stats_start_[sym] = &all_stats_[ind];
      for (uint32_t c = 0; c < symbols_info_.NumClusters(sym); ++c) {
        all_stats_[ind].type = Stat::Type::kUnknown;
        all_stats_[ind].use_mapping = false;
        all_stats_[ind].range = 0;
        all_stats_[ind].mappings = mapping_buffer;
        mapping_buffer += symbols_info_.Range(sym, c);
        ++ind;
      }
    }
    return WP2_STATUS_OK;
  }
  // Fills the array 'is_maybe_used' with true when the symbol might be used,
  // false if we are sure it is not used.
  virtual void GetPotentialUsage(uint32_t sym, uint32_t cluster,
                                 bool is_maybe_used[], uint32_t size) const = 0;

 protected:
  // The stats of a symbol.
  struct Stat {
    static const uint16_t kInvalidMapping;
    enum class Type {
      // We have no information on this symbol.
      kUnknown,
      // Symbol always has the same value or is unused.
      kTrivial,
      // Symbol is stored as a range (all values assumed to have equal
      // probability).
      kRange,
      // Symbol is stored as a dictionary.
      kDict,
      // Symbol is stored with a prefix code.
      kPrefixCode,
      // Symbol is stored as an adaptive bit.
      kAdaptiveBit,
      // Symbol is stored as an adaptive symbol.
      kAdaptiveSymbol,
    };
    Type type;
    // Whether to use 'mappings' to map raw stored values to symbol values.
    bool use_mapping;
    uint16_t* mappings;
    // If set to true, it assumes values are stored as abs(value) + sign with
    // abs(value) stored as range/dict/prefix code/... Otherwise, value - min is
    // the one stored as range/dict/prefix code/...
    bool is_symmetric;
    // For type kRange, the symbol is stored as a value in [0;range[
    uint16_t range;
    union Param {
      // For type kPrefixCode.
      struct {
        // Range of the prefix.
        uint16_t range;
        // Size in bits of the prefix code.
        uint8_t prefix_size;
      } prefix_code;
      // For type kTrivial, there is only one value. If the range contains
      // negative and positive values, the sign is stored apart.
      uint16_t trivial_value;
      // For type kAdaptive where range is 2.
      // Index of this adaptive bit a_bits_.
      uint32_t a_bit_index;
      // For type kAdaptive where range is != 2.
      // Index of the adaptive symbol in a_symbols_.
      uint32_t a_symbol_index;
    };
    Param param;
    EXTRA extra;
  };
  // Sets basic information about symbols.
  WP2Status Init(const SymbolsInfo& symbols_info) {
    WP2_CHECK_STATUS(symbols_info_.CopyFrom(symbols_info));
    return WP2_STATUS_OK;
  }

  // Gets the statistics for symbol 'sym' in cluster 'cluster'.
  inline Stat* GetStats(uint32_t sym, uint32_t cluster) {
    assert(sym < symbols_info_.Size() &&
           cluster < symbols_info_.NumClusters(sym));
    return &stats_start_[sym][cluster];
  }
  inline const Stat* GetStats(uint32_t sym, uint32_t cluster) const {
    assert(sym < symbols_info_.Size() &&
           cluster < symbols_info_.NumClusters(sym));
    return &stats_start_[sym][cluster];
  }

  // Helper function to set the stats of symbol 'sym' at 'cluster' to be proper
  // prefix code.
  void SetPrefixCodeStat(uint32_t sym, uint32_t cluster, uint32_t prefix_size) {
    Stat* const stat = GetStats(sym, cluster);
    stat->type = Stat::Type::kPrefixCode;
    stat->range = symbols_info_.Range(sym, cluster);
    const PrefixCode prefix_code = PrefixCode(stat->range - 1, prefix_size);
    stat->param.prefix_code.range = prefix_code.prefix + 1;
    stat->param.prefix_code.prefix_size = prefix_size;
  }

  // Sets up the given symbol as an adaptive bit.
  WP2Status AddAdaptiveBit(uint32_t sym, uint32_t cluster, uint32_t p0 = 1,
                           uint32_t p1 = 1) {
    Stat* const stat = GetStats(sym, cluster);
    stat->type = Stat::Type::kAdaptiveBit;
    stat->is_symmetric = false;
    stat->use_mapping = false;
    stat->range = 2;
    stat->param.a_bit_index = a_bits_.size();
    WP2_CHECK_ALLOC_OK(a_bits_.push_back(ANSBinSymbol(p0, p1)));
    return WP2_STATUS_OK;
  }

  // Sets up the given symbol as an adaptive symbol.
  WP2Status AddAdaptiveSymbol(uint32_t sym, uint32_t cluster,
                              ANSAdaptiveSymbol::Method method,
                              uint32_t speed) {
    Stat* const stat = GetStats(sym, cluster);
    stat->type = Stat::Type::kAdaptiveSymbol;
    stat->is_symmetric = false;
    stat->use_mapping = false;
    const uint32_t range = symbols_info_.Range(sym, cluster);
    stat->range = range;
    stat->param.a_symbol_index = a_symbols_.size();
    const uint16_t* cdf;
    uint16_t max_proba;
    symbols_info_.GetInitialCDF(sym, cluster, &cdf, &max_proba);
    WP2_CHECK_STATUS(a_symbols_.Add(range, cdf, max_proba));
    auto& asym = a_symbols_[stat->param.a_symbol_index];
    asym.SetAdaptationSpeed(method, speed);
    return WP2_STATUS_OK;
  }

  SymbolsInfo symbols_info_;

  ANSAdaptiveBits a_bits_;
  ANSAdaptiveSymbols a_symbols_;

  // For each symbol 'sym', the statistics for all clusters start at
  // stats_start_[sym].
  Stat* stats_start_[kSymbolNumMax];
  VectorNoCtor<Stat> all_stats_;
  Vector_u16 mappings_buffer_;
};

template <class T>
const uint16_t SymbolIO<T>::Stat::kInvalidMapping =
    std::numeric_limits<uint16_t>::max();

}  // namespace WP2

//------------------------------------------------------------------------------
// Info about the symbols.

namespace WP2 {

enum Symbol {
  kSymbolModeY = 0,  // prediction modes for Y
  kSymbolModeUV,     // prediction modes for U/V
  kSymbolModeA,      // prediction mode for A
  kSymbolSegmentId,
  kSymbolRndMtx0,
  // Symbol for the block transform (same in X and Y).
  kSymbolTransform,
  // Symbol for whether the block contains several transforms or not.
  kSymbolSplitTransform,
  // Symbol for BlockAlphaMode for each block.
  kSymbolBlockAlphaMode,
  // Symbol for whether the block uses 420 (downscaled UV).
  kSymbolYuv420,
  // Symbol for whether the block uses random matrices.
  kSymbolUseRandomMatrix,
  // Symbol for whether at least one coeff is not zero.
  kSymbolHasCoeffs,
  // Symbol for the residual encoding method for Y, U and V.
  kSymbolEncodingMethod,
  // Symbol for the residual encoding method for A.
  kSymbolEncodingMethodA,
  // Slope ('a' in 'chroma = a * luma + b') for chroma-from-luma prediction.
  kSymbolCflSlope,
  // Block size or block split. One cluster per bounding BlockSize.
  kSymbolBlockSize,

  // ------------------
  // Residual encoding
  // ------------------
  // Number of zero residuals before the enc of blocK
  kSymbolResNumZeros,
  // Symbols for small residuals.
  kSymbolBits0,
  // Symbols for the prefix of big residuals.
  kSymbolBits1,
  // Symbols for DC residuals.
  kSymbolDC,
  // Symbol for the prefix of DC coefficients.
  kSymbolDCDict,

  // Whether we use 1 or 2 boundaries for the coefficients.
  kSymbolResidualUseBounds,
  // Whether the first used boundary is X.
  kSymbolResidualBound1IsX,
  // Whether we store the second boundary for the coefficients.
  kSymbolResidualUseBound2,
  // Whether the residual value is 0.
  kSymbolResidualIsZero,
  // Whether the residual value is 1.
  kSymbolResidualIsOne,
  // Whether the residual value is 2.
  kSymbolResidualIsTwo,
  // Whether all residuals after this one are 0.
  kSymbolResidualEndOfBlock,
  // Whether all residuals after this one are 1 or -1.
  kSymbolResidualHasOnlyOnesLeft,

  // ---------------------
  // AOM residual encoding
  // ---------------------
  kAOMEOBPT4,
  kAOMEOBPT8,
  kAOMEOBPT16,
  kAOMEOBPT32,
  kAOMEOBPT64,
  kAOMEOBPT128,
  kAOMEOBPT256,
  kAOMEOBPT512,
  kAOMEOBPT1024,
  kAOMEOBExtra,
  kAOMCoeffBaseEOB,
  kAOMCoeffBase,
  kAOMCoeffBaseRange,
  kSymbolNum
};

static_assert(kSymbolNum <= kSymbolNumMax, "kSymbolNumMax too small");

constexpr Symbol kSymbolsForCoeffs[] = {
    kSymbolResidualIsZero, kSymbolResidualIsOne, kSymbolResidualIsTwo,
    kSymbolResidualEndOfBlock, kSymbolResidualHasOnlyOnesLeft};
constexpr Symbol kSymbolsForCoeffsPerTf[] = {
    kSymbolDC, kSymbolResidualUseBounds, kSymbolResidualBound1IsX,
    kSymbolResidualUseBound2};
constexpr Symbol kSymbolsForAOMCoeffs[] = {
    kAOMEOBPT4,        kAOMEOBPT8,   kAOMEOBPT16,      kAOMEOBPT32,
    kAOMEOBPT64,       kAOMEOBPT128, kAOMEOBPT256,     kAOMEOBPT512,
    kAOMEOBPT1024,     kAOMEOBExtra, kAOMCoeffBaseEOB, kAOMCoeffBase,
    kAOMCoeffBaseRange};

enum SymbolCount {
  kSymbolCountZero = 0,
  kSymbolCountOne,
  kSymbolCountMoreThanOne,
  kSymbolCountLast
};

enum AlphaMode { kAlphaModeLossless = 0, kAlphaModeLossy, kAlphaModeNum };

enum BlockAlphaMode {
  kBlockAlphaFullTransp,  // All pixels have an alpha of 0.
  kBlockAlphaFullOpaque,  // All pixels have an alpha of kAlphaMax.
  kBlockAlphaLossy,       // Block alpha is encoded lossily.
  kBlockAlphaLossless,    // Block alpha is encoded losslessly.
};

enum class ChromaSubsampling {
  k420,
  k444,
  kAdaptive,     // Can differ from one block to the next.
  kSingleBlock,  // Do not signal anything before decoding the only block.
};

}  // namespace WP2

namespace WP2L {

enum Symbol {
  kSymbolType = 0,  // one of the SymbolType
  kSymbolA,
  kSymbolR,
  kSymbolG,
  kSymbolB,
  kSymbolColorCache,
  kSymbolSegmentCache,
  kSymbolDist,
  kSymbolLen,
  kSymbolSubMode,
  kSymbolNum
};
static const char* const kSymbolNames[] = {
    "type",          "a",    "r",   "g",       "b", "color_cache",
    "segment_cache", "dist", "len", "sub_mode"};
STATIC_ASSERT_ARRAY_SIZE(kSymbolNames, kSymbolNum);

// Group 4 symbols.
enum SymbolGroup4 {
  kSymbolG4Type,
  kSymbolG4HorizontalDist,
  kSymbolG4VerticalDist,
  // Boolean to signal if the next color is different from the expected one
  // (as deduced from previous row).
  kSymbolG4ColorChange,
  // Index of the new color if kSymbolG4ColorChange was true.
  kSymbolG4NewColor,

  kSymbolG4Num
};
static const char* const kSymbolGroup4Names[] = {"type", "hdist", "vdist",
                                                 "color_change", "new_color"};
STATIC_ASSERT_ARRAY_SIZE(kSymbolGroup4Names, kSymbolG4Num);

// Initializes a SymbolsInfo to be used with Group 4 algorithm.
void InitGroup4(uint32_t width, uint32_t num_colors, WP2::SymbolsInfo* info);

static_assert(kSymbolNum <= WP2::kSymbolNumMax, "kSymbolNumMax too small");

enum SymbolType {
  kSymbolTypeLiteral,          // literal value
  kSymbolTypeCopy,             // copy from (distance, length)
  kSymbolTypeColorCacheIdx,    // index in the color cache
  kSymbolTypeSegmentCacheIdx,  // index in the segment cache
  kSymbolTypeNum,
  kSymbolTypeDiscarded  // discarded symbol due to segment cache
};

// Returns whether a symbol will be potentially used, depending on type.
bool IsSymbolUsed(Symbol sym, const bool is_maybe_used[kSymbolTypeNum]);

// Class containing info about the size (in bits) of the different symbols
// (colors, cache index and LZ77 distance/length).
class LosslessSymbolsInfo : public WP2::SymbolsInfo {
 public:
  LosslessSymbolsInfo();
  // By default, there is only one cluster per symbol and the cache bits are set
  // to 0.
  void Init(uint32_t num_pixels, bool has_alpha, WP2SampleFormat format);
  // Set the symbols A,R,B as useless as all the information will be stored in
  // G channel.
  void InitAsLabelImage(uint32_t num_pixels, uint32_t num_labels);
  // Set the symbol A as useless and R,G,B as ranging with the cross color image
  // bounds kRedToBlueMax, kGreenToBlueMax, kGreenToRedMax
  void InitAsCrossColorImage(uint32_t num_pixels);
  // Initialize to store predictors.
  void InitAsPredictorImage(uint32_t num_pixels);
  // Initialize kSymbolSubMode.
  WP2Status InitSubMode();

  virtual ~LosslessSymbolsInfo() = default;

  WP2Status CopyFrom(const LosslessSymbolsInfo& other);

  // Changes the number of clusters for all symbols.
  void SetNumClusters(uint32_t num_clusters);

  // Sets the range of the color cache and segment cache (if used).
  void SetCacheRange(uint32_t cache_range, bool has_segment_cache);

  WP2SampleFormat SampleFormat() const { return sample_format_; }

  // Returns whether alpha is used and whether it is a label image (ARB unused).
  static bool HasAlpha(const SymbolsInfo& info);
  static bool IsLabelImage(const SymbolsInfo& info);

 private:
  void InitNonARGB(uint32_t num_pixels);

  WP2SampleFormat sample_format_;
  uint32_t num_pixels_;
};

// Fills ranges of ARGB symbols according to the transforms applied to the
// image. 'num_colors' is the number of colors used in the palette if any (it is
// unused if no color indexing transform is present).
// If 'transforms' is not one of the official sets of transforms you can specify
// its length with 'num_transforms'.
void GetARGBRanges(const TransformType* transforms, WP2SampleFormat format,
                   uint32_t num_colors, int16_t minima_range[4],
                   int16_t maxima_range[4],
                   uint32_t num_transforms = kPossibleTransformCombinationSize);

}  // namespace WP2L

//------------------------------------------------------------------------------

#endif  // WP2_COMMON_SYMBOLS_H_
