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
//   Common code for AOM residuals, branched from libgav1, with as little
//   changes as possible.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_COMMON_LOSSY_RESIDUALS_AOM_H_
#define WP2_COMMON_LOSSY_RESIDUALS_AOM_H_

#include <array>
#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/residuals.h"
#include "src/common/lossy/transforms.h"
#include "src/common/symbols.h"
#include "src/wp2/format_constants.h"

namespace WP2 {
namespace libgav1 {

//------------------------------------------------------------------------------
// From libgav1/src/dsp/constants.h

constexpr uint32_t kLevelBufferPadding = 4;
constexpr uint32_t kMaxTxBufferDim = kMaxBlockSizePix + kLevelBufferPadding;

// The plane types, called luma and chroma in the spec.
// Matches PlaneType in cdfs.inc
// TODO(maryla): should there be one separate for alpha?
enum PlaneType : uint8_t { kPlaneTypeYA, kPlaneTypeUV, kNumPlaneTypes };

constexpr PlaneType GetPlaneType(Channel channel) {
  return (channel == kUChannel || channel == kVChannel) ? kPlaneTypeUV
                                                        : kPlaneTypeYA;
}

//------------------------------------------------------------------------------
// From libgav1/src/utils/common.h

constexpr int DivideBy2(int n) { return n >> 1; }
constexpr int DivideBy4(int n) { return n >> 2; }
constexpr int DivideBy8(int n) { return n >> 3; }
constexpr int MultiplyBy4(int n) { return (4 * n); }

// Input: 1d array index |index|, which indexes into a 2d array of width
//     1 << |tx_width_log2|.
// Output: 1d array index which indexes into a 2d array of width
//     (1 << |tx_width_log2|) + kQuantizedCoefficientBufferPadding.
inline int PaddedIndex(int index, int tx_width_log2) {
  return index + MultiplyBy4(index >> tx_width_log2);
}

//------------------------------------------------------------------------------
// From libgav1/src/symbol_decoder_context.h

enum {
  kBooleanFieldCdfSize = 3,
  kNumSquareTransformSizes = 5,
  kAllZeroContexts = 13,
  kEobPtContexts = 2,
  kEobPt4SymbolCount = 3,
  kEobPt8SymbolCount = 4,
  kEobPt16SymbolCount = 5,
  kEobPt32SymbolCount = 6,
  kEobPt64SymbolCount = 7,
  kEobPt128SymbolCount = 8,
  kEobPt256SymbolCount = 9,
  kEobPt512SymbolCount = 10,
  kEobPt1024SymbolCount = 11,
  kEobExtraContexts = 9,
  kCoeffBaseEobContexts = 4,
  kCoeffBaseEobSymbolCount = 3,
  kCoeffBaseContexts = 42,
  kCoeffBaseSymbolCount = 4,
  kCoeffBaseRangeContexts = 21,
  kCoeffBaseRangeSymbolCount = 3
};  // anonymous enum

typedef uint32_t (*CoeffBaseContextFunc)(const uint8_t qlevels[], TrfSize tdim,
                                         uint32_t tx_width_log2, uint32_t pos);
typedef uint32_t (*CoeffBaseRangeContextFunc)(const uint8_t qlevels[],
                                              uint32_t tx_width_log2,
                                              uint32_t pos);

// Class specialized in reading transform residuals.
class AOMResidualIO {
 public:
  // Returns the cluster for a given symbol, given certain other parameters.
  // The overload GetClusterAOM1 and GetClusterAOM2 are meant
  // for symbols using 1 or 2 arguments.
  template <Symbol sym>
  static uint32_t GetClusterAOM1(uint32_t arg0) {
    static_assert(sym == kAOMEOBPT512 || sym == kAOMEOBPT1024, "wrong symbol");
    return arg0;
  }
  template <Symbol sym>
  static uint32_t GetClusterAOM2(uint32_t arg0, uint32_t arg1) {
    static_assert(sym == kAOMEOBPT4 || sym == kAOMEOBPT8 ||
                      sym == kAOMEOBPT16 || sym == kAOMEOBPT32 ||
                      sym == kAOMEOBPT64 || sym == kAOMEOBPT128 ||
                      sym == kAOMEOBPT256 || sym == kAOMCoeffBase ||
                      sym == kAOMCoeffBaseRange || sym == kAOMEOBExtra ||
                      sym == kAOMCoeffBaseEOB,
                  "wrong symbol");
    const std::array<uint32_t, 2> extent =
        (sym == kAOMEOBExtra)
            ? std::array<uint32_t, 2>{kNumPlaneTypes, kEobExtraContexts}
        : (sym == kAOMCoeffBaseEOB)
            ? std::array<uint32_t, 2>{kNumPlaneTypes, kCoeffBaseEobContexts}
        : (sym == kAOMCoeffBase)
            ? std::array<uint32_t, 2>{kNumPlaneTypes, kCoeffBaseContexts}
        : (sym == kAOMCoeffBaseRange)
            ? std::array<uint32_t, 2>{kNumPlaneTypes, kCoeffBaseRangeContexts}
            : std::array<uint32_t, 2>{kNumPlaneTypes, kEobPtContexts};
    uint32_t cluster = arg1;
    uint32_t stride = extent[1];
    cluster += arg0 * stride;
    return cluster;
  }

 public:
  // Lookup used to call the right variant of GetCoeffBase*() based on
  // the transform class.
  static CoeffBaseContextFunc GetCoeffsBaseContextFunc(TransformClass tx_class);
  static CoeffBaseRangeContextFunc GetCoeffBaseRangeContextFunc(
      TransformClass tx_class);
  static void UpdateLevels(int32_t res, uint32_t pos, TrfSize tdim,
                           uint8_t* levels);

 protected:
  static uint32_t GetCoeffBaseContextEob(TrfSize tdim, uint32_t index);
  static uint32_t GetCoeffBaseRangeContextEob(uint32_t tx_width_log2,
                                              uint32_t pos);
};

//------------------------------------------------------------------------------
// From libgav1/src/tile/tile.cc

// Range above kNumQuantizerBaseLevels which the exponential prefix coding
// process is activated.
constexpr uint32_t kNumQuantizerBaseLevels = 3u;  // (inclusive) limit
constexpr uint32_t kQuantizerCoefficientBaseRange = 12u;
constexpr uint32_t kQuantizerCoefficientBaseRangeContextClamp =
    kQuantizerCoefficientBaseRange + kNumQuantizerBaseLevels;
constexpr uint32_t kCoeffBaseRangeMaxIterations =
    kQuantizerCoefficientBaseRange / kCoeffBaseRangeSymbolCount;

}  // namespace libgav1
}  // namespace WP2

#endif /* WP2_COMMON_LOSSY_RESIDUALS_AOM_H_ */
