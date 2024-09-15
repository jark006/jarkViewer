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

#ifndef WP2_ENC_PARTITION_SCORE_FUNC_TILE_H_
#define WP2_ENC_PARTITION_SCORE_FUNC_TILE_H_

#include "src/common/global_params.h"
#include "src/common/lossy/block_size.h"
#include "src/common/lossy/predictor.h"
#include "src/common/progress_watcher.h"
#include "src/dec/tile_dec.h"
#include "src/enc/analysis.h"
#include "src/enc/partitioning/partition_score_func.h"
#include "src/enc/tile_enc.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"
#include "src/wp2/encode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Based on tile encoding then decoding. Warning: very slow.
class TileScoreFunc : public PartitionScoreFunc {
 public:
  explicit TileScoreFunc(
      PartitionMethod sub_partition_method = MULTIPASS_PARTITIONING)
      : sub_partition_method_(sub_partition_method) {}

  WP2Status Init(const EncoderConfig& config, const Rectangle& tile_rect,
                 const YUVPlane& yuv, const GlobalParams& gparams,
                 const ProgressRange& progress) override;

  // Encodes the whole tile with the forced 'blocks' and returns a score based
  // on its distortion and size in bytes.
  WP2Status TryEncode(const VectorNoCtor<Block>& blocks,
                      const ProgressRange& progress, float* score);

  // Temporarily forces the 'block' for a TryEncode() call.
  WP2Status ComputeScore(const Block& block, const ProgressRange& progress,
                         float* score) override;

  // Forces the 'block' for all future TryEncode() calls.
  WP2Status Use(const Block& block) override;

 protected:
  WP2Status InitForEncode();

  const PartitionMethod sub_partition_method_;
  EncoderConfig tmp_config_;
  VectorNoCtor<Block> blocks_;
  EncTilesLayout enc_tiles_layout_;
  TileEncoder tile_encoder_;
  ArgbBuffer argb_;

  // These two are used per tile. More convenient than the already computed one
  // for the whole image.
  FeatureMap local_features_map_;
  GlobalParams local_gparams_;

  float best_score_ = 0.f;         // Best score before last call to Use().
  float cached_best_score_ = 0.f;  // Best score since last call to Use().

  YUVPlane decompressed_yuv_;
  ArgbBuffer decompressed_argb_;
  BitstreamFeatures features_;
  DecoderConfig dec_config_;
  TilesLayout tiles_layout_;
  float distortion_[5];

 private:  // Visual debug.
  WP2Status RegisterScoreForVDebug(const char label[], const Block& block,
                                   float score) const;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITION_SCORE_FUNC_TILE_H_
