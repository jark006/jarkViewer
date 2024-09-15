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
// Restoration filter parameters.
// Optimized during encoding and read from the bitstream during decoding.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_COMMON_FILTERS_RSTR_FLT_PARAMS_H_
#define WP2_COMMON_FILTERS_RSTR_FLT_PARAMS_H_

#include <cstdint>

#include "src/dsp/dsp.h"
#include "src/dsp/math.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

static constexpr uint32_t kWieMaxNumAreas =
    DivCeil(kMaxTileSize, kWieFltWidth) * DivCeil(kMaxTileSize, kWieFltHeight);

//------------------------------------------------------------------------------

struct RstrFltParams {
 public:
  RstrFltParams(uint32_t width, uint32_t height);

  void SetAll(const int8_t values[kWieFltTapDist]);

 public:
  // Number of areas where the restoration filter can be applied within a tile.
  const uint32_t num_areas;

  // Stores the first three clamped Wiener tap weights per channel (Y, U, V),
  // per area (horizontal stripe), per direction (horizontal, vertical).
  int8_t half_tap_weights[3][kWieMaxNumAreas][2][kWieFltTapDist];
};

//------------------------------------------------------------------------------

inline int32_t GetMinWienerTapWeight(uint32_t index) {
  return LeftShift(-1, kWieNumBitsPerTapWgt[index] - 1);
}
inline int32_t GetMaxWienerTapWeight(uint32_t index) {
  return LeftShift(1, kWieNumBitsPerTapWgt[index] - 1) - 1;
}

// Makes sure that fewer bits than 'kWieNumBitsPerTapWgt' are used and that the
// sum of the absolutes of the full tap weights is using at most
// 'kWieFltNumBitsTapWgts + kWieFltNumBitsOverflow'.
bool WienerVerifyHalfTapWeights(const int8_t half_tap_weights[kWieFltTapDist]);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_FILTERS_RSTR_FLT_PARAMS_OPTIMIZER_H_
