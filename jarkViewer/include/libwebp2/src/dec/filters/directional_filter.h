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
// Intratile directional filter.
// Uses AV1's Constrained Directional Enhancement Filter.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_FILTERS_DIRECTIONAL_FILTER_H_
#define WP2_DEC_FILTERS_DIRECTIONAL_FILTER_H_

#include <cstdint>

#include "src/dec/filters/block_map_filter.h"
#include "src/utils/plane.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"

namespace WP2 {

//------------------------------------------------------------------------------

bool IsDirectionalFilterEnabled(const DecoderConfig& config,
                                const GlobalParams& gparams);

//------------------------------------------------------------------------------

class DirectionalFilter {
 public:
  static constexpr uint32_t kMaxPriStr = (1u << 4) - 1u;  // Constants defined
  static constexpr uint32_t kMaxSecStr = (1u << 2) - 1u;  // in AV1 spec.

  DirectionalFilter(const DecoderConfig& config, const GlobalParams& gparams,
                    const FilterBlockMap& blocks);

  // Allocates storage.
  WP2Status Allocate();

  // Performs the filtering up to 'num_rows'. Returns the total number of
  // filtered pixel rows (in YUV space).
  uint32_t Smooth(uint32_t num_rows);

 private:
  const DecoderConfig& config_;
  const GlobalParams& gparams_;
  const FilterBlockMap& blocks_;
  const bool enabled_;

  uint32_t num_filtered_rows_ = 0;  // Rows of pixels.

  YUVPlane top_taps_;       // Contains unfiltered pixels from the previous row.
  YUVPlane bottom_backup_;  // Will be used as top taps for the next row.
  YUVPlane left_taps_;
  YUVPlane right_backup_;

 private:
  // Visual debugging.
  YUVPlane unfiltered_pixels_;
  uint32_t num_debug_rows_ = 0;

  void SavePixelsForVDebug(uint32_t num_rows);
  void RegisterPixelsForVDebug(uint32_t from_x, uint32_t to_x, uint32_t from_y,
                               uint32_t to_y, uint32_t primary_strength,
                               uint32_t secondary_strength, uint32_t direction,
                               uint32_t variance);
  void ApplyVDebug(uint32_t num_rows);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_FILTERS_DIRECTIONAL_FILTER_H_
