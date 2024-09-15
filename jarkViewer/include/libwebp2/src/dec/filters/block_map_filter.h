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
// Map of block features to be used by filters.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_FILTERS_BLOCK_MAP_FILTER_H_
#define WP2_DEC_FILTERS_BLOCK_MAP_FILTER_H_

#include <cstdint>

#include "src/common/lossy/block.h"
#include "src/common/lossy/block_size.h"
#include "src/utils/csp.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

struct BitstreamFeatures;
struct TilesLayout;
struct DecoderConfig;
class GlobalParams;

// Returns true if at least one filter is enabled and needs FilterBlockMap data.
bool IsFilterBlockMapNeeded(const DecoderConfig& config,
                            const GlobalParams& gparams,
                            const TilesLayout& tiles_layout);

//------------------------------------------------------------------------------

class FilterBlockMap {
 public:
  // 'tile_rect' contains the position and the dimensions of the tile in the
  // image in pixels, not padded.
  // 'pixels' should point to the padded tile samples.
  void Init(const Rectangle& tile_rect, BitDepth bit_depth, int32_t yuv_min,
            int32_t yuv_max, YUVPlane* pixels);

  // Allocates storage to register block features.
  WP2Status Allocate();

  // Registers the features of a block.
  void RegisterBlock(uint32_t x, uint32_t y, BlockSize dim, uint32_t segment_id,
                     uint8_t min_bpp, bool has_lossy_alpha,
                     const uint32_t res_den[4]);
  void RegisterBlock(const CodedBlockBase& cb, uint32_t min_num_bytes);

  void Clear();

 public:
  struct BlockFeatures {
    uint32_t index;        // Unique block index (vdebug).
    BlockSize size;        // Width, height.
    uint8_t segment_id;    // The id of the segment it belongs to.
    uint8_t min_bpp;       // Lower bound of num bits-per-pixel. 255 is ~2 bpp.
    uint8_t res_den[4];    // Residual density per channel. 255 is all non-zero.
    bool has_lossy_alpha;  // Whether the block has lossy alpha.
  };

  Rectangle tile_rect_;  // Tile bounds within frame (px, not padded).
  BitDepth bit_depth_ = {0, false};
  int32_t yuv_min_ = 0, yuv_max_ = 0;
  YUVPlane* pixels_ = nullptr;  // Tile samples, padded.

  uint32_t max_num_blocks_x_ = 0;
  uint32_t max_num_blocks_y_ = 0;
  VectorNoCtor<BlockFeatures> features_;

  uint32_t max_registered_height_ = 0;  // Block height in pixels.

  const BlockFeatures* GetRow(uint32_t y) const;

 protected:
  uint32_t current_block_index_ = 0;

 public:
  void ApplyVDebug(const DecoderConfig& config, ArgbBuffer* debug_output);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_FILTERS_BLOCK_MAP_FILTER_H_
