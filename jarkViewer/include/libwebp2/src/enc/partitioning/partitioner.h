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

#ifndef WP2_ENC_PARTITIONER_H_
#define WP2_ENC_PARTITIONER_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/predictor.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partition_score_func.h"
#include "src/utils/plane.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Used to find the best block layout in a tile.
class Partitioner : public WP2Allocable {
 public:
  virtual ~Partitioner() = default;

  // Configures the partition settings and allocates memory.
  virtual WP2Status Init(const EncoderConfig& config, const YUVPlane& yuv,
                         const Rectangle& tile_rect,
                         PartitionScoreFunc* score_func);

  // Computes and returns the best layout. Input 'blocks' are considered forced.
  // Some partitioners also fill in 'splits' (if not nullptr), as an
  // alternate way to encode block geometry. See also split_iterator.h
  virtual WP2Status GetBestPartition(const ProgressRange& progress,
                                     VectorNoCtor<Block>* blocks,
                                     Vector_u32* splits) = 0;

 protected:
  // Gets or sets areas as occupied.
  bool IsOccupied(const Block& block) const;
  void Occupy(const Block& block);

  // Returns true if the 'block' fits within 'max_block', is snapped etc.
  bool IsBlockValid(const Block& block, const Block& max_block,
                    uint32_t num_forced_blocks) const;

  // Verifies that the 'forced_blocks' don't overlap each other.
  // 'max_num_blocks_left' will be decremented by the sum of their areas.
  WP2Status RegisterForcedBlocks(const VectorNoCtor<Block>& forced_blocks,
                                 uint32_t* max_num_blocks_left);

  const EncoderConfig* config_ = nullptr;
  Rectangle tile_rect_;            // In pixels, not padded.
  const YUVPlane* src_ = nullptr;  // Original image, padded.
  uint32_t num_block_cols_ = 0;    // Maximum number of blocks on the horizontal
  uint32_t num_block_rows_ = 0;    // and vertical axis.
  PartitionScoreFunc* score_func_ = nullptr;

  VectorNoCtor<bool> occupancy_;  // For a given pos, true if occupied.

  // Visual debug: display the order in which the 'block' was chosen and during
  // which pass.
  WP2Status RegisterOrderForVDebug(uint32_t pass, uint32_t pass_index,
                                   const Block& block, uint32_t block_index,
                                   uint32_t max_num_blocks) const;
};

//------------------------------------------------------------------------------

// Verifies and adds the blocks concerning this 'tile' from
// 'EncoderInfo::force_partition' to 'out' (debug).
WP2Status AddForcedBlocks(const EncoderConfig& config,
                          const Rectangle& padded_tile_rect,
                          VectorNoCtor<Block>* out);

// Returns true if the given split pattern (given as the whole block + sub
// blocks) is compatible with the given forced blocks which must be sorted.
// 'splittable' indicates if each sub block is further splittable.
bool IsCompatibleWithForcedBlocks(const Block& block, const Block sub_blocks[4],
                                  const bool splittable[4],
                                  uint32_t num_sub_blocks,
                                  const VectorNoCtor<Block>& forced_blocks);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITIONER_H_
