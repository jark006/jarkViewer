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
//   Block sizes and partitions
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_LOSSY_BLOCK_SIZE_IO_H_
#define WP2_COMMON_LOSSY_BLOCK_SIZE_IO_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/dec/symbols_dec.h"
#include "src/enc/symbols_enc.h"
#include "src/utils/ans_enc.h"
#include "src/utils/front_mgr.h"
#include "src/utils/split_iterator.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

constexpr uint32_t kMaxSplitPatterns = 10;
// Index of the "split pattern" that keeps the block whole.
static constexpr uint32_t kNoSplitIndex = 0;

class PartitionSetStats {
 public:
  static constexpr uint8_t kInvalidSlot = BLK_LAST;
  PartitionSetStats(PartitionSet partition_set, const BlockSize blocks[]);

 public:
  const PartitionSet partition_set_;
  uint8_t num_blocks_[BLK_LAST];              // Per bounding size.
  uint32_t block_index_[BLK_LAST];            // Index of given size in the set.
  BlockSize blocks_[BLK_LAST][BLK_LAST + 1];  // Per bounding size, size array.
  uint8_t slots_[BLK_LAST][BLK_LAST];         // Per bounding size, index array.
  BlockSize smallest_bounds_[BLK_LAST];       // Per bounding size.
  uint32_t num_unique_bounds_;                // Among 'smallest_bounds_'.
  BlockSize unique_bounds_[BLK_LAST];         // At most 'BLK_LAST' elements.
  uint32_t unique_bounds_index_[BLK_LAST];    // Each is < 'num_unique_bounds_'.
  uint32_t used_trfs_;                        // Bitset for used transforms.
  // Number of split patterns per block size (max kMaxSplitPatterns).
  uint32_t num_split_patterns_[BLK_LAST];
  // Split patterns per block size.
  SplitPattern split_patterns_[BLK_LAST][kMaxSplitPatterns];
};
static_assert(TRF_LAST < 32u, "can't use 32b bitset for transforms used.");

extern const PartitionSetStats kPartSetsStats[];

//------------------------------------------------------------------------------

// Predefined partitioning sets of blocks for a given bounding BlockSize.
const BlockSize* GetBlockSizes(PartitionSet set, BlockSize bounds = BLK_32x32);
uint32_t GetNumBlockSizes(PartitionSet set, BlockSize bounds = BLK_32x32);
bool TrfIsUsed(PartitionSet set, TrfSize tdim);

// Returns one of the biggest BlockSizes from the 'set' fitting in 'bounds'.
BlockSize GetFittingBlockSize(PartitionSet set, BlockSize bounds);

// Returns true if 'size' belongs in 'set'.
bool IsBlockSizeInSet(PartitionSet set, BlockSize size);

// For given 'bounds', contains the smallest BlockSize at least as wide and
// tall as each BlockSize belonging to the 'set' fitting into these 'bounds'.
// Same as GetFittingBlockSize() unless the 'set' is sparse (meaning some
// width/height combinations are missing up to the biggest square).
BlockSize GetSmallestBounds(PartitionSet set, BlockSize bounds);

// Returns the number of bounds that lead to different subsets of BlockSizes.
uint32_t GetNumUniqueBounds(PartitionSet set);
// Returns the 'index'th among all unique smallest bounds.
BlockSize GetUniqueBounds(PartitionSet set, uint32_t index);

// Returns the number of ways that the block 'size' can be split, when using
// the partition set 'set'.
uint32_t GetNumSplitPatterns(PartitionSet set, BlockSize size);
// Returns an array of split patterns for the given partition set and block
// size. The array has length GetNumSplitPatterns(set, size).
const SplitPattern* GetSplitPatterns(PartitionSet set, BlockSize size);

//------------------------------------------------------------------------------

// Writes 'dim'.
void WriteBlockSize(const FrontMgrDoubleOrderBase& mgr, BlockSize dim,
                    SymbolManager* sm, ANSEncBase* enc);
void WriteBlockSize(BlockSize dim, BlockSize max_possible_size,
                    PartitionSet partition_set, SymbolManager* sm,
                    ANSEncBase* enc);
// Reads, updates and returns 'dim'.
BlockSize ReadBlockSize(const FrontMgrDoubleOrderBase& mgr, SymbolReader* sr);
BlockSize ReadBlockSize(BlockSize max_possible_size, PartitionSet partition_set,
                        SymbolReader* sr);

//------------------------------------------------------------------------------

// Writes the split 'split_idx'. Asserts that it.CurrentBlock().splittable
void WriteBlockSplit(const SplitIteratorBase& it, uint32_t split_idx,
                     SymbolManager* sm, ANSEncBase* enc);

// Reads and returns 'split_idx'. Asserts that it.CurrentBlock().splittable
WP2Status ReadBlockSplit(const SplitIteratorBase& it, SymbolReader* sr,
                         uint32_t* split_idx);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_COMMON_LOSSY_BLOCK_SIZE_IO_H_
