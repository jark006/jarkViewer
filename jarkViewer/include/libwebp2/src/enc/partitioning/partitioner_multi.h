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

#ifndef WP2_ENC_PARTITIONER_MULTI_H_
#define WP2_ENC_PARTITIONER_MULTI_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partition_score_func_multi.h"
#include "src/enc/partitioning/partitioner.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Uses several metrics to select block sizes.
class MultiPassPartitioner : public Partitioner {
 public:
  // This partitioner needs a specific MultiScoreFunc.
  explicit MultiPassPartitioner(MultiScoreFunc* const score_func)
      : multi_score_func_(score_func) {}

  WP2Status GetBestPartition(const ProgressRange& progress,
                             VectorNoCtor<Block>* blocks,
                             Vector_u32* splits) override;

 private:
  enum class Grid { Snapped, NonSnapped, All };

  // Selects blocks of dimension 'block_size', good enough according to the
  // given 'pass' and 'restrict_to' some alignment.
  WP2Status GetBlocks(MultiScoreFunc::Pass pass, BlockSize block_size,
                      Grid restrict_to, const ProgressRange& progress,
                      uint32_t* max_num_blocks_left, VectorNoCtor<Block>* out);

  WP2Status SelectBlock(MultiScoreFunc::Pass pass, const Block& block,
                        uint32_t* max_num_blocks_left,
                        VectorNoCtor<Block>* out);

  MultiScoreFunc* const multi_score_func_;  // Specific 'score_func_'.
  uint32_t pass_index_ = 0;

  WP2Status RegisterPassForVDebug(MultiScoreFunc::Pass pass,
                                  BlockSize block_size,
                                  uint32_t num_chosen_blocks) const;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITIONER_MULTI_H_
