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
//   Deterministic platform-agnostic random tools.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_UTILS_RANDOM_H_
#define WP2_UTILS_RANDOM_H_

#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <random>
#include <type_traits>

namespace WP2 {

//------------------------------------------------------------------------------

// Returns a uniformly distributed random value in range [min, max].
// Uses fixed-point instead of floating-point operations to remove any
// possibility of different results on different platforms.
template <typename Generator, typename IntType>
static IntType GetUniformIntDistribution(Generator* const gen, IntType min,
                                         IntType max) {
  static constexpr uint64_t random_max_value =
      (uint64_t)Generator::max() - Generator::min();
  static_assert(random_max_value <= std::numeric_limits<uint32_t>::max(),
                "Invalid generator range");
  static_assert(std::is_integral<IntType>::value,
                "Unsupported non integral type");

  assert(min <= max);
  const uint64_t max_value = (uint64_t)max + 1ull - min;
  assert(max_value <= random_max_value);  // Prevent overflow.

  // Make sure the distribution is uniform by re-rolling if outside 'hi_thresh'.
  const uint64_t hi_thresh = (random_max_value / max_value) * max_value;
  uint64_t v;
  do {
    v = (uint64_t)(*gen)();
  } while (v >= hi_thresh);
  return (IntType)((v % max_value) + min);
}

//------------------------------------------------------------------------------

// mt19937 + GetUniformIntDistribution() convenience wrapper
class UniformIntDistribution {
 public:
  explicit UniformIntDistribution(uint32_t seed = 0) : generator_(seed) {}

  template <typename T = int32_t>
  T Get(T min, T max) {  // uniform distribution in [min, max]
    return GetUniformIntDistribution(&generator_, min, max);
  }

  bool FlipACoin() { return (bool)(generator_() & 1); }

 private:
  std::mt19937 generator_;  // mt19937 is 32bits based.
};

//------------------------------------------------------------------------------

// Possible implementation of std::shuffle() from cppreference.com
template <class RandomIt, typename SeedType>
void Shuffle(RandomIt first, RandomIt last, SeedType seed = 0) {
  typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;

  const diff_t n = last - first;
  if (n < 1) return;
  assert(n <= (diff_t)std::numeric_limits<uint32_t>::max());

  std::mt19937 generator(seed);
  for (uint32_t i = (uint32_t)n - 1u; i > 0u; --i) {
    std::swap(first[i], first[GetUniformIntDistribution(&generator, 0u, i)]);
  }
}

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_UTILS_RANDOM_H_
