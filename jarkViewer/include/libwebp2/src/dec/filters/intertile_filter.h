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
// Intertile filter.
// Same as deblocking filter but for tiles instead of transform blocks.
// It is done in a single-threaded pass because it's computationally cheap (few
// edges). However it can be run incrementally so it can be applied after each
// decoding of all available pixel rows.
// TODO(yguyon): Also filter edges between tiles and background color, if any.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_FILTERS_INTERTILE_FILTER_H_
#define WP2_DEC_FILTERS_INTERTILE_FILTER_H_

#include <cstdint>

#include "src/common/global_params.h"
#include "src/dec/filters/block_map_filter.h"
#include "src/dec/tile_dec.h"
#include "src/utils/plane.h"
#include "src/wp2/decode.h"

namespace WP2 {

//------------------------------------------------------------------------------

bool IsIntertileFilterEnabled(const DecoderConfig& config,
                              const GlobalParams& gparams,
                              const TilesLayout& tiles_layout);

//------------------------------------------------------------------------------

class IntertileFilter {
 public:
  // 'config', 'tiles_layout' and 'features' are used to determine whether
  // the filter will be enabled and its strength. 'canvas' should contain the
  // padded compressed YUV samples to be filtered. Calls Clear().
  void Init(const DecoderConfig& config, const GlobalParams& gparams,
            const TilesLayout& tiles_layout, YUVPlane* canvas);

  // Resets the filter. After that, Init() must be called before Deblock().
  void Clear();

  // Performs the deblocking up to 'num_rows' of RGB pixels.
  void Deblock(uint32_t num_rows);

  // Returns the total number of deblocked pixel rows.
  uint32_t GetNumFilteredRows() const { return num_vertically_deblocked_rows_; }

 private:
  void DeblockHorizontally(uint32_t num_rows);  // For vertical edges.
  void DeblockVertically(uint32_t num_rows);    // For horizontal edges.

 private:
  const DecoderConfig* config_ = nullptr;
  const GlobalParams* gparams_ = nullptr;
  const TilesLayout* tiles_layout_ = nullptr;
  YUVPlane canvas_;  // Non-padded compressed YUV samples.
  bool enabled_ = false;

  uint32_t num_horizontally_deblocked_rows_ = 0;  // Of pixels.
  uint32_t num_vertically_deblocked_rows_ = 0;

 private:
  // Visual debugging.
  YUVPlane unfiltered_pixels_;
  uint32_t num_debug_rows_ = 0;

  // Sets 'num_rows' to 0 if all rows are needed.
  void SavePixelsForVDebug(uint32_t num_rows);
  // Modifies 'canvas_' if all rows were filtered.
  void ApplyVDebug(uint32_t num_rows);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_FILTERS_INTERTILE_FILTER_H_
