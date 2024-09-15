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
// Author: Vincent Rabaud (vincent.rabaud@google.com)

#ifndef WP2_COMMON_LOSSY_BLOCK_SIZE_H_
#define WP2_COMMON_LOSSY_BLOCK_SIZE_H_

#include <cassert>
#include <cstdint>

#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------
// Block characteristics

// Block types
typedef enum {
  BLK_4x4 = 0,
  BLK_8x4,
  BLK_16x4,
  BLK_4x8,
  BLK_8x8,
  BLK_16x8,
  BLK_32x8,
  BLK_4x16,
  BLK_8x16,
  BLK_16x16,
  BLK_32x16,
  BLK_8x32,
  BLK_16x32,
  BLK_32x32,
  BLK_LAST
} BlockSize;

// All the partition block sizes we support.
extern const BlockSize kAllBlockSizes[BLK_LAST];

// Returns the block's width/height in kMinBlockSizePix units.
extern const uint32_t BlockWidth[BLK_LAST];
extern const uint32_t BlockHeight[BLK_LAST];
// Returns the block's width/height in pixels.
static inline uint32_t BlockWidthPix(BlockSize size) {
  return BlockWidth[size] * kMinBlockSizePix;
}
static inline uint32_t BlockHeightPix(BlockSize size) {
  return BlockHeight[size] * kMinBlockSizePix;
}

// Returns the biggest BlockSize with dimensions at most 'width, height' in
// kMinBlockSizePix units.
BlockSize GetBlockSize(uint32_t width, uint32_t height);
// Returns the biggest BlockSize with dimensions at most 'width, height' snapped
// at position 'x, y' in kMinBlockSizePix units.
BlockSize GetSnappedBlockSize(uint32_t x, uint32_t y, uint32_t width,
                              uint32_t height);

// number of pixel for a given block size
extern uint32_t NumPix(BlockSize dim);

// number of kPredSize blocks for a given dimension
extern const uint32_t kNumBlocks[BLK_LAST];

// Table for printing block names (debug)
extern const char* const kDimNames[BLK_LAST + 1];

// Maximum number of sub blocks when splitting a block.
static constexpr uint32_t kMaxSplitBlocks = 4;

// Returns an arbitrary BlockSize that can be used to split 'size' into squares.
BlockSize GetSplitSize(BlockSize size, bool split);
uint32_t GetNumTransformsInBlock(BlockSize size, bool split);

// Pattern for splitting a block into at most 4 smaller blocks.
struct SplitPattern {
  uint32_t num_blocks;       // Number of blocks in this split.
  BlockSize block_sizes[4];  // Top-left,top-right,bottom-left,bottom-right
                             // (or subset in same order if less than 4).
};

//------------------------------------------------------------------------------
// Trf (=transform blocks) characteristics

// Possible transform blocks (taking reduce dimension into account)
// clang-format off
typedef enum {
  TRF_2x2 = 0, TRF_4x2,  TRF_8x2,
  TRF_2x4,     TRF_4x4,  TRF_8x4,  TRF_16x4,
  TRF_2x8,     TRF_4x8,  TRF_8x8,  TRF_16x8,  TRF_32x8,
               TRF_4x16, TRF_8x16, TRF_16x16, TRF_32x16,
                         TRF_8x32, TRF_16x32, TRF_32x32,
  TRF_LAST
} TrfSize;
// clang-format on

// returns the reduced dimension, or TRF_LAST if non-halvable
extern const TrfSize kHalfDim[BLK_LAST];
// convert from block-size to transform-size at full dimension
extern const TrfSize kFullDim[BLK_LAST];
static inline TrfSize GetTransform(BlockSize dim, bool reduced = false) {
  return reduced ? kHalfDim[dim] : kFullDim[dim];
}
// number of coefficients for a given transform size
extern const uint32_t kNumCoeffs[TRF_LAST];
extern const uint32_t TrfWidth[TRF_LAST];
extern const uint32_t TrfHeight[TRF_LAST];
extern const uint32_t TrfLog2[kMaxBlockSizePix + 1];  // log2(width/height)

// all transform sizes
extern const TrfSize kAllTrfSizes[TRF_LAST];
extern const char* const kTDimNames[TRF_LAST + 1];

//------------------------------------------------------------------------------

class YUVPlane;

// Simple block that can be used for anything.
class Block {
 public:
  Block() = default;
  Block(uint32_t x, uint32_t y, BlockSize dim) {
    SetXY(x, y);
    w_ = (dim == BLK_LAST) ? 0 : BlockWidth[dim];
    h_ = (dim == BLK_LAST) ? 0 : BlockHeight[dim];
    w_pix_ = w_ * kMinBlockSizePix;
    h_pix_ = h_ * kMinBlockSizePix;
    dim_ = dim;
  }
  inline bool operator<(const Block& other) const {
    return (y_ != other.y_) ? (y_ < other.y_) : (x_ < other.x_);
  }
  inline bool operator==(const Block& other) const {
    return (x_ == other.x_ && y_ == other.y_ && w_ == other.w_ &&
            h_ == other.h_);
  }
  inline bool operator!=(const Block& other) const {
    return !operator==(other);
  }
  void Draw(YUVPlane* yuv) const;  // for debugging

  // x, y coordinates and width height in pixel units.
  inline uint32_t x_pix() const { return x_pix_; }
  inline uint32_t y_pix() const { return y_pix_; }
  inline uint32_t w_pix() const { return w_pix_; }
  inline uint32_t h_pix() const { return h_pix_; }
  // (translated) bounding rectangle
  inline Rectangle AsRect(uint32_t offset_x = 0, uint32_t offset_y = 0) const {
    return {offset_x + x_pix_, offset_y + y_pix_, w_pix_, h_pix_};
  }
  // Returns a rectangle in pixels corresponding to the given sub transform
  // index 'tf_i' if 'split_tf' is true, or to the whole block otherwise.
  inline Rectangle TfRect(bool split_tf, uint32_t tf_i) const {
    const Rectangle local_rect = LocalTfRect(split_tf, tf_i);
    return {x_pix_ + local_rect.x, y_pix_ + local_rect.y, local_rect.width,
            local_rect.height};
  }
  // Returns a rectangle in pixels corresponding to the given sub transform
  // index 'tf_i' if 'split_tf' is true, or to the whole block otherwise,
  // relative to the block itself.
  inline Rectangle LocalTfRect(bool split_tf, uint32_t tf_i) const {
    const BlockSize split_size = GetSplitSize(dim(), split_tf);
    const uint32_t split_cols = w() / BlockWidth[split_size];
    const uint32_t split_w = BlockWidthPix(split_size);
    const uint32_t split_h = BlockHeightPix(split_size);
    return {tf_i % split_cols * split_w, tf_i / split_cols * split_h, split_w,
            split_h};
  }

  inline uint32_t x() const { return x_; }
  inline uint32_t y() const { return y_; }
  inline uint32_t w() const { return w_; }
  inline uint32_t h() const { return h_; }
  inline uint32_t area() const { return (w_ * h_); }
  inline Rectangle rect() const { return {x_, y_, w_, h_}; }

  inline BlockSize dim() const { return dim_; }

  // Setters.
  inline void SetXY(uint32_t x, uint32_t y) {
    x_ = x;
    x_pix_ = x * kMinBlockSizePix;
    y_ = y;
    y_pix_ = y * kMinBlockSizePix;
  }

  // Returns true if aligned in a quadtree-like manner.
  inline bool IsSnapped() const { return ((x_ % w_) == 0 && (y_ % h_) == 0); }

  // Returns true if the block is considered 'small' (one of the dimensions is
  // 4 pixels).
  inline bool IsSmall() const { return (w_ == 1 || h_ == 1); }

 private:
  // The members below are redundant but they are cached for speed.
  // coordinates/sizes in kMinBlockSizePix units
  uint32_t x_ = 0, y_ = 0;  // position in kMinBlockSizePix units
  uint32_t w_ = 0, h_ = 0;  // size, in kMinBlockSizePix units
  // coordinates/sizes in pixel units.
  uint32_t x_pix_ = 0, y_pix_ = 0;
  uint32_t w_pix_ = 0, h_pix_ = 0;
  BlockSize dim_ = BLK_LAST;
};

}  // namespace WP2

#endif /* WP2_COMMON_LOSSY_BLOCK_SIZE_H_ */
