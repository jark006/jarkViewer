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
// Image transforms and color space conversion methods for lossless decoder.
//
// Authors: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_DSP_LOSSLESS_DECL_DSP_H_
#define WP2_DSP_LOSSLESS_DECL_DSP_H_

#include <cstdint>

#include "src/dsp/lossless/dspl.h"

namespace WP2L {

//------------------------------------------------------------------------------
// Decoding

typedef void (*PredictorNonClampedAddFunc)(const int16_t* in, bool has_alpha,
                                           const int16_t* upper,
                                           uint32_t num_pixels, int16_t* out,
                                           int16_t* predicted);
extern PredictorNonClampedAddFunc
    PredictorsNonClampedAdd[kNumPredictorsNonClamped];

typedef void (*PredictorClampedAddFunc)(const int16_t* in, bool has_alpha,
                                        const int16_t* upper,
                                        uint32_t num_pixels,
                                        const int16_t min_value[4],
                                        const int16_t max_values[4],
                                        int16_t* out, int16_t* predicted);
extern PredictorClampedAddFunc PredictorsClampedAdd[kNumPredictorsClamped];

// 'delta' is an integer in [-3, 3].
typedef void (*PredictorAngleAddFunc)(int delta, const int16_t* in,
                                      bool has_alpha, const int16_t* upper,
                                      uint32_t num_pixels, int16_t* out,
                                      int16_t* predicted);
extern PredictorAngleAddFunc PredictorsAngleAdd[kNumPredictorsAngle];

typedef void (*SimpleColorSpaceInverseFunc)(const int16_t* src,
                                            uint32_t num_pixels, int16_t* dst);
extern SimpleColorSpaceInverseFunc AddGreenToBlueAndRed;
extern SimpleColorSpaceInverseFunc YCoCgRToRGB;

typedef void (*TransformColorInverseFunc)(const Multipliers& m,
                                          const int16_t* src,
                                          uint32_t num_pixels, int16_t* dst,
                                          int16_t* predicted);
extern TransformColorInverseFunc TransformColorInverse;

class Transform;  // Defined in dec/lossless/losslessi_dec.h.

// Performs inverse transform of data given transform information, start and end
// rows. Transform will be applied to rows [row_start, row_end[.
// The *in and *out pointers refer to source and destination data respectively
// corresponding to the intermediate row (row_start).
// If 'predicted' is not null, it will contain the predicted samples on return.
void InverseTransform(const Transform* transform, uint32_t row_start,
                      uint32_t row_end, const int16_t min_values[4],
                      const int16_t max_values[4], const int16_t* in,
                      bool has_alpha, int16_t* out,
                      int16_t* predicted = nullptr);

// Fills 'dst' with the samples from the 'color_map' at the indices specified by
// the green channel of the pixels in 'src'.
typedef void (*MapARGBFunc)(const int16_t* src, const int16_t* color_map,
                            uint32_t color_map_size, uint32_t y_start,
                            uint32_t y_end, uint32_t width, int16_t* dst);

extern MapARGBFunc MapColor;

//------------------------------------------------------------------------------

// Must be called before calling any of the above methods.
void DecLDspInit();

}  // namespace WP2L

#endif  // WP2_DSP_LOSSLESS_DECL_DSP_H_
