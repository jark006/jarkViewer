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
// Block position/size scoring functions.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_ENC_PARTITION_SCORE_FUNC_MULTI_H_
#define WP2_ENC_PARTITION_SCORE_FUNC_MULTI_H_

#include <cstdint>

#include "src/common/global_params.h"
#include "src/common/integral.h"
#include "src/common/lossy/block_size.h"
#include "src/common/lossy/predictor.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partition_score_func.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Uses several metrics to select block sizes.
class MultiScoreFunc : public PartitionScoreFunc {
 public:
  static constexpr float kMinScore = 0.5f;  // Blocks below that are discarded.

  enum class Pass {     // The following passes select blocks with:
    LumaAlphaGradient,  // gradient-like original luma and alpha values
    NarrowStdDev,  // a narrow range of orig luma/alpha std dev per 4x4 block
    Direction,     // a common direction with a high enough certainty
    Any,           // no criterion (used for filling what's left)
  };

  WP2Status Init(const EncoderConfig& config, const Rectangle& tile_rect,
                 const YUVPlane& yuv, const GlobalParams& gparams,
                 const ProgressRange& progress) override;

  // Sets the 'pass_' of the next ComputeScore().
  void SetPass(Pass pass) { pass_ = pass; }
  // Returns a value/threshold ratio of the current 'pass_' in ]0:1].
  // Higher means better, scores above 0.5 are selected.
  WP2Status ComputeScore(const Block& block, const ProgressRange& progress,
                         float* score) override;

 protected:
  // Passes. Values below threshold are selected.
  float GetLumaAlphaGradient(const Block& block) const;
  float GetLumaAlphaGradientThreshold(const Block& block) const;

  float GetStdDevRange(const Block& block) const;
  float GetStdDevRangeThreshold(const Block& block) const;

  float GetDirection(const Block& block) const;
  float GetDirectionThreshold(const Block& block) const;

  float yuv_range_ = 0.f;  // Range of allowed original luma values.
  float a_range_ratio_;    // Range of yuv values over range of alpha values.

  // Results of image processing done on the original planes.
  YUVPlane min_, max_;  // Minimum and maximum original values per block.
  Integral stddev_;     // Contains quantized luma std dev of the block grid.
  Integral a_stddev_;   // Contains std dev for the alpha plane (if any).

  // Pixel orientation of the block grid. TODO(yguyon): Use less memory
  Vector_u32 direction_;            // [0:7] maps from +45 to -112.5 degrees.
  Vector_u32 direction_certainty_;  // [0:3] the higher the more confident.

  Pass pass_ = Pass::Any;

  // Visual debug.
  void DrawValueThresholdColor(uint32_t cell_width, uint32_t cell_height,
                               const Block& block,
                               ArgbBuffer* debug_output) const;
  WP2Status DrawSelection(const Block& block, const Rectangle& block_rect,
                          ArgbBuffer* debug_output) const;
  WP2Status DrawVDebug() const;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITION_SCORE_FUNC_MULTI_H_
