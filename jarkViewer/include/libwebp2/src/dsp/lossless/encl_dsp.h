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
// Image transforms and color space conversion methods for lossless encoder.
//
// Authors: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_DSP_LOSSLESS_ENCL_DSP_H_
#define WP2_DSP_LOSSLESS_ENCL_DSP_H_

#include <cstdint>

#include "src/dsp/lossless/dspl.h"
#include "src/wp2/format_constants.h"

namespace WP2L {

//------------------------------------------------------------------------------
// Predictors

// Difference of each component, mod 256.
static inline void SubPixels(const int16_t* const a, bool has_alpha,
                             const int16_t* const b, int16_t* const out) {
  out[0] = has_alpha ? a[0] - b[0] : WP2::kAlphaMax;
  for (uint32_t i = 1; i < 4; ++i) out[i] = a[i] - b[i];
}

//------------------------------------------------------------------------------
// Encoding

typedef void (*SimpleColorSpaceFunc)(int16_t* dst, uint32_t num_pixels);
extern SimpleColorSpaceFunc SubtractGreenFromBlueAndRed;
extern SimpleColorSpaceFunc RGB2YCoCgR;
typedef void (*TransformColorFunc)(const Multipliers& m, uint32_t num_pixels,
                                   int16_t* const data);
extern TransformColorFunc TransformColor;

// Functions collecting stats about blue and red transforms. If the transform
// produces a value bigger (in abs()) than abs_value_max, returns false (meaning
// the transform is not valid), true otherwise.
typedef bool (*CollectColorBlueTransformsFunc)(
    const int16_t* argb, uint32_t width, uint32_t tile_width,
    uint32_t tile_height, int16_t green_to_blue, int16_t red_to_blue,
    uint16_t abs_value_max, uint32_t histo_size, uint32_t* const histo);
extern CollectColorBlueTransformsFunc CollectColorBlueTransforms;

typedef bool (*CollectColorRedTransformsFunc)(
    const int16_t* argb, uint32_t width, uint32_t tile_width,
    uint32_t tile_height, int16_t green_to_red, uint16_t abs_value_max,
    uint32_t histo_size, uint32_t* const histo);
extern CollectColorRedTransformsFunc CollectColorRedTransforms;

// Predictors.
typedef void (*PredictorNonClampedSubFunc)(const int16_t* in, bool has_alpha,
                                           const int16_t* upper,
                                           uint32_t num_pixels, int16_t* out);
extern PredictorNonClampedSubFunc
    PredictorsNonClampedSub[kNumPredictorsNonClamped];
extern PredictorNonClampedSubFunc
    PredictorsNonClampedSub_C[kNumPredictorsNonClamped];

typedef void (*PredictorClampedSubFunc)(const int16_t* in, bool has_alpha,
                                        const int16_t* upper,
                                        uint32_t num_pixels,
                                        const int16_t min_value[4],
                                        const int16_t max_values[4],
                                        int16_t* out);
extern PredictorClampedSubFunc PredictorsClampedSub[kNumPredictorsClamped];
extern PredictorClampedSubFunc PredictorsClampedSub_C[kNumPredictorsClamped];

typedef void (*PredictorAngleSubFunc)(int delta, const int16_t* in,
                                      bool has_alpha, const int16_t* upper,
                                      uint32_t num_pixels, int16_t* out);
extern PredictorAngleSubFunc PredictorsAngleSub[kNumPredictorsAngle];

extern PredictorNonClampedSubFunc PredictorSubBlack;
extern PredictorAngleSubFunc PredictorSubTop;
extern PredictorAngleSubFunc PredictorSubLeft;

//------------------------------------------------------------------------------
// Entropy-cost related functions.

typedef void (*BufferAddFunc)(const uint32_t* const a, const uint32_t* const b,
                              uint32_t size, uint32_t* const out,
                              uint32_t* const nonzeros);
extern BufferAddFunc BufferAdd;

typedef float (*CostFunc)(const uint32_t* population, int length);
typedef float (*CostCombinedFunc)(const uint32_t* X, const uint32_t* Y,
                                  int length);
typedef float (*CombinedShannonEntropyFunc)(const uint32_t* X,
                                            const uint32_t* Y, uint32_t length);

extern CostFunc ExtraCost;
extern CostCombinedFunc ExtraCostCombined;
extern CombinedShannonEntropyFunc CombinedShannonEntropy;

//------------------------------------------------------------------------------

// Must be called before calling any of the above methods.
void EncLDspInit();

//------------------------------------------------------------------------------

}  // namespace WP2L

#endif  // WP2_DSP_LOSSLESS_ENCL_DSP_H_
