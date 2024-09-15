// Copyright 2019 Google LLC
// Copyright 2018 The Abseil Authors.
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
// Pixel hash functions, copied from abseil's city hash.

#ifndef WP2_UTILS_HASH_H_
#define WP2_UTILS_HASH_H_

#include <cassert>
#include <cstdint>

//------------------------------------------------------------------------------
// Copied from abseil city.cc computing city hash.
// The char buffer has been replaced by a uint8_t one and styling changed.
// The endianness is checked with WP2_WORDS_BIGENDIAN (big-endian if defined,
// little-endian otherwise).

namespace WP2 {

namespace internal {

// Combines two hashes.
// Inspired by Boost's hash_combine with a uint64_t constant that represents the
// floating part of the golden ratio.
static inline uint64_t HashCombine(uint64_t h, uint64_t k) {
  return k ^ (h + 0x9e3779b97f4a7c15ull + (k << 6) + (k >> 2));
}

// Computes the hash of a buffer: taken from WebP.
static inline uint64_t HashPixImpl(const uint8_t* const color) {
  // Fast hash taken from WebP V1.
  const uint64_t kHashMul = 0x1e35a7bdull;
  const uint64_t color32 =
      (color[0] << 24) | (color[1] << 16) | (color[2] << 8) | (color[3]);
  return (color32 * kHashMul);
}

static inline uint32_t FinalizeHash(uint64_t hash, uint32_t hash_bits) {
  assert(hash_bits > 0);  // uint32_t >> 32 is undefined
  // Note that masking with 0xffffffffu is for preventing an
  // 'unsigned int overflow' warning. Doesn't impact the compiled code.
  return (uint32_t)(hash & 0xffffffffu) >> (32 - hash_bits);
}

}  // namespace internal

#ifdef WP2_WORDS_BIGENDIAN
#define SPLIT(X) (uint8_t)((uint16_t)argb[X] >> 8), (uint8_t)(argb[X] & 0xff)
#else
#define SPLIT(X) (uint8_t)(argb[X] & 0xff), (uint8_t)((uint16_t)argb[X] >> 8)
#endif

namespace encoding {
// Defines a 'hash_bits'-bit hash value of a pixel.
static inline uint32_t HashPix(const uint8_t* const argb, uint32_t hash_bits) {
  return internal::FinalizeHash(internal::HashPixImpl(argb), hash_bits);
}
static inline uint32_t HashPix(const uint16_t* const argb, uint32_t hash_bits) {
  const uint8_t s[8] = {SPLIT(0), SPLIT(1), SPLIT(2), SPLIT(3)};
  const uint64_t h1 = internal::HashPixImpl(s);
  const uint64_t h2 = internal::HashPixImpl(s + 4);
  const uint64_t h3 = internal::HashCombine(h1, h2);
  return internal::FinalizeHash(h3, hash_bits);
}
// Defines a 'hash_bits'-bit hash value of a pair of pixels.
static inline uint32_t HashPixPair(const int16_t argb[8], uint32_t hash_bits) {
  const uint8_t s[16] = {SPLIT(0), SPLIT(1), SPLIT(2), SPLIT(3),
                         SPLIT(4), SPLIT(5), SPLIT(6), SPLIT(7)};
  const uint64_t h1 = internal::HashPixImpl(s);
  const uint64_t h2 = internal::HashPixImpl(s + 4);
  const uint64_t h3 = internal::HashCombine(h1, h2);
  const uint64_t h4 = internal::HashPixImpl(s + 8);
  const uint64_t h5 = internal::HashPixImpl(s + 12);
  const uint64_t h6 = internal::HashCombine(h4, h5);
  const uint64_t h7 = internal::HashCombine(h3, h6);
  return internal::FinalizeHash(h7, hash_bits);
}
}  // namespace encoding
namespace decoding {
static inline uint32_t HashPix(const int16_t* const argb, uint32_t hash_bits) {
  const uint8_t s[8] = {SPLIT(0), SPLIT(1), SPLIT(2), SPLIT(3)};
  const uint64_t h1 = internal::HashPixImpl(s);
  const uint64_t h2 = internal::HashPixImpl(s + 4);
  const uint64_t h3 = internal::HashCombine(h1, h2);
  return internal::FinalizeHash(h3, hash_bits);
}

// Defines a 'hash_bits'-bit hash value of num_pixels pixels.
uint32_t HashPixN(const int16_t* argb, uint32_t num_pixels, uint32_t hash_bits);
}  // namespace decoding

#undef SPLIT

}  // namespace WP2

#endif  // WP2_UTILS_HASH_H_
