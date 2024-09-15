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
// Intratile restoration filter. Uses AV1's Wiener filter.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_FILTERS_RESTORATION_FILTER_H_
#define WP2_DEC_FILTERS_RESTORATION_FILTER_H_

#include <cstdint>

#include "src/common/filters/rstr_flt_params.h"
#include "src/dec/filters/block_map_filter.h"
#include "src/utils/ans.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"

namespace WP2 {

//------------------------------------------------------------------------------

bool IsRestorationFilterEnabled(const DecoderConfig& config);

//------------------------------------------------------------------------------

class RestorationFilter {
 public:
  RestorationFilter(const DecoderConfig& config, const FilterBlockMap& blocks,
                    const RstrFltParams& params);

  // Allocates storage.
  WP2Status Allocate();

  // Performs the filtering up to 'num_rows'. Returns the total number of
  // filtered pixel rows (in YUV space).
  uint32_t Enhances(uint32_t num_rows);

 private:
  const DecoderConfig& config_;
  const FilterBlockMap& blocks_;
  const bool enabled_;

  uint32_t num_filtered_rows_ = 0;  // Rows of pixels.

  YUVPlane top_taps_;       // Contains unfiltered pixels from the previous row.
  YUVPlane bottom_backup_;  // Will be used as top taps for the next row.
  Vector_u8 filter_strength_map_;  // Filter strength per pixel.

  const RstrFltParams& params_;  // Extracted from the bitstream.

 private:
  // Visual debugging.
  YUVPlane unfiltered_pixels_;
  uint32_t num_debug_rows_ = 0;

  void SavePixelsForVDebug(uint32_t num_rows);
  void RegisterPixelsForVDebug(uint32_t from_x, uint32_t to_x, uint32_t from_y,
                               uint32_t to_y);
  void ApplyVDebug(uint32_t num_rows);
};

//------------------------------------------------------------------------------

// Reads filter parameters from the bitstream.
WP2Status ReadRestorationFilterParams(ANSDec* dec, RstrFltParams* params);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_FILTERS_RESTORATION_FILTER_H_
