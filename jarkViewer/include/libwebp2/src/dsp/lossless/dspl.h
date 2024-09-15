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
// Image transforms and color space conversion methods common to
// decoder/encoder.
//
// Authors: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_DSP_LOSSLESS_DSPL_H_
#define WP2_DSP_LOSSLESS_DSPL_H_

#include <algorithm>
#include <cstdint>

#include "src/dsp/math.h"
#include "src/utils/utils.h"
#include "src/wp2/format_constants.h"

namespace WP2L {

// There are two kinds of predictors, depending on their signature.
// The type 0 predictors are linear combinations of the surrounding pixels.
// The type 1 predictors are a clamped linear combination of the surrounding
// pixels.
static constexpr uint32_t kNumPredictorsNonClamped = 6u;
static constexpr uint32_t kNumPredictorsClamped = 2u;
static constexpr uint32_t kNumPredictorsAngle = 7u;
static constexpr uint32_t kNumPredictors =
    kNumPredictorsNonClamped + kNumPredictorsClamped + kNumPredictorsAngle;
static constexpr uint32_t kNumPredictorsBits =
    1 + WP2Log2Floor_k(kNumPredictors);
// Make sure we can store two predictors and the sub-mode on 16 bits.
static_assert(2 * kNumPredictorsBits <= 16,
              "Wrong number of bits for predictor storage");
enum class PredictorType { NonClamped, Clamped, Angle };
static constexpr PredictorType kPredictorType[] = {
    PredictorType::NonClamped, PredictorType::Angle,
    PredictorType::Angle,      PredictorType::Angle,
    PredictorType::Angle,      PredictorType::NonClamped,
    PredictorType::Angle,      PredictorType::NonClamped,
    PredictorType::Angle,      PredictorType::Angle,
    PredictorType::NonClamped, PredictorType::NonClamped,
    PredictorType::NonClamped, PredictorType::Clamped,
    PredictorType::Clamped};
STATIC_ASSERT_ARRAY_SIZE(kPredictorType, kNumPredictors);
// TODO(vrabaud) the order of predictors is chosen to not create regressions
//               (changing the order of predictors slightly changes the
//               compression)
static constexpr uint32_t kPredictorIndex[] = {0u, 6u, 2u, 0u, 4u, 1u, 5u, 2u,
                                               3u, 1u, 3u, 4u, 5u, 0u, 1u};
STATIC_ASSERT_ARRAY_SIZE(kPredictorIndex, kNumPredictors);

//------------------------------------------------------------------------------
// Color transforms.

typedef struct {
  int32_t green_to_red;
  int32_t green_to_blue;
  int32_t red_to_blue;
} Multipliers;

// Given a color_product (color_pred*color or sum of such), finalizes it by
// shifting it to return a proper delta.
static inline int32_t ColorTransformDelta(int32_t color_product) {
  // This assumes a sign extension of the shift which is the case in C++20 and
  // is easier to implement in SIMD. Doing the proper rounding (e.g. -48 is
  // rounded to -64, not -32) provides negligeable benefit.
  return WP2::RightShiftRound(color_product, kColorTransformBits);
}

//------------------------------------------------------------------------------
// Misc methods.

// Transforms a signed value into an unsigned index.
static inline uint16_t MakeIndex(int16_t val) {
  return (val >= 0) ? 2 * val : -2 * val - 1;
}
// Capped version of the above.
static inline uint16_t MakeIndex(int16_t val, uint16_t max_index) {
  return std::min(MakeIndex(val), max_index);
}

// Computes sampled size of 'size' when sampling using 'sampling bits'.
// Equivalent to: ceil(size / 2^sampling_bits)
static inline uint32_t SubSampleSize(uint32_t size, uint32_t sampling_bits) {
  return (size + (1 << sampling_bits) - 1) >> sampling_bits;
}

//------------------------------------------------------------------------------
// Predictors

// These Add/Sub function expects upper[-1] and out[-1] to be readable.
typedef void (*PredictorNonClampedFunc)(const int16_t* left, const int16_t* top,
                                        int16_t* out);
extern PredictorNonClampedFunc PredictorsNonClamped[kNumPredictorsNonClamped];
extern PredictorNonClampedFunc PredictorsNonClamped_C[kNumPredictorsNonClamped];

#define COPY_PREDICTOR_NON_CLAMPED_ARRAY(IN, OUT)                  \
  do {                                                             \
    (PredictorsNonClamped##OUT)[0] = PredictorNonClamped##IN##0_C; \
    (PredictorsNonClamped##OUT)[1] = PredictorNonClamped##IN##1_C; \
    (PredictorsNonClamped##OUT)[2] = PredictorNonClamped##IN##2_C; \
    (PredictorsNonClamped##OUT)[3] = PredictorNonClamped##IN##3_C; \
    (PredictorsNonClamped##OUT)[4] = PredictorNonClamped##IN##4_C; \
    (PredictorsNonClamped##OUT)[5] = PredictorNonClamped##IN##5_C; \
  } while (0);

typedef void (*PredictorClampedFunc)(const int16_t* left, const int16_t* top,
                                     const int16_t min_values[4],
                                     const int16_t max_values[4], int16_t* out);
extern PredictorClampedFunc PredictorsClamped[kNumPredictorsClamped];
extern PredictorClampedFunc PredictorsClamped_C[kNumPredictorsClamped];

#define COPY_PREDICTOR_CLAMPED_ARRAY(IN, OUT)                \
  do {                                                       \
    (PredictorsClamped##OUT)[0] = PredictorClamped##IN##0_C; \
    (PredictorsClamped##OUT)[1] = PredictorClamped##IN##1_C; \
  } while (0);

typedef void (*PredictorAngleFunc)(int delta, const int16_t* left,
                                   const int16_t* top, int16_t* out);
extern PredictorAngleFunc PredictorsAngle[kNumPredictorsAngle];
template <int MAIN_ANGLE>
void AnglePredict(int delta, const int16_t* left, const int16_t* top,
                  int16_t* out);

// Indices of the left/top predictors in PredictorsAngle.
static uint32_t constexpr kPredictorAngleLeftIndex = 6u;
static uint32_t constexpr kPredictorAngleTopIndex = 2u;

// Must be called before calling any of the above methods.
void DspInit();

}  // namespace WP2L

#endif  // WP2_DSP_LOSSLESS_DSPL_H_
