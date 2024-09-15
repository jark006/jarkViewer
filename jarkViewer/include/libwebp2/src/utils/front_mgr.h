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
// Front manager: common struct for encoder/decoder, handling the
// front line of unprocessed blocks.
//
// Authors: Skal (pascal.massimino@gmail.com)

#ifndef WP2_UTILS_FRONT_MGR_H_
#define WP2_UTILS_FRONT_MGR_H_

#include <cassert>
#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

// Base class for a simple front manager that only fills the wave front
// and keeps track of the filling height of the columns.
class FrontMgrBase {
 public:
  // 'width' and 'height' are the dimensions of the current canvas in pixels.
  FrontMgrBase() = default;
  virtual ~FrontMgrBase() = default;
  FrontMgrBase(const FrontMgrBase&) = delete;

  // Allocates storage. Returns false in case of memory error.
  // Must be called first (once) before using the other methods.
  WP2Status InitBase(uint32_t width, uint32_t height);

  // Clears the front manager and sets it to an initial state.
  virtual void Clear();

  // Marks the block as used. The block must be on the "wavefront", i.e.
  // blocks to its top and left must already have been used.
  void Use(const Block& block);
  // Undoes Use() for the given block.
  void UndoUse(const Block& block);

  // Returns the y position of the first unused sub-block at column position 'x'
  // (hence, returns 0 if column is empty). Note that parameter 'x' can be out
  // of bound, in which case it returns 0.
  // 'x' and the returned value are in kMinBlockSizePix units.
  uint32_t GetOccupancy(int x) const;

  // Returns the extent of the top context to the right of the block top in
  // kMinBlockSizePix units.
  uint32_t GetRightContextExtent(const Block& blk) const;

  // Returns true if the block at position x, y (in kMinBlockSizePix units)
  // has already been marked as 'used'.
  bool IsUsed(uint32_t x, uint32_t y) const;

  // Returns true if all the surface is covered (has been marked 'used').
  bool Done() const { return (left_ == 0); }

  // Finds the coordinates of the next unused block in lexicographic order.
  void FindNextLexico(uint32_t* bx, uint32_t* by) const;
  // Same as above but we know the last block that got added (speed optim).
  void FindNextLexico(const Block& added_last, uint32_t* bx,
                      uint32_t* by) const;
  // Returns the largest possible block that fits at coordinates (x, y),
  // taking snapping into account.
  Block LargestPossibleBlockAt(uint32_t x, uint32_t y, bool snapped) const;

  WP2Status CopyInternal(const FrontMgrBase& other);

 protected:
  Vector_u32 occupancy_;
  uint32_t bwidth_ = 0, bheight_ = 0;  // In kMinBlockSizePix units.
  uint32_t left_ = 0;
};

//------------------------------------------------------------------------------

// Simple front manager that only fills 4x4 blocks.
// It is only useful to get the context of 4x4 blocks.
class FrontMgr4x4 : public FrontMgrBase {
 public:
  using FrontMgrBase::FrontMgrBase;

  WP2Status CopyFrom(const FrontMgr4x4& other);
  void Clear() override;

  // Uses the next available 4x4 block in lexicographic order.
  void Use4x4();

 private:
  uint32_t bx_ = 0, by_ = 0;
};

//------------------------------------------------------------------------------

// Base class managing the reading/writing of blocks in a given order.
// There are two things concerning a block: its size, and everything else
// (including block pixels).
// These two aspects can be processed independently and in different orders.
// - Size order: order in which block sizes are written/read.
// - Block order: order of encoding/decoding of block pixels.
// This class decides both processing orders, and keeps track of which blocks
// have been processed, for both aspects (size and block pixels).
//
// Encoding side:
// MyFrontMgrSubclass mgr;
// mgr.Init(...)
// VectorNoCtor<Block> blocks;
// (... fill blocks)
// Vector_u16 size_order_indices;
// mgr.Sort(blocks, size_order_indices);
// Vector<CodedBlock> cblocks;
// (... fill cblocks in same order as 'blocks')
// // Start writing to the bitsream.
// for (uint16_t ind : size_order_indices) {
//   // Write a block size.
//   {
//     const CodedBlock& cb = cblocks[ind];
//     ( ... write cb.dim() to the bistream)
//     Block block_tmp;
//     WP2_CHECK_ALLOC_OK(mgr->UseSize(cb.dim(), &block_tmp));
//     assert(block_tmp == cb.blk());
//   }
//   // Process ready blocks (if any).
//   uint32_t block_idx = 0;
//   while (true) {
//     Block block;
//     if (!mgr->UseReady(&block)) break;
//     const CodedBlock& cb = cblocks[block_idx++];
//     (... process )
//   }
// }
//
// Decoding side:
// MyFrontMgrSubclass mgr;
// mgr.Init(...)
// while (!mgr.Done()) { // Not all blocks have been processed.
//    // Read sizes until we get a block ready to be processed.
//    Block block;
//    while (true) {
//      if (mgr.UseReady(&block)) break; // A block is ready!
//      // No block is ready, read block size from bitstream.
//      const BlockSize dim = ...
//      WP2_CHECK_ALLOC_OK(mgr.UseSize(dim, &block));
//    }
//   // ... use 'info.block'
// }
class FrontMgrDoubleOrderBase : public FrontMgrBase {
 public:
  // Allocates storage. Returns false in case of memory error.
  // Must be called first (once) before using the other methods.
  virtual WP2Status Init(PartitionSet partition_set, bool snapped,
                         uint32_t width, uint32_t height);

  PartitionSet GetPartitionSet() const { return partition_set_; }

  void Clear() override;

  // Sorts the given blocks in block order and fills 'size_order_indices' with
  // the indices of a permutation of 'blocks' that puts it in size order (order
  // in which block sizes are written).
  virtual WP2Status Sort(VectorNoCtor<Block>& blocks,
                         Vector_u16& size_order_indices) const = 0;

  // Returns the biggest possible next block to use in size order.
  // This may not be a BlockSize allowed by the partition set.
  Block GetMaxPossibleBlock() const { return next_; }
  // Returns the biggest allowed next block to use in size order.
  Block GetMaxFittingBlock() const;

  // Marks a block of a given size 'dim' as processed, size wise. The full
  // block is returned in 'block' if not nullptr.
  // If 'use_block' is true, the block is also marked as used, block pixel
  // wise. Otherwise, it is added to a stack of blocks to be popped by calling
  // UseReady(). Returns false in case of error.
  WP2_NO_DISCARD bool UseSize(BlockSize dim, Block* block,
                              bool use_block = false);

  // Finds a block whose size was recorded with UseSize(), but that hasn't been
  // used yet, and is ready to be used. Returns whether a block was found.
  bool UseReady(Block* block = nullptr);

  // For a valid 'size', returns true and the next block (pos) in size order.
  // Assumes 'size' is part of the 'partition_set_'.
  virtual WP2_NO_DISCARD bool TryGetNextBlock(BlockSize size,
                                              Block* block) const;

 protected:
  // Finds the position (min x/y coordinates and max size) of the next block
  // in size order.
  virtual Block FindNextBlock(const Block& last_block) const = 0;
  // Returns whether an element is ready to be used, i.e. its context
  // is considered filled at its maximum.
  virtual bool IsReady(const WP2::Block& block) const = 0;

  WP2Status CopyInternal(const FrontMgrDoubleOrderBase& other);

  PartitionSet partition_set_ = NUM_PARTITION_SETS;  // Undefined.
  bool snapped_ = false;
  // Stack of blocks whose size has been read but that haven't been marked used
  // yet. Blocks get appended when calling UseSize() and popped when calling
  // UseReady().
  VectorNoCtor<Block> size_stack_;
  FrontMgrBase occupancy_size_;
  // Next max possible block (for reading/writing block sizes).
  Block next_;
};

//------------------------------------------------------------------------------
// Chooses between a lexicographic or maximum top and left context front
// manager. For now, we only use one or the other (hence the typedef) but both
// could be used independently.

// Lexicographic front manager.
// Front manager for which the size and block order are the same.
class FrontMgrLexico : public FrontMgrDoubleOrderBase {
 public:
  WP2Status CopyFrom(const FrontMgrLexico& other);
  virtual void UndoUseSize(const Block& block);
  WP2Status Sort(VectorNoCtor<Block>& blocks,
                 Vector_u16& size_order_indices) const override;

 protected:
  bool IsReady(const WP2::Block& block) const override;
  Block FindNextBlock(const Block& last_block) const override;

 private:
  // Returns whether the 'Blocks()' are in the order they will be written.
  WP2Status CheckBlockList(const VectorNoCtor<Block>& blocks) const;
};

//------------------------------------------------------------------------------

// Front manager that maximizes the top and left context before writing the next
// block in size order.
class FrontMgrMax : public FrontMgrDoubleOrderBase {
 public:
  WP2Status CopyFrom(const FrontMgrMax& other);
  WP2_NO_DISCARD
  bool TryGetNextBlock(BlockSize size, Block* block) const override;
  WP2Status Sort(VectorNoCtor<Block>& blocks,
                 Vector_u16& size_order_indices) const override;

 protected:
  bool IsReady(const WP2::Block& block) const override;
  Block FindNextBlock(const Block& last_block) const override;

 private:
  bool DoIsReady(const FrontMgrBase& occupancy, const WP2::Block& block) const;
  // Returns true if a block is considered ready in the size occupancy.
  bool IsReadySize(const WP2::Block& block) const;
  // Returns whether the 'Blocks()' are in the order they will be written.
  WP2Status CheckBlockList(const VectorNoCtor<Block>& blocks) const;
};

// This is the FrontMgr class that should be used by default.
// TODO(vrabaud): Make sure the size order does not need anything outside the 32
//                lines of the cache for FrontMgrMax.
typedef FrontMgrMax FrontMgrDefault;

//------------------------------------------------------------------------------

// Generates all blocks lexicographically in the first WxH area, then all blocks
// in the second WxH area etc. (W and H being typically kMaxBlockSizePix).
class FrontMgrArea : public FrontMgrDoubleOrderBase {
 public:
  // BinaryPredicate used to sort blocks so that they are grouped by area.
  class Comp {
   public:
    // In kMinBlockSizePix.
    Comp(uint32_t tile_width, uint32_t area_width, uint32_t area_height);

    // Returns true if 'a' belongs to a lexicographically earlier area than 'b'
    // or if 'a' comes before 'b' in the same area.
    bool operator()(const Block& a, const Block& b) const;

   private:
    // Returns the unique index of the area whose 'block' belongs to.
    uint32_t GetAreaIndex(const Block& block) const;

    const uint32_t area_step_, area_width_, area_height_;
  };

  FrontMgrArea(uint32_t area_width, uint32_t area_height)  // In pixels.
      : area_width_(area_width / kMinBlockSizePix),
        area_height_(area_height / kMinBlockSizePix) {
    assert(area_width_ * kMinBlockSizePix == area_width);
    assert(area_height_ * kMinBlockSizePix == area_height);
  }
  // Only 'snapped=true' is allowed.
  WP2Status Init(PartitionSet partition_set, bool snapped, uint32_t width,
                 uint32_t height) override;

  WP2Status CopyFrom(const FrontMgrArea& other);
  WP2Status Sort(VectorNoCtor<Block>& blocks,
                 Vector_u16& size_order_indices) const override;

 protected:
  bool IsReady(const WP2::Block& block) const override;
  Block FindNextBlock(const Block& last_block) const override;

 protected:
  // Returns the next available block in the current area, or the first block in
  // the next area if the current area is full.
  void FindNextInArea(const FrontMgrBase& occupancy, uint32_t* bx,
                      uint32_t* by) const;

  const uint32_t area_width_, area_height_;  // In kMinBlockSizePix units.
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_UTILS_FRONT_MGR_H_
