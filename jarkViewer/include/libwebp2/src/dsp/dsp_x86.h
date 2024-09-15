// Copyright 2020 Google LLC
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
//   SSE helper function
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_DSP_DSP_X86_H_
#define WP2_DSP_DSP_X86_H_

#if defined(WP2_USE_SSE)

#include <assert.h>
#include <stdio.h>
#include <algorithm>

static inline __m128i Load128b(const void* const in) {
  return _mm_loadu_si128((const __m128i*)in);
}
static inline void Store128b(const __m128i v, void* const out) {
  _mm_storeu_si128((__m128i*)out, v);
}
static inline __m128i Load64b(const void* const in) {
  return _mm_loadl_epi64((const __m128i*)in);
}
static inline void Store64b(const __m128i v, void* const out) {
  _mm_storel_epi64((__m128i*)out, v);
}

// sign-extending 16b->32b load
static inline __m128i Load_s16_s32(const void* const in) {
  return _mm_cvtepi16_epi32(Load64b(in));
}
// saturating 32b->16b store
static inline void Store_s32_s16(const __m128i v, void* const out) {
  Store64b(_mm_packs_epi32(v, v), out);
}

// handy struct for holding N uint32_t words
template<uint32_t N>
struct VectorSSE {
 public:
  VectorSSE() = default;
  // some ctor can't be mark 'explicit' or you'll lose some coding flexibility
  VectorSSE(const void* const in) : v(Load128b(in)) {}  // NOLINT(explicit)
  VectorSSE(const __m128i a) : v(a) {}
  VectorSSE(int32_t a) : v(_mm_set1_epi32(a)) {}
  VectorSSE& operator=(const __m128i a) {
    v = a;
    return *this;
  }
  VectorSSE& operator=(int32_t a) {
    *this = VectorSSE(a);
    return *this;
  }
  operator __m128i() const { return v; }

  // arithmetic
  VectorSSE operator+(const __m128i a) const {
    return VectorSSE(_mm_add_epi32(v, a));
  }
  VectorSSE operator-(const __m128i a) const {
    return VectorSSE(_mm_sub_epi32(v, a));
  }
  // 32b x 32b -> 32b multiply
  VectorSSE operator*(const __m128i a) const {
    return VectorSSE(_mm_mullo_epi32(v, a));
  }
  // arithmetic right shift
  VectorSSE operator>>(uint32_t n) const {
    return VectorSSE(_mm_srai_epi32(v, n));
  }
  static constexpr VectorSSE Rounding(uint32_t n) {
    return VectorSSE(_mm_set1_epi32((1 << n) >> 1));
  }

  // I/O
  static WP2_NO_DISCARD VectorSSE Load(const void* const in) {
    return VectorSSE(in);
  }
  void Store(void* const out) const { Store128b(v, out); }

  static WP2_NO_DISCARD VectorSSE Load16(const void* const in) {
    return VectorSSE(Load_s16_s32(in));
  }
  void Store16(void* const out) const { Store_s32_s16(v, out); }

  // return size in 'step' units
  static uint32_t Size(uint32_t width) {
    assert((width % N) == 0);
    return width / N;
  }
  static constexpr uint32_t Step() { return N; }

 private:
  __m128i v;
};

typedef VectorSSE<4> VecI32_SSE;    // four 32bits-words

//------------------------------------------------------------------------------
// debugging

template<typename T>
void Print(const __m128i A, const char name[], int how_many = 32) {
  constexpr int size = 16 / sizeof(T);
  how_many = std::min(how_many, size);
  T tmp[size];
  _mm_storeu_si128((__m128i*)tmp, A);
  printf("%s :", name);
  for (int i = 0; i < how_many; ++i) {
    printf((size == 16) ? " %.2x" : (size == 8) ? " %.4x" : " %.8x", tmp[i]);
  }
  printf("\n");
}
static inline
void Print32b(const __m128i A, const char name[], int how_many = 4) {
  Print<uint32_t>(A, name, how_many);
}
static inline
void Print16b(const __m128i A, const char name[], int how_many = 8) {
  Print<uint16_t>(A, name, how_many);
}
static inline
void Print8b(const __m128i A, const char name[], int how_many = 16) {
  Print<uint8_t>(A, name, how_many);
}

#endif  // WP2_USE_SSE

//------------------------------------------------------------------------------

#endif  /* WP2_DSP_DSP_X86_H_ */
