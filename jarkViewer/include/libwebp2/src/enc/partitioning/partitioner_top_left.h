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

#ifndef WP2_ENC_PARTITIONER_TOP_LEFT_H_
#define WP2_ENC_PARTITIONER_TOP_LEFT_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partitioner.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

// For each block (starting with the top-left one), takes the best size.
class TopLeftBestPartitioner : public Partitioner {
 public:
  WP2Status GetBestPartition(const ProgressRange& progress,
                             VectorNoCtor<Block>* blocks,
                             Vector_u32* splits) override;

 private:
  // Returns the 'best_block' size within 'max_block'.
  WP2Status GetBestSize(const Block& max_block, uint32_t num_forced_blocks,
                        const ProgressRange& progress, Block* best_block);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITIONER_TOP_LEFT_H_
