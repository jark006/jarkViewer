// Copyright 2021 Google LLC
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
// Author: Maryla (maryla@google.com)

#ifndef WP2_ENC_PARTITIONER_SPLIT_RECURSE_H_
#define WP2_ENC_PARTITIONER_SPLIT_RECURSE_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/predictor.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partition_score_func.h"
#include "src/enc/partitioning/partition_score_func_block.h"
#include "src/enc/partitioning/partitioner.h"
#include "src/enc/symbols_enc.h"
#include "src/utils/ans_utils.h"
#include "src/utils/plane.h"
#include "src/utils/split_iterator.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// For each block, tries some split patterns and keeps the best one then
// recurses on sub-blocks.
class SplitRecursePartitioner : public Partitioner {
 public:
  WP2Status Init(const EncoderConfig& config, const YUVPlane& yuv,
                 const Rectangle& tile_rect,
                 PartitionScoreFunc* score_func) override;
  WP2Status GetBestPartition(const ProgressRange& progress,
                             VectorNoCtor<Block>* blocks,
                             Vector_u32* splits) override;

 private:
  // Tries all split patterns for the block it.CurrentBlock() and appends the
  // split indices to 'best_pattern_idxs'.
  WP2Status FindBestPattern(
      const ProgressRange& progress, const VectorNoCtor<Block>& forced_blocks,
      const SplitIteratorDefault& it, BlockScoreFunc* score_func,
      SymbolCounter* symbol_counter, ANSEncCounter* counter, float extra_rate,
      uint32_t recursion_level, bool selected, Vector_u32* best_pattern_idxs,
      float* best_score) const;

  // Recursively computes the score for the given split pattern.
  WP2Status ComputeScore(const ProgressRange& progress,
                         const VectorNoCtor<Block>& forced_blocks,
                         const SplitIteratorDefault& it, uint32_t split_idx,
                         Block sub_blocks[4], uint32_t num_sub_blocks,
                         BlockScoreFunc* score_func,
                         SymbolCounter* symbol_counter, ANSEncCounter* counter,
                         float best_score, float extra_rate,
                         uint32_t recursion_level, bool selected,
                         Vector_u32* best_pattern_idxs, float* score) const;

  WP2Status VDebugRecursionLevel(const Block& block,
                                 uint32_t recursion_level) const;
  bool VDebugBlockSelected(const Block& block) const;
  WP2Status RegisterScoreForVDebug(const Block& block, uint32_t recursion_level,
                                   const float scores[4], float extra_rate,
                                   float total_score, bool is_best) const;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITIONER_SPLIT_RECURSE_H_
