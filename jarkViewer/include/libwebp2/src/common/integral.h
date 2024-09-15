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
//  'Integral' struct to quickly compute mean and variance over rectangles.
//
// Author: Skal (pascal.massimino@gmail.com)
//
#ifndef WP2_COMMON_INTEGRAL_H_
#define WP2_COMMON_INTEGRAL_H_

#include <cassert>
#include <cmath>
#include <cstdint>

#include "src/dsp/math.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

class Integral {
 public:
  // integral_t will contain the sum of 10 bit values (and their square) over a
  // tile (for the maximum value). Even if the tile has side 1024, we end up
  // with 40 bits at most so we are safe with 64 bits.
  typedef int64_t integral_t;
  typedef int16_t input_t;

  // 'w' and 'h' are dimension in block units (of size 'size').
  WP2Status Allocate(uint32_t w, uint32_t h, uint32_t size);

  bool empty() const { return (w_ == 0 || h_ == 0); }

  // Adds the contribution of Y/U/V planes 'yuv', with a mixing factor 'uv_mix'.
  void AddValues(const YUVPlane& yuv, float uv_mix = 0.f);
  // Adds the contribution of the given plane.
  void AddValues(const Plane16& plane);

  // Returns the standard deviation of a block.
  float StdDev(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) const {
    assert(x1 > x0 && y1 > y0);
    const int32_t area = (x1 - x0) * (y1 - y0);
    const float norm_pix = 1.f / (area * size_ * size_);
    // Get values per pixel.
    const float m = norm_pix * get_m(x0, y0, x1, y1);
    const float m2 = norm_pix * get_m2(x0, y0, x1, y1);
    const float var = m2 - m * m;
    return (var < 0.f ? 0.f : std::sqrt(var));
  }
  // Returns the clamped version of StdDev.
  uint8_t StdDevUint8(uint32_t x0, uint32_t y0, uint32_t x1,
                      uint32_t y1) const {
    return (uint8_t)std::lround(Clamp(StdDev(x0, y0, x1, y1), 0.f, 255.f));
  }

 protected:
  uint32_t pos(uint32_t x, uint32_t y) const { return x + y * wstride_; }
  integral_t w1(uint32_t x, uint32_t y) const {
    assert(pos(x, y) >= 0 && pos(x, y) < w1_.size());
    return w1_[pos(x, y)];
  }
  integral_t w2(uint32_t x, uint32_t y) const {
    assert(pos(x, y) >= 0 && pos(x, y) < w2_.size());
    return w2_[pos(x, y)];
  }
  // Returns the sum of values over a block.
  integral_t get_m(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) const {
    return w1(x1, y1) + w1(x0, y0) - w1(x1, y0) - w1(x0, y1);
  }
  // Returns the sum of squared values over a block.
  integral_t get_m2(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) const {
    return w2(x1, y1) + w2(x0, y0) - w2(x1, y0) - w2(x0, y1);
  }

  // Returns mean and square mean at a given position (in block units).
  void GetArea(const Plane<input_t>& p, uint32_t x0, uint32_t y0, integral_t* M,
               integral_t* M2) const;
  void GetArea(const YUVPlane& yuv, float uv_mix, uint32_t x0, uint32_t y0,
               integral_t* M, integral_t* M2) const;

 private:
  void SetW(uint32_t i, uint32_t j, integral_t M, integral_t M2);

  VectorNoCtor<integral_t> w1_, w2_;
  uint32_t wstride_ = 0;
  uint32_t w_ = 0, h_ = 0;  // dimension in block unit
  uint32_t size_ = 0;       // unit block size
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_COMMON_INTEGRAL_H_
