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
// Convenience color types and conversion functions.
//
// Authors: Yannis Guyon (yguyon@google.com)

#ifndef WP2_COMMON_COLOR_PRECISION_H_
#define WP2_COMMON_COLOR_PRECISION_H_

#include <cassert>
#include <cstdint>

#include "src/dsp/math.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------
// Color structs

// Y, Co, Cg are not literally premultiplied by alpha but the RGB equivalent is.
struct AYCoCg19b {
  static constexpr uint8_t kAlphaMax = 1u;
  static constexpr uint8_t kYCoCgBitDepth = 6;
  static constexpr uint8_t kYCoCgMax = (1u << kYCoCgBitDepth) - 1u;
  uint8_t a;          // Precision:  1 bit
  uint8_t y, co, cg;  // Precision:  6 bits
};

//------------------------------------------------------------------------------

namespace Internal {

template <typename TypeIn, int NumBitsIn, typename TypeOut, int NumBitsOut>
constexpr TypeOut Quantize(TypeIn x) {
  static_assert(NumBitsIn > NumBitsOut, "Cannot quantize to wider range");
  return (TypeOut)(x >> (NumBitsIn - NumBitsOut));
}

template <typename TypeIn, int NumBitsIn, typename TypeOut, int NumBitsOut>
constexpr TypeOut Dequantize(TypeIn x) {
  static_assert(NumBitsIn < NumBitsOut, "Cannot dequantize to smaller range");
  static_assert(2 * NumBitsIn >= NumBitsOut,
                "Cannot dequantize more than twice the input range");
  return (TypeOut)(((uint32_t)x << (NumBitsOut - NumBitsIn)) |
                   ((uint32_t)x >> (2 * NumBitsIn - NumBitsOut)));
}

}  // namespace Internal

//------------------------------------------------------------------------------
// Argb32b <-> uint32_t

constexpr uint32_t ToUInt32(Argb32b color) {
  return ((uint32_t)color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
}

constexpr Argb32b ToArgb32b(uint32_t color) {
  return {(uint8_t)((color >> 24) & 0xFFu), (uint8_t)((color >> 16) & 0xFFu),
          (uint8_t)((color >> 8) & 0xFFu), (uint8_t)((color >> 0) & 0xFFu)};
}

//------------------------------------------------------------------------------
// Argb32b <-> uint8_t

inline void ToUInt8(Argb32b color, uint8_t argb[4]) {
  argb[0] = color.a;
  argb[1] = color.r;
  argb[2] = color.g;
  argb[3] = color.b;
}

inline Argb32b ToArgb32b(const uint8_t argb[4]) {
  return {argb[0], argb[1], argb[2], argb[3]};
}

//------------------------------------------------------------------------------
// RGB12b <-> uint32_t

constexpr uint32_t ToUInt32(RGB12b color) {
  return (((uint32_t)color.r & 0xFu) << 8) | (((uint32_t)color.g & 0xFu) << 4) |
         (((uint32_t)color.b & 0xFu) << 0);
}

constexpr RGB12b ToRGB12b(uint32_t color) {
  return {(uint8_t)((color >> 8) & 0xFu), (uint8_t)((color >> 4) & 0xFu),
          (uint8_t)((color >> 0) & 0xFu)};
}

//------------------------------------------------------------------------------
// Argb32b <-> Argb38b

constexpr Argb38b ToArgb38b(Argb32b color) {
  return {color.a, Internal::Dequantize<uint8_t, 8, uint16_t, 10>(color.r),
          Internal::Dequantize<uint8_t, 8, uint16_t, 10>(color.g),
          Internal::Dequantize<uint8_t, 8, uint16_t, 10>(color.b)};
}

constexpr Argb32b ToArgb32b(Argb38b color) {
  return {color.a, Internal::Quantize<uint16_t, 10, uint8_t, 8>(color.r),
          Internal::Quantize<uint16_t, 10, uint8_t, 8>(color.g),
          Internal::Quantize<uint16_t, 10, uint8_t, 8>(color.b)};
}

//------------------------------------------------------------------------------
// Argb32b <-> RGB12b

constexpr RGB12b ToRGB12b(Argb32b color) {
  return (color.a == 0) ? RGB12b{0xFu, 0xFu, 0xFu}
                        : RGB12b{Internal::Quantize<uint32_t, 8, uint8_t, 4>(
                                     color.r * WP2::kAlphaMax / color.a),
                                 Internal::Quantize<uint32_t, 8, uint8_t, 4>(
                                     color.g * WP2::kAlphaMax / color.a),
                                 Internal::Quantize<uint32_t, 8, uint8_t, 4>(
                                     color.b * WP2::kAlphaMax / color.a)};
}

constexpr Argb32b ToArgb32b(RGB12b color) {
  return {WP2::kAlphaMax, Internal::Dequantize<uint8_t, 4, uint8_t, 8>(color.r),
          Internal::Dequantize<uint8_t, 4, uint8_t, 8>(color.g),
          Internal::Dequantize<uint8_t, 4, uint8_t, 8>(color.b)};
}

//------------------------------------------------------------------------------
// Argb32b <-> AYCoCg19b (stable after three conversions)
// Does not use Quantize() functions to limit max loss from RGB per channel to 7

constexpr AYCoCg19b ToAYCoCg19b(Argb32b color) {
  return {(uint8_t)(color.a >> 7),
          (uint8_t)(((uint32_t)1u + color.g * 2 + color.r + color.b) >> 4),
          (uint8_t)(((uint32_t)256u + color.r - color.b) >> 3),
          (uint8_t)(((uint32_t)512u + color.g * 2 - color.r - color.b) >> 4)};
}

constexpr Argb32b ToArgb32b(AYCoCg19b color) {
  return {(uint8_t)((color.a) ? WP2::kAlphaMax : 0u),
          (uint8_t)Clamp(((int32_t)color.y - color.cg + color.co) * 4, 0,
                         (int32_t)WP2::kAlphaMax),
          (uint8_t)Clamp(((int32_t)color.y + color.cg) * 4 - 128, 0,
                         (int32_t)WP2::kAlphaMax),
          (uint8_t)Clamp(((int32_t)color.y - color.cg - color.co) * 4 + 256, 0,
                         (int32_t)WP2::kAlphaMax)};
}

//------------------------------------------------------------------------------

inline bool CheckPremultiplied(Argb32b color) {
  return (color.r <= color.a && color.g <= color.a && color.b <= color.a);
}

inline bool CheckPremultiplied(Argb38b color) {
  const uint16_t alpha_10b =
      Internal::Dequantize<uint8_t, 8, uint16_t, 10>(color.a);
  return (color.r <= alpha_10b && color.g <= alpha_10b && color.b <= alpha_10b);
}

//------------------------------------------------------------------------------

// Returns the maximum value for a given channel.
// 'channel' is the index of the channel in order ARGB.
static inline uint64_t FormatMax(WP2SampleFormat format, uint32_t channel) {
  if (format == WP2_Argb_32 || channel == 0) {
    return WP2::kAlphaMax;
  } else if (format == WP2_ARGB_32) {
    return 255;
  } else if (format == WP2_Argb_38) {
    return (channel == 0) ? WP2::kAlphaMax : 1023;
  }
  assert(false);
  return 0;
}

}  // namespace WP2

#endif  // WP2_COMMON_COLOR_PRECISION_H_
