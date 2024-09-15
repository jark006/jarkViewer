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

#ifndef WP2_ENC_PARTITIONER_EXHAUSTIVE_H_
#define WP2_ENC_PARTITIONER_EXHAUSTIVE_H_

#include <array>
#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/predictor.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partition_score_func.h"
#include "src/enc/partitioning/partition_score_func_tile.h"
#include "src/enc/partitioning/partitioner.h"
#include "src/utils/front_mgr.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Tries all possible block layouts. Warning: super slow.
class ExhaustivePartitioner : public Partitioner {
 public:
  // This partitioner needs a specific PartitionScoreFunc.
  explicit ExhaustivePartitioner(TileScoreFunc* const score_func)
      : tile_score_func_(score_func) {}

  WP2Status Init(const EncoderConfig& config, const YUVPlane& yuv,
                 const Rectangle& tile_rect,
                 PartitionScoreFunc* score_func) override;

  WP2Status GetBestPartition(const ProgressRange& progress,
                             VectorNoCtor<Block>* blocks,
                             Vector_u32* splits) override;

 private:
  // Finds the next valid partition. If 'found', returns its 'score'.
  WP2Status GetNextPartitionScore(const ProgressRange& progress, bool* found,
                                  float* score);

  // Pops blocks from the 'partition_' until a new path can be explored.
  // Returns false if none found or returns true and the 'next_block_size'.
  bool FindNewBranch(BlockSize* next_block_size);

  TileScoreFunc* const tile_score_func_;  // Specific 'score_func_'.

  // For a given block size, stores the next one in the current partition set.
  std::array<BlockSize, BLK_LAST + 1> next_block_sizes_;

  VectorNoCtor<Block> forced_blocks_;
  FrontMgrLexico front_mgr_;       // To easily browse the partitions.
  VectorNoCtor<Block> partition_;  // Matches the layout of the 'front_mgr_'.

  // VDebug
  WP2Status RegisterScoreForVDebug(float best_partition_score,
                                   uint64_t best_partition_size,
                                   uint64_t num_iterations) const;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITIONER_EXHAUSTIVE_H_
