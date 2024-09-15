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
// Bitstream/chunk header reader and writer.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_HEADER_ENC_DEC_H_
#define WP2_COMMON_HEADER_ENC_DEC_H_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

#include "src/utils/ans.h"
#include "src/utils/data_source.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Returns true if 'value' is successfully read, false otherwise.
// 'value' is expected to be in [min_value:max_value].
WP2Status TryReadVarInt(DataSource* data_source, uint32_t min_value,
                        uint32_t max_value, uint32_t* value);

// Encodes 'value' in [min_value:max_value] into a variable length integer.
// 'max_value' cannot exceed kMaxVarInt.
// 'dst' must be at least GetMaxVarIntLength(min_value, max_value) long.
uint32_t WriteVarInt(uint32_t value, uint32_t min_value, uint32_t max_value,
                     uint8_t dst[]);

// Returns the maximum number of bytes used by an integer of variable length,
// given 'min/max_value'.
constexpr uint32_t GetMaxVarIntLength(uint32_t min_value, uint32_t max_value) {
  return (min_value >= max_value) ? 0
         : (max_value - min_value <= 0xFFu)
             ? 1
             : (1 + GetMaxVarIntLength(0, ((max_value - min_value) >> 7) - 1));
}

//------------------------------------------------------------------------------

// Class for reading small headers or raw bits.
class BitUnpacker {
 public:
  BitUnpacker(DataSource* const data_source, const char debug_prefix[])
      : data_source_(data_source),
        byte_(0),
        bit_pos_(8),
        status_(WP2_STATUS_OK),
        debug_prefix_(debug_prefix) {
    (void)debug_prefix_;
  }

  // Reads a value in [0:2^num_bits-1]. 'num_bits' must be in range [1, 24].
  uint32_t ReadBits(uint32_t num_bits, WP2_OPT_LABEL);

  // Reads a signed value in [-2^(num_bits-1):2^(num_bits-1)-1].
  int32_t ReadSBits(uint32_t num_bits, WP2_OPT_LABEL);

  // Reads a variable-size value in [min_value:max_value].
  // Previously read bits must align to full bytes.
  uint32_t ReadVarUInt(uint32_t min_value, uint32_t max_value, WP2_OPT_LABEL);

  // Reads the four corners of a non-empty rectangle fitting in 'canvas_*'.
  // Previously read bits must align to full bytes.
  Rectangle ReadRect(uint32_t canvas_width, uint32_t canvas_height,
                     WP2_OPT_LABEL);

  // Reads bits until the next aligned byte.
  // Returns true if there were only zeros.
  bool Pad();

  // Tries to fetch the next 'num_bits' if not already available.
  // Returns true if the next 'num_bits' can be immediately read afterwards.
  bool Prefetch(uint32_t num_bits);

  // Should be checked before using values extracted with the above functions.
  WP2Status GetStatus() const { return status_; }

 private:
  // Returns the number of bytes necessary to extract the next 'num_bits'.
  uint32_t GetMinNumBytes(uint32_t num_bits) const;

  // Reads the next 'num_bits'.
  uint32_t ReadBitsInternal(uint32_t num_bits);

  DataSource* const data_source_;  // Input bitstream.
  uint8_t byte_;                   // Last byte read by the 'data_source_'.
  uint32_t bit_pos_;  // Number of already used bits in 'byte_' in [1:8].

  WP2Status status_;
  const char* const debug_prefix_;
};

//------------------------------------------------------------------------------

// Class for writing small headers or raw bits.
class BitPacker {
 public:
  BitPacker(uint8_t* buf, uint32_t size, const char debug_prefix[])
      : buf_(buf), bit_pos_(0), error_(false), debug_prefix_(debug_prefix) {
    (void)debug_prefix_;
    // Check for potential overflows.
    max_pos_ = (uint32_t)std::min(
        (uint64_t)size * 8, (uint64_t)std::numeric_limits<uint32_t>::max());
    memset(buf, 0, size);
  }
  // Write next bits. 'num_bits' must be in range [1, 24].
  void PutBits(uint32_t value, uint32_t num_bits, WP2_OPT_LABEL);

  // Writes a signed 'value' in [-2^(num_bits-1):2^(num_bits-1)-1].
  void PutSBits(int32_t value, uint32_t num_bits, WP2_OPT_LABEL);

  // Writes a variable-size 'value' in [min_value:max_value].
  // Previously written bits must align to full bytes.
  void PutVarUInt(uint32_t value, uint32_t min_value, uint32_t max_value,
                  WP2_OPT_LABEL);

  // Writes the four corners of a non-empty rectangle fitting in 'canvas_*'.
  // It is expected that the most common rectangles are big.
  // Previously written bits must align to full bytes.
  void PutRect(const Rectangle& rect, uint32_t canvas_width,
               uint32_t canvas_height, WP2_OPT_LABEL);

  // Writes binary zeros until the next aligned byte.
  void Pad();

  // Size used so far, in bytes.
  uint32_t Used() const { return (bit_pos_ + 7) >> 3; }

  // Should be checked before using the above methods.
  bool Ok() const { return !error_; }

 private:
  void PutBitsInternal(uint32_t value, uint32_t num_bits);

  uint8_t* const buf_;  // Output bitstream.
  uint32_t max_pos_;    // Maximum number of bits that can be written to 'buf_'.
  uint32_t bit_pos_;    // Number of bits written to 'buf_'.
  bool error_;
  const char* const debug_prefix_;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_COMMON_HEADER_ENC_DEC_H_
