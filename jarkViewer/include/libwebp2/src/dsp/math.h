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
// Misc. common math functions
//
// Authors: vrabaud (vincent.rabaud@google.com)
//

#ifndef WP2_DSP_MATH_H_
#define WP2_DSP_MATH_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <type_traits>

#include "src/dsp/dsp.h"

const uint32_t kLogLookupIdxMax = 256;

extern const float kWP2Log2Table[kLogLookupIdxMax];
extern const float kWP2SLog2m1Table[kLogLookupIdxMax];

typedef float (*WP2Log2SlowFunc)(uint32_t v);
extern WP2Log2SlowFunc WP2Log2Slow;
extern WP2Log2SlowFunc WP2SLog2m1Slow;

// Fast calculation of log2(v) for integer input.
static inline float WP2Log2(uint32_t v) {
  return (v < kLogLookupIdxMax) ? kWP2Log2Table[v] : WP2Log2Slow(v);
}
// Fast calculation of v * (1 - log2(v)) for integer input.
static inline float WP2SLog2m1(uint32_t v) {
  return (v < kLogLookupIdxMax) ? kWP2SLog2m1Table[v] : WP2SLog2m1Slow(v);
}

// fast look-up version for small values
static inline float WP2Log2Fast(uint32_t v) {
  assert(v < kLogLookupIdxMax);
  return kWP2Log2Table[v];
}

// Returns (int)floor(log2(n)).
// use GNU builtins where available.
#if defined(__GNUC__)
static inline int WP2Log2Floor(uint32_t n) {
  return (n == 0) ? 0 : (31 ^ __builtin_clz(n));
}
// counts the number of trailing zero
static inline int WP2Ctz(uint32_t n) { return __builtin_ctz(n); }
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)

static inline int WP2Log2Floor(uint32_t n) {
  if (n == 0) return 0;
  unsigned long first_set_bit;  // NOLINT (runtime/int)
  _BitScanReverse(&first_set_bit, n);
  return first_set_bit;
}
static inline int WP2Ctz(uint32_t n) {
  unsigned long first_set_bit;  // NOLINT (runtime/int)
  _BitScanForward(&first_set_bit, n);
  return first_set_bit;
}
#else  // default: use the C-version.
#define WP2_NEED_LOG_TABLE_8BIT
extern const uint8_t WP2LogTable8bit[256 + 1];
static inline int WP2Log2Floor(uint32_t n) {
  int log_value = 0;
  while (n > 256) {
    log_value += 8;
    n >>= 8;
  }
  return log_value + WP2LogTable8bit[n];
}
static inline int WP2Ctz(uint32_t n) {
  for (uint32_t i = 0; i < 32; ++i, n >>= 1) {
    if (n & 1) return i;
  }
  return 32;
}
#endif

// To be used by constants and assertions.
// Returns floor(log2(n)).
static constexpr uint32_t WP2Log2Floor_k(uint64_t n) {
  return (n < 2)     ? 0
         : (n < 4)   ? 1
         : (n < 8)   ? 2
         : (n < 16)  ? 3
         : (n < 32)  ? 4
         : (n < 64)  ? 5
         : (n < 128) ? 6
         : (n < 256) ? 7
                     : (8u + WP2Log2Floor_k(n >> 8));
}
// Returns ceil(log2(n)).
static constexpr uint32_t WP2Log2Ceil_k(uint64_t n) {
  return (n <= 1) ? 0 : (1 + WP2Log2Floor_k(n - 1));
}

// Only use for small values, smaller than 0xffff.
typedef int32_t (*WP2InnerProductFunc)(const int16_t* const a,
                                       const int16_t* const b, size_t size);
extern WP2InnerProductFunc WP2InnerProduct;

// Must be called before calling any of the above methods.
void WP2MathInit();

//------------------------------------------------------------------------------

namespace WP2 {

// Returns the closest value to 'v' in [min:max].
template <typename T>
static constexpr T Clamp(T v, T min, T max) {
  return (v < min) ? min : (v > max) ? max : v;
}
// Returns the closest value to 'v' fitting in 'num_bits' (sign included).
template <typename T>
static inline T ClampToSigned(T v, uint32_t num_bits) {
  static_assert(std::numeric_limits<T>::min() < 0, "T must be signed");
  assert(num_bits > 0 && num_bits < sizeof(T) * 8);
  const T limit = (T)1 << (num_bits - 1);
  return Clamp<T>(v, -limit, limit - 1);
}

// Returns the closest value to 'a + b' in [min:max], for under/overflow check.
template <typename T>
static constexpr T SafeAdd(T a, T b, T min = std::numeric_limits<T>::min(),
                           T max = std::numeric_limits<T>::max()) {
  // First condition should be optimized out for unsigned types.
  return (b < 0) ? ((/*a+b >= min*/ a >= min - b) ? a + b : min)
                 : ((/*a+b <= max*/ a <= max - b) ? a + b : max);
}

// Returns the closest value to 'a - b' in [min:max], for under/overflow check.
template <typename T>
static constexpr T SafeSub(T a, T b, T min = std::numeric_limits<T>::min(),
                           T max = std::numeric_limits<T>::max()) {
  // First condition should be optimized out for unsigned types.
  return (b < 0) ? ((/*a-b <= max*/ a <= max + b) ? a - b : max)
                 : ((/*a-b >= min*/ a >= min + b) ? a - b : min);
}

//------------------------------------------------------------------------------
// Regular shifts with ignored Undefined Behavior Sanitizer warnings about the
// shift of negative values.

template <typename T>
inline WP2_UBSAN_IGNORE_UNDEF T RightShift(T v, uint32_t shift) {
  return v >> shift;
}
template <typename T>
inline T RightShiftRound(T v, uint32_t shift) {
  return RightShift(v + ((T)1 << shift >> 1), shift);
}
template <typename T>
inline WP2_UBSAN_IGNORE_UNDEF T LeftShift(T v, uint32_t shift) {
  return v << shift;
}

// Returns 'v' shifted right or left by abs(from - to). Right shifting uses an
// extra bit during computation for rounding.
template <typename T>
inline T ChangePrecision(T v, uint32_t from, uint32_t to) {
  return (from <= to) ? LeftShift<T>(v, to - from)
                      : RightShiftRound<T>(v, from - to);
}

// Returns ceil(a / b).
static constexpr uint32_t DivCeil(uint32_t a, uint32_t b) {
  return (a == 0u) ? 0u : (a - 1u) / b + 1u;
}

// Returns round(a / b).
template <typename T>
static constexpr T DivRound(T a, T b) {
  return ((a < 0) == (b < 0)) ? ((a + b / 2) / b) : ((a - b / 2) / b);
}

static constexpr uint32_t Pad(uint32_t dim, uint32_t pad) {
  return (dim + pad - 1) & ~(pad - 1);
}

// 3x3 matrix multiplication such as 'a * b = c'.
// Make sure there is no overflow by keeping a margin of 3 bits in 'a' or 'b').
template <typename TypeA, typename TypeB, typename TypeC>
static void Multiply(const TypeA a[9], const TypeB b[9], TypeC c[9]) {
  for (uint32_t j : {0, 1, 2}) {
    for (uint32_t i : {0, 1, 2}) {
      c[j * 3 + i] = (TypeC)a[j * 3 + 0] * b[0 * 3 + i] +
                     (TypeC)a[j * 3 + 1] * b[1 * 3 + i] +
                     (TypeC)a[j * 3 + 2] * b[2 * 3 + i];
    }
  }
}

// Vector and 3x3 matrix multiplication such as 'm * in = out'.
template <typename TypeIn, typename TypeMatrix, typename TypeOut>
inline void Multiply(TypeIn in0, TypeIn in1, TypeIn in2, const TypeMatrix m[9],
                     TypeOut* const out0, TypeOut* const out1,
                     TypeOut* const out2) {
  *out0 = m[0] * (TypeOut)in0 + m[1] * (TypeOut)in1 + m[2] * (TypeOut)in2;
  *out1 = m[3] * (TypeOut)in0 + m[4] * (TypeOut)in1 + m[5] * (TypeOut)in2;
  *out2 = m[6] * (TypeOut)in0 + m[7] * (TypeOut)in1 + m[8] * (TypeOut)in2;
}

// Fixed-point vector/matrix multiplication such as 'out = (m * in) >> shift'.
// Make sure the range of TInternal can hold internal computation.
template <typename TypeIn, typename TypeMatrix, typename TypeOut,
          typename TInternal = int32_t>
static void Multiply(TypeIn in0, TypeIn in1, TypeIn in2, const TypeMatrix m[9],
                     uint32_t shift, TypeOut* const out0, TypeOut* const out1,
                     TypeOut* const out2) {
  TInternal tmp0, tmp1, tmp2;
  Multiply(in0, in1, in2, m, &tmp0, &tmp1, &tmp2);
  tmp0 = RightShiftRound(tmp0, shift);
  tmp1 = RightShiftRound(tmp1, shift);
  tmp2 = RightShiftRound(tmp2, shift);
  constexpr TInternal kMin = std::numeric_limits<TypeOut>::min();
  constexpr TInternal kMax = std::numeric_limits<TypeOut>::max();
  *out0 = (TypeOut)Clamp(tmp0, kMin, kMax);
  *out1 = (TypeOut)Clamp(tmp1, kMin, kMax);
  *out2 = (TypeOut)Clamp(tmp2, kMin, kMax);
}

// Useful macros for dividing by alpha (un-mult)
// for all values v in [0,255] and a in [1,255], we have:
//  (v * 255 + a/2) / a  ==  (v * M + 1<<15) >> 16
// where M = ((255u << 16) + a - 1) / a
// ... except for the pair v=207, a=193 !!
// But we're taking the std::min(..., 255u), so it's fine to use 16 bit.
extern const uint32_t kAlphaDiv[256];  // <- ((255u << 16) + a - 1) / a

static inline uint32_t DivByAlphaDiv(uint32_t x, uint32_t mult) {
  return (x * mult + (1u << 15)) >> 16;
}

// Same as (v+127)/255 for v up to 255 and error margin of +-1 up to 1024*256.
static inline int32_t DivBy255(int32_t v) { return (v + (v >> 8) + 128) >> 8; }
static inline uint32_t DivBy255(uint32_t v) {
  return (v + (v >> 8) + 128) >> 8;
}

// Returns the square root of integer 'v' such as 'r^2 <= v' and '(r+1)^2 > v'.
// Make sure 'v^2' fits in T. Slow func, use sparingly or optimize it.
template <typename T>
T SqrtFloor(T v) {
  static_assert(std::is_integral<T>::value, "SqrtFloor() is only for integers");
  T lo = std::max((T)0, std::numeric_limits<T>::min()), hi = v;
  while (lo < hi) {
    const auto mid = (lo + hi + 1) / 2;
    if (mid * mid > v) {
      hi = mid - 1;
    } else {
      lo = mid;
    }
  }
  return lo;
}

}  // namespace WP2

//------------------------------------------------------------------------------
// Random generator

namespace WP2 {

class PseudoRNG {
 public:
  PseudoRNG();

  // Returns a pseudo-random number in (-amp, amp) range. 'amp' must be > 0.
  // (uses D.Knuth's Difference-based random generator).
  int32_t GetSigned(int32_t amp) {
    assert(amp > 0);
    int32_t diff;
    do {
      diff = Update();
      diff = ((int64_t)diff * amp) >> 31;
    } while (diff == -amp);
    return diff;
  }
  // returns a pseudo-random number in [0, amp), with amp > 0 (slow!)
  uint32_t GetUnsigned(uint32_t amp) {
    assert(amp > 0);
    uint32_t diff;
    do {
      diff = (uint32_t)Update();
      diff = (((uint64_t)diff * amp) >> 32);
    } while (diff >= amp);
    return diff;
  }
  // returns a value in [min, max]
  int32_t Get(int32_t min, int32_t max) {
    assert(max >= min);
    return GetUnsigned((uint32_t)(max + 1 - min)) + min;
  }

 private:
  int32_t Update();  // update state and return uniform 31b-signed number
  static constexpr uint32_t kTabSize = 55u;
  static const uint32_t kRandomTable[kTabSize];
  uint32_t index1_, index2_;
  uint32_t tab_[kTabSize];
};

}  // namespace WP2

#endif /* WP2_DSP_MATH_H_ */
