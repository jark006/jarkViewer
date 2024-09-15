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
// Author: Vincent Rabaud (rabaud@google.com)
//

#ifndef WP2_ENC_LOSSLESS_BACKWARD_REFERENCES_H_
#define WP2_ENC_LOSSLESS_BACKWARD_REFERENCES_H_

#include <cassert>
#include <cstdint>
#include <memory>

#include "src/common/lossless/color_cache.h"
#include "src/common/symbols.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {
class SymbolRecorder;
}  // namespace WP2

namespace WP2L {

// -----------------------------------------------------------------------------
// PixelMode

// Class detailing what information a pixel contains.
class PixelMode {
 public:
  PixelMode() = default;

  static inline PixelMode CreateCopy(uint32_t offset, uint32_t len) {
    return {kSymbolTypeCopy, len, offset};
  }

  static inline PixelMode CreateColorCacheIdx(uint32_t idx, uint32_t range) {
    assert(idx < (1 << kMaxColorCacheBits));
    return {kSymbolTypeColorCacheIdx, 1, idx, range};
  }

  static inline PixelMode CreateSegmentCacheIdx(uint32_t len, uint32_t idx,
                                                uint32_t range) {
    assert(idx < (1 << kMaxSegmentCacheBits));
    return {kSymbolTypeSegmentCacheIdx, len, idx, range};
  }

  static inline PixelMode CreateDiscarded() {
    return {kSymbolTypeDiscarded, 0, 0, 0};
  }

  static inline PixelMode CreateLiteral(const int16_t* const argb) {
    return {kSymbolTypeLiteral, 1, argb};
  }

  inline SymbolType GetMode() const { return mode_; }

  // component 0,1,2,3 in order A,R,G,B.
  inline int16_t GetLiteral(int component) const {
    assert(mode_ == kSymbolTypeLiteral);
    return argb_or_offset_.argb[component];
  }

  inline uint32_t GetLength() const { return len_; }

  inline uint32_t GetColorCacheIdx() const {
    assert(mode_ == kSymbolTypeColorCacheIdx);
    assert(argb_or_offset_.cache_idx < (1U << kMaxColorCacheBits));
    return argb_or_offset_.cache_idx;
  }

  inline uint32_t GetSegmentCacheIdx() const {
    assert(mode_ == kSymbolTypeSegmentCacheIdx);
    assert(argb_or_offset_.cache_idx < (1U << kMaxSegmentCacheBits));
    return argb_or_offset_.cache_idx;
  }

  inline uint32_t GetColorCacheIdxRange() const {
    assert(mode_ == kSymbolTypeColorCacheIdx);
    return range_;
  }

  inline uint32_t GetSegmentCacheIdxRange() const {
    assert(mode_ == kSymbolTypeSegmentCacheIdx);
    return range_;
  }

  inline uint32_t GetOffset() const {
    assert(mode_ == kSymbolTypeCopy);
    return argb_or_offset_.offset;
  }

  inline void SetOffset(uint32_t offset) {
    assert(mode_ == kSymbolTypeCopy);
    argb_or_offset_.offset = offset;
  }

 private:
  PixelMode(SymbolType mode, uint32_t len, uint32_t offset, uint32_t range = 0)
      : mode_(mode), len_(len), range_(range) {
    argb_or_offset_.offset = offset;
  }
  PixelMode(SymbolType mode, uint16_t len, const int16_t* const argb)
      : mode_(mode), len_(len), range_(0) {
    for (uint32_t i = 0; i < 4; ++i) {
      argb_or_offset_.argb[i] = argb[i];
    }
  }

  SymbolType mode_;
  uint32_t len_;
  union {
    int16_t argb[4];
    uint32_t offset;
    uint32_t cache_idx;
  } argb_or_offset_;  // Or index in color cache or index in segment cache.
  uint32_t range_;    // Range of 'argb_or_offset_' (currently used for cache).
};

// -----------------------------------------------------------------------------
// WP2LHashChain

class HashChain {
 public:
  // Must be called first, to set size.
  // This is the maximum size of the hash_chain that can be constructed.
  // Typically this is the pixel count (width x height) for a given image.
  WP2_NO_DISCARD
  bool Allocate(uint32_t size);
  // Pre-compute the best matches for argb.
  WP2Status Fill(int effort, const int16_t* argb, uint32_t width,
                 uint32_t height);

  // Contains the offset and length at which the best match is found.
  struct OffsetLength {
    uint32_t offset;
    uint32_t length;
  };
  WP2::VectorNoCtor<OffsetLength> offset_length_;

 private:
  // Updates the hash chain linking a pixel at 'pos' to its predecessor with the
  // same hash defined by 'pixel_pair'.
  void UpdateChain(uint32_t pos, const int16_t* pixel_pair,
                   WP2::Vector_s32* hash_to_first_index, int32_t* chain);

  static constexpr uint8_t kHashBits = 18;
  static constexpr uint32_t kHashSize = (1u << kHashBits);
};

// -----------------------------------------------------------------------------
// WP2LBackwardRefs (block-based backward-references storage)

// maximum number of reference blocks the image will be segmented into
#define MAX_REFS_BLOCK_PER_IMAGE 16

struct PixOrCopyBlock {
  std::shared_ptr<PixOrCopyBlock> next;  // next block (or nullptr)
  WP2::VectorNoCtor<PixelMode> modes;    // pixel modes
  int size;                              // currently used size
};
class RefsCursor;
class BackwardRefsPool;

// Container for blocks chain
class BackwardRefs {
 public:
  BackwardRefs()
      : block_size_(0),
        refs_(nullptr),
        tail_(nullptr),
        free_blocks_(nullptr),
        last_block_(nullptr),
        pool_(nullptr) {}
  ~BackwardRefs() { Reset(); }
  WP2Status CopyFrom(const BackwardRefs& refs);
  // Releases memory for backward references.
  void Reset();
  void Clear();
  WP2Status CursorAdd(const PixelMode& v);
  // Returns true if the path covers "num_pixels" pixels.
  bool IsValid(uint32_t num_pixels) const;

  friend RefsCursor;        // To access refs_
  friend BackwardRefsPool;  // To access Init

 private:
  // Initialize the object. 'block_size' is the common block size to store
  // references (typically, width * height / MAX_REFS_BLOCK_PER_IMAGE).
  void Init(uint32_t block_size, BackwardRefsPool* pool);
  WP2Status AddBlock();
  static void FreeFromPool(BackwardRefs* refs);

  // minimum block size for backward references
  static constexpr uint32_t kMinBlockSize = 256;

  int block_size_;                         // common block-size
  std::shared_ptr<PixOrCopyBlock> refs_;   // list of currently used blocks
  std::shared_ptr<PixOrCopyBlock>* tail_;  // for list recycling
  std::shared_ptr<PixOrCopyBlock> free_blocks_;  // free-list
  // used for adding new refs (internal)
  std::shared_ptr<PixOrCopyBlock> last_block_;
  BackwardRefsPool* pool_;  // pool to which it belongs
};

// Class holding several allocated BackwardRefs.
// As we compare several BackwardRefs but only keep the best one, this
// class is useful not to keep reallocating BackwardRefs.
class BackwardRefsPool {
 public:
  // Initialize the BackwardRefs.
  void Init(uint32_t num_pixels) {
    // We round the block size up, so we're guaranteed to have
    // at most MAX_REFS_BLOCK_PER_IMAGE blocks used:
    const uint32_t block_size = (num_pixels - 1) / MAX_REFS_BLOCK_PER_IMAGE + 1;
    for (uint32_t i = 0; i < kNumBackwardRefs; ++i) {
      refs_[i].Reset();
      refs_[i].Init(block_size, this);
      is_used_[i] = false;
    }
  }
  typedef std::unique_ptr<BackwardRefs, void (*)(BackwardRefs*)> RefsPtr;
  static RefsPtr GetEmptyBackwardRefs() {
    return RefsPtr(nullptr, BackwardRefs::FreeFromPool);
  }
  RefsPtr GetFreeBackwardRefs() {
    // Send back the first BackwardRefs that is not used.
    for (uint32_t i = 0; i < kNumBackwardRefs; ++i) {
      if (!is_used_[i]) {
        is_used_[i] = true;
        return RefsPtr(&refs_[i], BackwardRefs::FreeFromPool);
      }
    }
    assert(false);
    return RefsPtr(nullptr, BackwardRefs::FreeFromPool);
  }
  // Reset the unused backward refs.
  void Reset() {
    for (uint32_t i = 0; i < kNumBackwardRefs; ++i) {
      if (!is_used_[i]) refs_[i].Reset();
    }
  }
  friend BackwardRefs;

 private:
  void Release(BackwardRefs* const refs) {
    for (uint32_t i = 0; i < kNumBackwardRefs; ++i) {
      if (&refs_[i] == refs) {
        assert(is_used_[i]);
        is_used_[i] = false;
        return;
      }
    }
    assert(false);
  }
  // 5 is enough in the code for now: 1 as the chosen backward ref between
  // iterations of the cruncher, 1 best per iteration, and 2 temps in addition
  // in GetBackwardReferences
  static constexpr uint32_t kNumBackwardRefs = 5;
  BackwardRefs refs_[kNumBackwardRefs];
  bool is_used_[kNumBackwardRefs];
};

// Cursor for iterating on references content
class RefsCursor {
 public:
  // Positions the cursor at the beginning of the references list.
  explicit RefsCursor(const BackwardRefs& refs);
  // Returns true if cursor is pointing at a valid position.
  inline bool Ok() const { return (cur_pos_ != nullptr); }
  inline void Next() {
    assert(Ok());
    if (++cur_pos_ == last_pos_) NextBlock();
  }

  PixelMode* cur_pos_;  // current position
 private:
  // Move to next block of references.
  void NextBlock();

  std::shared_ptr<PixOrCopyBlock> cur_block_;  // current block in the refs list
  const PixelMode* last_pos_;  // sentinel for switching to next block
};

// -----------------------------------------------------------------------------
// Main entry points

enum LZ77Type {
  kLZ77Standard = 1,
  kLZ77RLE = 2,
  // LZ77 where matches are forced to happen within a given distance cost.
  kLZ77Box = 4,
  kLZ77None = 8
};

// Evaluates best possible backward references for specified effort.
// The input cache_bits to 'GetBackwardReferences' sets the maximum cache
// bits to use (passing 0 implies disabling the local color cache).
// The optimal cache bits is evaluated and set for the *cache_bits parameter.
// The return value is the pointer to the best of the two backward refs viz,
// refs[0] or refs[1].
WP2Status GetBackwardReferences(
    uint32_t width, uint32_t height, const int16_t* argb, int effort,
    int lz77_types_to_try, uint32_t cache_bits_max, const HashChain& hash_chain,
    const LosslessSymbolsInfo& symbols_info,
    WP2::SymbolRecorder* symbol_recorder, CacheConfig* cache_config,
    BackwardRefsPool* ref_pool, BackwardRefsPool::RefsPtr* refs);
}  // namespace WP2L

#endif  // WP2_ENC_LOSSLESS_BACKWARD_REFERENCES_H_
