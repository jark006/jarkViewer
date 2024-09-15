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
// Deblocking filter.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_FILTERS_DEBLOCKING_FILTER_H_
#define WP2_DEC_FILTERS_DEBLOCKING_FILTER_H_

#include <cstdint>

#include "src/dec/filters/block_map_filter.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

bool IsDeblockingFilterEnabled(const DecoderConfig& config,
                               const GlobalParams& gparams);

//------------------------------------------------------------------------------

class DeblockingFilter {
 public:
  DeblockingFilter(const DecoderConfig& config, const GlobalParams& gparams,
                   const FilterBlockMap& blocks);

  // Allocates storage.
  WP2Status Allocate();

  // Performs the deblocking up to 'num_rows' in YUV. Returns the total number
  // of deblocked pixel rows (still in YUV space).
  uint32_t Deblock(uint32_t num_rows);

  // Performs the deblocking of an edge represented with the following notation:
  //   p3 p2 p1 p0 | q0 q1 q2 q3
  //      p2 p1 p0 | q0 q1 q2
  //      p2 p1 p0 | q0 q1 q2
  // with the edge located between each pair of adjacent pixels p0 and q0.
  // 'edge_rect' contains the first q0 coordinates and the number of times this
  // pattern is filtered along the edge (either width or height must be 0).
  static void DeblockEdge(const DecoderConfig& config, bool intertile,
                          const Rectangle& tile_rect, bool has_alpha,
                          uint32_t yuv_filter_magnitude, uint32_t yuv_num_bits,
                          int32_t yuv_min, int32_t yuv_max,
                          const FilterBlockMap::BlockFeatures& p_block,
                          const FilterBlockMap::BlockFeatures& q_block,
                          const Rectangle& edge_rect, YUVPlane* pixels);

  // Visual debugging.
  static void RegisterPixelsForVDebug(
      const DecoderConfig& config, uint32_t q0_x, uint32_t q0_y,
      uint32_t max_half, uint32_t filtered_half, bool horizontal_filtering,
      Channel channel, uint32_t strength, uint32_t sharpness,
      bool before_deblocking, bool deblocked, uint32_t num_bits,
      const int16_t* q0, uint32_t step);

 private:
  // Performs the horizontal deblocking of vertical edges in [from_y:to_y].
  // Returns the total number of horizontally deblocked pixel rows ('to_y + 1').
  uint32_t DeblockHorizontally(uint32_t from_y, uint32_t to_y);

  // Performs the vertical deblocking of horizontal edges up to 'to_y'. Returns
  // the total number of fully deblocked pixel rows (at most 'to_y + 1').
  uint32_t DeblockVertically(uint32_t to_y);

 private:
  const DecoderConfig& config_;
  const GlobalParams& gparams_;
  const FilterBlockMap& blocks_;
  const bool enabled_;

  uint32_t num_horizontally_deblocked_rows_ = 0;  // Rows of pixels.
  uint32_t num_vertically_deblocked_rows_ = 0;

  // For a given x, contains the y index of the next horizontal edge to deblock
  // (in fact it's the y index of the block just below the down-most already
  // deblocked horizontal edge or 0 if none yet). In kMinBlockSizePix units.
  Vector<uint16_t> x_to_next_up_block_y_;

  // The minimum number of available rows to continue the vertical deblocking.
  // Increasing it by 'max_registered_height_' ensures at least one segment per
  // column is deblocked when the filter is run, avoiding useless browsing of
  // 'x_to_next_up_block_y_'.
  uint32_t min_num_rows_to_vdblk_ = 0;

 public:
  // Estimate the filter parameters to apply from available data.
  static uint32_t GetStrengthFromMagnitude(uint32_t filter_magnitude);
  static uint32_t GetStrengthFromBpp(uint32_t num_bits_per_pixel);
  static uint32_t GetSharpnessFromMagnitude(uint32_t filter_magnitude);
  static uint32_t GetSharpnessFromResidualDensity(uint32_t res_den);

 private:
  // Visual debugging.
  YUVPlane unfiltered_pixels_;
  uint32_t num_debug_rows_ = 0;

  void SavePixelsForVDebug(uint32_t num_rows);
  void ApplyVDebug(uint32_t num_rows);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_FILTERS_DEBLOCKING_FILTER_H_
