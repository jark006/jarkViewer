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
// Tool for finding the best block layout.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_ENC_PARTITIONER_SPLIT_H_
#define WP2_ENC_PARTITIONER_SPLIT_H_

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/predictor.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partition_score_func.h"
#include "src/enc/partitioning/partitioner.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// For each block, tries some split patterns and keeps the best one then
// recurses on sub-blocks.
class SplitPartitioner : public Partitioner {
 public:
  WP2Status Init(const EncoderConfig& config, const YUVPlane& yuv,
                 const Rectangle& tile_rect,
                 PartitionScoreFunc* score_func) override;
  WP2Status GetBestPartition(const ProgressRange& progress,
                             VectorNoCtor<Block>* blocks,
                             Vector_u32* splits) override;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITIONER_SPLIT_H_
