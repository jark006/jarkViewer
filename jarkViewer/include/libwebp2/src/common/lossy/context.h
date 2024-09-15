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
//   Contexted predictions and syntax I/O
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_LOSSY_CONTEXT_H_
#define WP2_COMMON_LOSSY_CONTEXT_H_

#include <cassert>
#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/symbols.h"
#include "src/utils/ans.h"
#include "src/utils/ans_enc.h"
#include "src/utils/front_mgr.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

class CodedBlockBase;
class CodedBlock;
class SymbolManager;
class SymbolRecorder;
class SymbolWriter;
class SymbolReader;

// Class able to give the context around a given rectangular area.
// Type "T" should be a POD like uint8_t or int16_t.
template <typename T>
class ContextImpl {
 public:
  ContextImpl() = default;

  // 'data' is an image representing the blocks of width 'width'.
  void Init(VectorNoCtor<T>* data, uint32_t width, uint32_t height) {
    assert(height >= kMaxBlockSize + 1);  // minimum cache height
    data_ = data;
    width_ = width;
    height_ = height;
  }
  // Set a value in a specific Block in the original 'data'.
  void Set(const Block& cb, T val) {
    uint32_t Y = cb.y() % height_;
    for (uint32_t y = 0; y < cb.h(); ++y) {
      T* const data = &(*data_)[Y * width_ + cb.x()];
      std::fill(data, data + cb.w(), val);
      Y = (Y + 1) % height_;
    }
  }
  T Get(uint32_t x, uint32_t y) const {
    return (*data_)[(y % height_) * width_ + x];
  }
  // Reads around a CodedBlock and fill the values in the context.
  // The context should have a size max of kContextSize.
  uint32_t GetContext(const Block& cb, int8_t y_left_occ, int8_t y_right_occ,
                      T context[]) const {
    return GetContext(cb.x(), cb.y(), cb.w(), y_left_occ, y_right_occ, context);
  }

  // At a block of coordinates 'x' and 'y', compute the context using
  // the neighboring blocks:
  // - 'width' + 2 on top from ('x'-1,'y'-1) to ('x'+width,'y'-1)
  // - 'left_occ' on the left, from ('x'-1,'y') to ('x','y'+'left_occ')
  // - 'right_occ' on the left, from ('x'-1,'y') to ('x','y'+'right_occ')
  // 'left_occ' and 'right_occ' can be negative.
  // Returns the number of context values filled.
  uint32_t GetContext(uint32_t X, uint32_t Y, uint32_t width, int left_occ,
                      int right_occ,
                      T context[/*width + 2 + left_occ + right_occ*/]) const {
    uint32_t m = 0;
    // Add the left contribution.
    uint32_t x_start = X, x_end = X + width - 1;
    if (left_occ > 0) {
      assert(X > 0);
      x_start -= 1;
      for (uint32_t y = Y; y < Y + (uint32_t)left_occ; ++y) {
        context[m++] = (*data_)[(y % height_) * width_ + X - 1];
      }
    }
    // if available, include top-right below
    if (right_occ >= 0) ++x_end;
    // Add the right contribution.
    if (right_occ > 0) {
      assert(x_end < width_);
      for (uint32_t y = Y; y < Y + (uint32_t)right_occ; ++y) {
        context[m++] = (*data_)[(y % height_) * width_ + x_end];
      }
    }
    // Add the top contribution.
    if (Y > 0) {
      for (uint32_t x = x_start; x <= x_end; ++x) {
        context[m++] = (*data_)[((Y - 1) % height_) * width_ + x];
      }
    }
    return m;
  }

 private:
  // externally allocated data.
  VectorNoCtor<T>* data_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;  // cache height. Doesn't have to be picture's height
};

// Generic predictor for block level values, based on values of the
// surrounding blocks.
class BlockContextPredictor {
 public:
  BlockContextPredictor() = default;

  WP2Status Init(uint8_t symbol, uint32_t symbol_range, uint32_t width);

  // Deep copy.
  WP2Status CopyFrom(const BlockContextPredictor& other);

  void WriteValue(const CodedBlockBase& cb, uint8_t value, SymbolManager* sm,
                  ANSEncBase* enc, WP2_OPT_LABEL) const;
  uint8_t ReadValue(const CodedBlockBase& cb, SymbolReader* sr, WP2_OPT_LABEL);
  // Updates the context for writing/reading values for following blocks.
  void SetValue(const CodedBlockBase& cb, uint8_t value);

 private:
  static constexpr uint32_t kMaxRange = 8;
  uint8_t symbol_ = 0;
  uint32_t range_ = 0;

  Vector_u8 map_;
  ContextImpl<uint8_t> ctxt_;

 private:
  // Given a CodedBlock, returns in 'context' possible values sorted by
  // frequency in neighboring blocks. 'context' must have a size of 'range_'.
  void GetContext(const CodedBlockBase& cb, uint8_t* context) const;
  // Returns the slot where the coded-block's value is to be found in context[].
  uint32_t GetSlot(const CodedBlockBase& cb, uint8_t id) const;
};

//------------------------------------------------------------------------------
// Segment id contexting

// This class is used for predicting and storing segment-id.
class SegmentIdPredictor {
 public:
  SegmentIdPredictor() = default;

  void InitInitialSegmentId(const CodedBlockBase& cb, uint32_t id);

  // Deep copy.
  WP2Status CopyFrom(const SegmentIdPredictor& other);

  // Called first.
  WP2Status InitWrite(uint32_t num_segments, bool explicit_segment_ids,
                      uint32_t width);
  // Called before the writing pass.
  WP2Status WriteHeader(ANSEncBase* enc) const;
  // Called for each block.
  void WriteId(const CodedBlockBase& cb, SymbolManager* sm,
               ANSEncBase* enc) const;
  void RecordId(const CodedBlockBase& cb);

  // Initializes the reader.
  WP2Status ReadHeader(ANSDec* dec, uint32_t num_segments,
                       bool explicit_segment_ids, uint32_t width);
  // Reads and records cb->id_. Called for each block.
  void ReadId(SymbolReader* sr, CodedBlockBase* cb);

  static uint8_t GetIdFromSize(uint32_t num_segments, BlockSize block_size);

 private:
  bool explicit_segment_ids_ = false;
  uint32_t num_segments_ = 0;

  BlockContextPredictor predictor_;
};

//------------------------------------------------------------------------------
// Alpha mode predictor.

class AlphaModePredictor {
 public:
  AlphaModePredictor() = default;

  WP2Status Init(uint32_t width, uint32_t height);
  WP2Status CopyFrom(const AlphaModePredictor& other);
  void Write(const CodedBlockBase& cb, ANSEncBase* enc, SymbolManager* sw);
  void Update(const CodedBlockBase& cb);
  BlockAlphaMode Read(const CodedBlockBase& cb, SymbolReader* sr);

 private:
  BlockContextPredictor predictor_;
};

//------------------------------------------------------------------------------
// Diffusion error map

class DCDiffusionMap {
 public:
  // Converts from EncoderConfig::error_diffusion range to DCDiffusionMap range.
  static uint32_t GetDiffusion(uint32_t error_diffusion) {
    return error_diffusion * 255 / 100;
  }

  WP2Status Init(uint32_t width);  // dimension in kMinBlockSizePix units
  WP2Status CopyFrom(const DCDiffusionMap& other);

  void Clear();  // re-initialize the errors to zero
  void Store(const FrontMgrBase& mgr, const Block& blk, int16_t error);
  // Returns the error propagated from surrounding blocks, weighted by
  // 'strength' which is in range [0..255]. The error value is for the whole
  // block, not per pixel.
  int16_t Get(const Block& blk, uint32_t strength) const;

 private:
  static const uint32_t kErrorHeight = kMaxBlockSize + 1;
  Vector_f error_[kErrorHeight];
  uint32_t max_row_ = 0;  // max row (inclusive) for which we have error stored
};

}  // namespace WP2

#endif /* WP2_COMMON_LOSSY_CONTEXT_H_ */
