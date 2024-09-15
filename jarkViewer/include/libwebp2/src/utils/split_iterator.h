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
// Iterator for splitting blocks.
//
// Author: Maryla (maryla@google.com)

#ifndef WP2_UTILS_SPLIT_ITERATOR_H_
#define WP2_UTILS_SPLIT_ITERATOR_H_

#include <cstdint>

#include "src/common/lossy/block.h"
#include "src/common/lossy/block_size.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Class for iterating over blocks by splitting them using various split
// patterns. Usage:
// while (!it.Done()) {
//   if (it.CurrentBlock().splittable) {
//     (... choose a split_idx out of it.NumSplitPatterns())
//     it.SplitCurrentBlock(split_idx);
//   } else {
//     (... do something with it.CurrentBlock().block)
//     it.NextBlock();
//   }
// }
class SplitIteratorBase {
 public:
  virtual ~SplitIteratorBase() = default;

  virtual WP2Status Init(PartitionSet partition_set, uint32_t width,
                         uint32_t height);

  // Returns true if the whole area has been covered.
  bool Done() const;
  PartitionSet partition_set() const { return partition_set_; }

  struct SplitInfo {
    Block block;
    // True if the block can be split, false if it is final.
    bool splittable;
  };
  // Returns the current block being considered.
  virtual const SplitInfo& CurrentBlock() const = 0;

  // -- If the current block is splittable --

  // Returns the number of possible split patterns for the current block.
  // A split pattern is a way of splitting (or not) a block into 1 to 4 blocks,
  // e.g. keeping it as a single block, splitting in 2 vertically or
  // horizontally, splitting in 4, etc.
  uint32_t NumSplitPatterns() const;
  // Fills 'sub_blocks' with the various sub-blocks corresponding to the
  // split_pattern 'split_idx' for the current block. 'split_idx' must be
  // smaller than NumSplitPatterns(). Fills 'splittable' with booleans
  // indicating if each sub block is further splittable.
  // Returns the number of sub-blocks (between 1 and 4).
  uint32_t GetSplitPatternBlocks(uint32_t split_idx, Block sub_blocks[4],
                                 bool splittable[4]) const;
  // Splits the current block using the split_idx'th split pattern. Only valid
  // if CurrentBlock().splittable is true. 'split_idx' must be smaller than
  // NumSplitPatterns(). Updates the current block.
  virtual void SplitCurrentBlock(uint32_t split_idx) = 0;

  // -- If the current block is NOT splittable --

  // Updates the CurrentBlock() to the next block. Only valid if
  // CurrantBlock().splittable is false.
  virtual void NextBlock() = 0;

 protected:
  WP2Status CopyInternal(const SplitIteratorBase& other);
  // Fills 'splittable' with booleans indicating if each sub block is further
  // splittable.
  virtual void GetSplittable(const Block sub_blocks[4], uint32_t num_sub_blocks,
                             bool splittable[4]) const = 0;

  // Returns the 'sub_blocks' given a 'block' (size and position) and a
  // 'split_pattern'.
  static void GetSplitPatternBlocks(const Block& block,
                                    const SplitPattern& split_pattern,
                                    Block* sub_blocks);

  FrontMgrBase front_mgr_;
  PartitionSet partition_set_;
};

// SplitIterator that iterates over blocks in lexicographic order.
class SplitIteratorLexico : public SplitIteratorBase {
 public:
  WP2Status Init(PartitionSet partition_set, uint32_t width,
                 uint32_t height) override;
  WP2Status CopyFrom(const SplitIteratorLexico& other);

  const SplitInfo& CurrentBlock() const override;
  void SplitCurrentBlock(uint32_t split_idx) override;
  void NextBlock() override;

 protected:
  void GetSplittable(const Block sub_blocks[4], uint32_t num_sub_blocks,
                     bool splittable[4]) const override;

 private:
  void SetCurrentBlock(uint32_t bx, uint32_t by);
  bool IsSplittable(BlockSize dim) const;

  // Same as SplitInfo but with just the BlockSize instead of Block (to make it
  // smaller).
  struct LightSplitInfo {
    BlockSize size;   // BLK_LAST if nothing decided yet.
    bool splittable;  // False if the decision is definitive
                      // or true if smaller blocks may be tried.
    bool operator==(const LightSplitInfo& other) const {
      return size == other.size && splittable == other.splittable;
    }
    bool operator!=(const LightSplitInfo& other) const {
      return !operator==(other);
    }
  };
  // No decision.
  static constexpr LightSplitInfo kNoSplitInfo = {BLK_LAST, false};

  // Gets the split pattern decision for a given block position.
  const LightSplitInfo& GetSplitInfo(uint32_t x, uint32_t y) const;
  // Sets the split pattern decision for a given 'block' area.
  void SetSplitInfo(const Block& block, bool recursive);

  SplitInfo current_block_;
  Plane<LightSplitInfo> split_info_;
};

// SplitIterator that iterates over blocks in recursive (quad tree like) order.
class SplitIteratorRecurse : public SplitIteratorBase {
 public:
  WP2Status Init(PartitionSet partition_set, uint32_t width,
                 uint32_t height) override;
  WP2Status CopyFrom(const SplitIteratorRecurse& other);

  void SplitCurrentBlock(uint32_t split_idx) override;
  void NextBlock() override;
  const SplitInfo& CurrentBlock() const override;

 protected:
  void GetSplittable(const Block sub_blocks[4], uint32_t num_sub_blocks,
                     bool splittable[4]) const override;

 private:
  void SetCurrentBlockLexico();

  Vector<SplitInfo> block_queue_;
};

// This is the SplitIterator class that should be used by default.
typedef SplitIteratorRecurse SplitIteratorDefault;

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_UTILS_SPLIT_ITERATOR_H_
