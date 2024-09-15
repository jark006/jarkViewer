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
//   utilities around planes and filters
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_UTILS_PLANE_H_
#define WP2_UTILS_PLANE_H_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/dsp/math.h"
#include "src/utils/csp.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

typedef int8_t tap_t;

// Contains a pair of 'Taps' used for up- or downsampling (e.g. chroma planes).
struct SamplingTaps {
  struct Taps {
    // Taps is assumed to contain only coefficients in [-31, 31] range
    // and be normalized to a sum of '16' (4 bits of fractional precision).
    // 'tap_coeffs' are centered at 'r'. They must be persistent.
    Taps(int r, const tap_t tap_coeffs[]) : radius(r), taps(tap_coeffs) {}

    tap_t operator[](int j) const { return taps[j]; }
    bool operator==(const Taps& rhs) const;
    bool operator!=(const Taps& rhs) const { return !operator==(rhs); }

    bool IsValid() const;

   public:
    const int radius;  // Half-size. Access to taps[-w ... w] should be valid.
    const tap_t* const taps;
  };

  // Default filters.
  static const SamplingTaps kDownSharp;
  static const SamplingTaps kDownAvg;
  static const SamplingTaps kUpSmooth;
  static const SamplingTaps kUpNearest;

  bool operator==(const SamplingTaps& rhs) const;
  bool operator!=(const SamplingTaps& rhs) const { return !operator==(rhs); }

  bool IsValid() const { return t1.IsValid() && t2.IsValid(); }

  // Can be either full/half-pixel pair, or horizontal/vertical.
  const Taps t1, t2;
};

//------------------------------------------------------------------------------

// everything unrelated to the template goes here
class PlaneBase {
 public:
  bool IsEmpty() const { return w_ == 0 || h_ == 0; }
  bool IsView() const { return !is_private_; }

  Rectangle AsRect() const { return {0, 0, w_, h_}; }
  uint32_t w_ = 0, h_ = 0;
  size_t step_ = 0;

 protected:
  WP2Status CheckRect(const Rectangle& rect) const;

  Vector_u8 memory_;
  bool is_private_ = true;
};

//------------------------------------------------------------------------------

// the type-dependent template
template <typename T>
class Plane : public PlaneBase {
 public:
  // Resizes the plane to zero and clears (private) memory. If this is a view,
  // it becomes an empty view, but the external memory is not affected, unless
  // it was a view of itself.
  void Clear() {
    w_ = h_ = step_ = 0;
    is_private_ = true;
    memory_.clear();
    data_ = nullptr;
  }

  // Returns a lower bound of the number of bytes available in this buffer/view.
  size_t Size() const { return (step_ * SafeSub(h_, 1u) + w_) * sizeof(T); }

  // Resizes the memory allocated without preserving content. 'step' is the
  // number of elements to go from one line to the next, it is computed if 0.
  WP2Status Resize(uint32_t width, uint32_t height, size_t step = 0) {
    if (width == 0 || height == 0) {
      Clear();
      return WP2_STATUS_OK;
    }
    if (step == 0) step = width;
    WP2_CHECK_OK(step >= width, WP2_STATUS_INVALID_PARAMETER);
    w_ = width;
    h_ = height;
    step_ = step;
    is_private_ = true;
    WP2_CHECK_ALLOC_OK(memory_.resize(h_ * step_ * sizeof(T)));
    data_ = reinterpret_cast<T*>(memory_.data());
    return WP2_STATUS_OK;
  }

  // Copies data.
  WP2Status Copy(const Plane& src, bool resize_if_needed) {
    if (w_ != src.w_ || h_ != src.h_) {
      WP2_CHECK_OK(resize_if_needed, WP2_STATUS_BAD_DIMENSION);
      WP2_CHECK_STATUS(Resize(src.w_, src.h_));
    }
    const T* src_row = src.data_;
    T* dst_row = data_;
    for (uint32_t y = 0; y < h_; ++y) {
      std::memcpy(dst_row, src_row, w_ * sizeof(T));
      src_row += src.Step();
      dst_row += Step();
    }
    return WP2_STATUS_OK;
  }

  // Fills with uniform value. 'rect' will be clipped to the plane's dimension.
  void Fill(const Rectangle& rect, T value) {
    const Rectangle r = rect.ClipWith({0, 0, w_, h_});
    if (r.width == 0 || r.height == 0) return;
    for (uint32_t y = r.y; y < r.y + r.height; ++y) {
      T* const row = &At(r.x, y);
      std::fill(row, row + r.width, value);
    }
  }
  void Fill(T value) { Fill({0, 0, w_, h_}, value); }

  // Same as Fill() but only draws the outline.
  void DrawRect(const Rectangle& rect, T value) {
    const Rectangle r = rect.ClipWith({0, 0, w_, h_});
    if (r.width == 0 || r.height == 0) return;
    const uint32_t x0 = r.x, x1 = r.x + r.width - 1;
    const uint32_t y0 = r.y, y1 = r.y + r.height - 1;
    for (uint32_t x = x0; x <= x1; ++x) At(x, y0) = At(x, y1) = value;
    for (uint32_t y = y0 + 1; y < y1; ++y) At(x0, y) = At(x1, y) = value;
  }

  // Stretches the values to fill the padded right and bottom borders.
  WP2Status FillPad(uint32_t non_padded_width, uint32_t non_padded_height) {
    WP2_CHECK_OK((non_padded_width >= 1 && non_padded_width <= w_ &&
                  non_padded_height >= 1 && non_padded_height <= h_),
                 WP2_STATUS_BAD_DIMENSION);
    if (non_padded_width < w_) {
      for (uint32_t y = 0; y < h_; ++y) {
        T* const row_end = &At(non_padded_width, y);
        std::fill(row_end, row_end + w_ - non_padded_width, row_end[-1]);
      }
    }
    for (uint32_t y = non_padded_height; y < h_; ++y) {
      std::memcpy(Row(y), Row(non_padded_height - 1), w_ * sizeof(T));
    }
    return WP2_STATUS_OK;
  }

  // Extracts a view from 'src' which must out-live the view. No memory is freed
  // if it is a view of itself, only the new bounds are applied.
  // Caution: the view may modify the pixels of 'src' even if it is const.
  WP2Status SetView(const Plane& src, const Rectangle& rect) {
    WP2_CHECK_STATUS(src.CheckRect(rect));
    if (&memory_ != &src.memory_) {
      memory_.clear();
      is_private_ = false;
    }
    data_ = const_cast<T*>(&src.At(rect.x, rect.y));
    w_ = rect.width;
    h_ = rect.height;
    step_ = src.step_;
    return WP2_STATUS_OK;
  }
  WP2Status SetView(const Plane& src) {
    return SetView(src, {0, 0, src.w_, src.h_});
  }

  // Sets view on external data.
  WP2Status SetView(T src[], uint32_t width, uint32_t height, size_t step) {
    WP2_CHECK_OK(width >= 1 && height >= 1, WP2_STATUS_BAD_DIMENSION);
    WP2_CHECK_OK(step >= width, WP2_STATUS_INVALID_PARAMETER);
    memory_.clear();
    is_private_ = false;
    data_ = src;
    w_ = width;
    h_ = height;
    step_ = step;
    return WP2_STATUS_OK;
  }

  // Convert V src[] plane into plane's type T
  template <typename V>
  void From(const V* src, size_t step, T offset = 0) {
    for (uint32_t y = 0; y < h_; ++y) {
      T* const dst = Row(y);
      for (uint32_t x = 0; x < w_; ++x) dst[x] = (T)src[x] + offset;
      src += step;
    }
  }
  // Convert plane's type T into dst[] plane of type V
  template <typename V>
  void To(V* dst, size_t step, T offset = 0) const {
    for (uint32_t y = 0; y < h_; ++y) {
      const T* const src = Row(y);
      for (uint32_t x = 0; x < w_; ++x) dst[x] = (V)(src[x] + offset);
      dst += step;
    }
  }

  inline size_t Step() const { return step_; }

  // Array access.
  T* Row(uint32_t y) {
    assert(y < h_);
    return data_ + y * Step();  // TODO(skal): safe non-overflow mult
  }
  const T* Row(uint32_t y) const {
    assert(y < h_);
    return data_ + y * Step();  // TODO(skal): safe non-overflow mult
  }

  // Pixel access with assertion.
  T& At(uint32_t x, uint32_t y) {
    assert(x < w_);
    return Row(y)[x];
  }
  const T& At(uint32_t x, uint32_t y) const {
    assert(x < w_);
    return Row(y)[x];
  }

  // Pixel access clamped to image boundaries.
  const T& AtClamped(uint32_t x, uint32_t y) const {
    assert(w_ >= 1 && h_ >= 1);
    return At(std::min(x, w_ - 1), std::min(y, h_ - 1));
  }
  const T& AtClamped(int x, int y) const {
    return AtClamped((uint32_t)std::max(x, 0), (uint32_t)std::max(y, 0));
  }
  T& AtClamped(int x, int y) {
    return At(std::min(std::max(x, 0), (int)w_ - 1),
              std::min(std::max(y, 0), (int)h_ - 1));
  }

  // No bound checking, to access pixels adjacent to a subview.
  const T& AtUnclamped(int x, int y) const {
    return data_[y * (int)Step() + x];
  }

  // Returns true if this plane has the same dimensions as 'other'.
  bool IsSameSize(const Plane& other) const {
    return w_ == other.w_ && h_ == other.h_;
  }

  // Chroma down-sampling. 'filter' is centered.
  WP2Status DownsampleFrom(
      const Plane& src, const SamplingTaps& filter = SamplingTaps::kDownSharp) {
    WP2_CHECK_OK(src.w_ >= 1 && src.h_ >= 1, WP2_STATUS_BAD_DIMENSION);
    WP2_CHECK_OK(filter.IsValid(), WP2_STATUS_INVALID_PARAMETER);
    WP2_CHECK_STATUS(Resize(DivCeil(src.w_, 2), DivCeil(src.h_, 2)));
    for (uint32_t y = 0; y < h_; ++y) {
      T* const dst = Row(y);
      for (uint32_t x = 0; x < w_; ++x) {
        dst[x] = src.Sample(filter, (int)x * 2, (int)y * 2);
      }
    }
    return WP2_STATUS_OK;
  }
  WP2Status Downsample(const SamplingTaps& filter = SamplingTaps::kDownSharp) {
    if (w_ == 1 && h_ == 1) return WP2_STATUS_OK;
    Plane<T> p;
    WP2_CHECK_STATUS(p.DownsampleFrom(*this, filter));
    using std::swap;
    swap(*this, p);
    return WP2_STATUS_OK;
  }

  // Up-sampling. 'filter.t1' is full-pixel, 'filter.t2' is half-pixel.
  WP2Status UpsampleFrom(const Plane& src, uint32_t to_width,
                         uint32_t to_height,
                         const SamplingTaps& filter = SamplingTaps::kUpSmooth) {
    WP2_CHECK_OK(
        src.w_ == DivCeil(to_width, 2) && src.h_ == DivCeil(to_height, 2),
        WP2_STATUS_BAD_DIMENSION);
    WP2_CHECK_OK(filter.IsValid(), WP2_STATUS_INVALID_PARAMETER);
    const uint32_t half_width = to_width / 2, half_height = to_height / 2;
    WP2_CHECK_STATUS(Resize(to_width, to_height));

    const uint32_t step = Step();
    const SamplingTaps ff = {filter.t1, filter.t1};  // full/full
    const SamplingTaps& fh = filter;
    const SamplingTaps hf = {filter.t2, filter.t1};
    const SamplingTaps hh = {filter.t2, filter.t2};  // half/half
    for (uint32_t j = 0; j < half_height; ++j) {
      for (uint32_t i = 0; i < half_width; ++i) {
        data_[(2 * i + 0) + (2 * j + 0) * step] = src.Sample(ff, i, j);
        data_[(2 * i + 1) + (2 * j + 0) * step] = src.Sample(hf, i, j);
        data_[(2 * i + 0) + (2 * j + 1) * step] = src.Sample(fh, i, j);
        data_[(2 * i + 1) + (2 * j + 1) * step] = src.Sample(hh, i, j);
      }
      if (src.w_ == half_width + 1) {
        const uint32_t i = half_width;
        data_[(2 * i + 0) + (2 * j + 0) * step] = src.Sample(ff, i, j);
        data_[(2 * i + 0) + (2 * j + 1) * step] = src.Sample(fh, i, j);
      }
    }
    if (src.h_ == half_height + 1) {
      const uint32_t j = half_height;
      for (uint32_t i = 0; i < half_width; ++i) {
        data_[(2 * i + 0) + (2 * j + 0) * step] = src.Sample(ff, i, j);
        data_[(2 * i + 1) + (2 * j + 0) * step] = src.Sample(hf, i, j);
      }
      if (src.w_ == half_width + 1) {
        const uint32_t i = half_width;
        data_[(2 * i + 0) + (2 * j + 0) * step] = src.Sample(ff, i, j);
      }
    }
    return WP2_STATUS_OK;
  }
  WP2Status Upsample(uint32_t to_width, uint32_t to_height,
                     const SamplingTaps& filter = SamplingTaps::kUpSmooth) {
    if (to_width == 1 && to_height == 1) {
      WP2_CHECK_OK(w_ == 1 && h_ == 1, WP2_STATUS_BAD_DIMENSION);
      return WP2_STATUS_OK;
    }
    Plane<T> p;
    WP2_CHECK_STATUS(p.UpsampleFrom(*this, to_width, to_height, filter));
    using std::swap;
    swap(*this, p);
    return WP2_STATUS_OK;
  }

  // Mostly for debugging. 'dst' is not resized. 'bit_depth' describes the data
  // in this Plane that will be converted to 8-bit unsigned to fit in 'dst'.
  WP2Status ToGray(ArgbBuffer* dst, BitDepth bit_depth) const;

  // Process 'src's samples with arbitrary function.
  WP2Status Process(const ArgbBuffer& src,
                    int32_t (*convert)(const void* sample) = ToGrayF) {
    WP2_CHECK_STATUS(Resize(src.width(), src.height()));
    for (uint32_t y = 0; y < h_; ++y) {
      T* const dst = Row(y);
      const uint8_t* psrc = src.GetRow8(y);
      for (uint32_t x = 0; x < w_; ++x, psrc += WP2FormatBpp(src.format())) {
        dst[x] = (T)convert((const void*)psrc);
      }
    }
    return WP2_STATUS_OK;
  }

  // Computes sum of squared error. Implemented for 8u/16s.
  WP2Status GetSSE(const Plane<T>& src, uint64_t* score) const;
  // Computes SSIM. Implemented for 16s.
  WP2Status GetSSIM(const Plane<T>& src, BitDepth bit_depth,
                    double* score) const;

 protected:
  // Returns the sample at 'x, y' filtered horizontally by 'filter.t1' and
  // vertically by 'filter.t2'.
  T Sample(const SamplingTaps& filter, int32_t x, int32_t y) const {
    const bool clipped =
        (x < filter.t1.radius || x + filter.t1.radius >= (int32_t)w_);
    int32_t sum = 0;
    for (int32_t j = -filter.t2.radius; j <= filter.t2.radius; ++j) {
      const T* const data = &data_[Clamp(y + j, 0, (int32_t)h_ - 1) * step_];
      uint32_t sub_sum = 0;
      if (!clipped) {
        for (int32_t i = -filter.t1.radius; i <= filter.t1.radius; ++i) {
          sub_sum += data[x + i] * filter.t1[i];
        }
      } else {
        for (int32_t i = -filter.t1.radius; i <= filter.t1.radius; ++i) {
          const int pos = Clamp(x + i, 0, (int32_t)w_ - 1);
          sub_sum += data[pos] * filter.t1[i];
        }
      }
      sum += sub_sum * filter.t2[j];
    }
    return RightShiftRound(sum, 8);
  }

  // simple Luma converter for Process()
  static int32_t ToGrayF(const void* src) {
    const uint8_t* const src8 = (const uint8_t*)src;
    const int32_t r = src8[1], g = src8[2], b = src8[3];
    return ((77 * r + 149 * g + 29 * b + 128) >> 8);
  }

 public:  // minimal iterator
  class iterator {
   public:
    iterator(const T* const ptr, uint32_t width, size_t step)
        : x_(0), y_(0), width_(width), step_(step), ptr_((T*)ptr) {}
    iterator operator++() {
      if (++x_ == width_) {
        ptr_ += step_, x_ = 0, ++y_;
      }
      return *this;
    }
    bool operator!=(const iterator& i) const {
      return (&ptr_[x_] != &i.ptr_[i.x_]);
    }
    T& operator*() { return ptr_[x_]; }

    size_t x_, y_;

   private:
    const size_t width_, step_;
    T* ptr_;
  };
  iterator begin() const { return {data_, w_, step_}; }
  iterator end() const { return {data_ + h_ * step_, 0, 0}; }

 protected:
  T* data_ = nullptr;

 public:
  friend void swap(Plane& a, Plane& b) {
    using std::swap;
    swap(a.w_, b.w_);
    swap(a.h_, b.h_);
    swap(a.step_, b.step_);
    swap(a.memory_, b.memory_);
    swap(a.is_private_, b.is_private_);
    swap(a.data_, b.data_);
  }
};

typedef Plane<int16_t> Plane16;
typedef Plane<uint8_t> Plane8u;
typedef Plane<float> Planef;

//------------------------------------------------------------------------------

// Points to the luma, chroma and alpha components of an image. Can own the data
// or be a view, so assumptions on the constness of the pixels should not be
// made even for const references.
class YUVPlane {
 public:
  // Returns true if each plane is empty (has no pixel).
  bool IsEmpty() const;

  // Returns true if this buffer points to a non-owned memory.
  bool IsView() const;

  // Returns the dimension of the image. Asserts that the size of each plane is
  // consistent with the others.
  uint32_t GetWidth() const;
  uint32_t GetHeight() const;
  Rectangle AsRect() const { return {0, 0, Y.w_, Y.h_}; }

  // Resizes the planes to zero and clears (private) memory. If there are views,
  // they become empty views, but the external memory is not affected.
  void Clear();

  // Returns either Y, U, V or A depending on the channel number.
  const Plane16& GetChannel(Channel channel) const;
  Plane16& GetChannel(Channel channel);

  bool HasAlpha() const { return (A.w_ != 0); }

  // Resizes each plane to be 'width x height' rounded up to a 'pad' multiple.
  // If 'as_yuv420' is true, U/V planes will have half dimension.
  WP2Status Resize(uint32_t width, uint32_t height, uint32_t pad = 1,
                   bool has_alpha = false, bool as_yuv420 = false);

  // Copies 'src'.
  WP2Status Copy(const YUVPlane& src, bool resize_if_needed);

  // Fills each plane with its 'color' component.
  void Fill(const Rectangle& rect, const Ayuv38b& color);
  void Fill(const Ayuv38b& color);

  // Stretch colors to fill the borders outside the 'non_padded_width' and
  // 'non_padded_height'.
  WP2Status FillPad(uint32_t non_padded_width, uint32_t non_padded_height);

  // Sets view on a sub-region of 'src' which must out-live the view.
  // Caution: the view may then modify the pixels of 'src'.
  WP2Status SetView(const YUVPlane& src, const Rectangle& rect);
  WP2Status SetView(const YUVPlane& src);

  // 'resize_if_needed' this YUVPlane to fit 'rgb' and copies its content,
  // applying the 'csp_transform'. Dimensions are rounded up to 'pad' multiple.
  // If 'import_alpha' is false, alpha should be fully opaque.
  WP2Status Import(const ArgbBuffer& rgb, bool import_alpha,
                   const CSPTransform& csp_transform, bool resize_if_needed,
                   size_t pad = 1);

  // Same as above but uses 'rgb_to_ccsp_matrix' and 'rgb_to_ccsp_shift' to
  // define the custom color space and precision of the output samples.
  WP2Status Import(const ArgbBuffer& rgb, bool import_alpha,
                   const CSPMtx& rgb_to_ccsp, bool resize_if_needed,
                   size_t pad = 1);

  // Same as above but uses 'ccsp_to_rgb_matrix' and 'ccsp_to_rgb_shift' to
  // define the custom color space and precision of the input samples.
  // See CSPTranform.
  WP2Status Import(const YUVPlane& ccsp, const CSPMtx& ccsp_to_rgb,
                   const CSPTransform& csp_transform, bool resize_if_needed,
                   size_t pad = 1);

  // Applies 'matrix' on all samples then 'shift' right. Ignores alpha.
  template <typename TMtx, typename TInternal = int32_t>
  WP2Status Apply(const TMtx matrix[9], uint32_t shift) {
    for (uint32_t y = 0; y < GetHeight(); ++y) {
      for (uint32_t x = 0; x < GetWidth(); ++x) {
        Multiply<int16_t, TMtx, int16_t, TInternal>(
            Y.At(x, y), U.At(x, y), V.At(x, y), matrix, shift, &Y.At(x, y),
            &U.At(x, y), &V.At(x, y));
      }
    }
    return WP2_STATUS_OK;
  }

  // Converts samples to Argb using the 'transform' and 'resize_if_needed'.
  // The 'upsample_if_needed' will convert 420->444 if not null and if needed.
  // 'dst' should be in WP2_Argb_32 or WP2_ARGB_32 format.
  WP2Status Export(const CSPTransform& transform, bool resize_if_needed,
                   ArgbBuffer* dst,
                   const SamplingTaps* upsample_if_needed = nullptr) const;

  // Same as above but uses 'ccsp_to_rgb_matrix' and 'ccsp_to_rgb_shift' to
  // define the color space and precision of the input samples. See CSPTranform.
  // 'dst' should be in WP2_Argb_32 format.
  WP2Status Export(const CSPMtx& ccsp_to_rgb, bool resize_if_needed,
                   ArgbBuffer* dst,
                   const SamplingTaps* upsample_if_needed = nullptr) const;

  // Chroma up- and downsampling (4:2:0 <-> 4:4:4).
  // Some memory can be saved with 'use_views_for_ya' is 'src' is persistent.
  WP2Status DownsampleFrom(const YUVPlane& src,
                           const SamplingTaps& f = SamplingTaps::kDownSharp,
                           bool use_views_for_ya = false);
  WP2Status Downsample(const SamplingTaps& f = SamplingTaps::kDownSharp);
  WP2Status UpsampleFrom(const YUVPlane& src,
                         const SamplingTaps& f = SamplingTaps::kUpSmooth,
                         bool use_views_for_ya = false);
  WP2Status Upsample(const SamplingTaps& f = SamplingTaps::kUpSmooth);
  bool IsDownsampled() const;

  // Composites this buffer on the given background buffer. Buffer size must
  // match. Assumes that the samples were converted from premultiplied RGB to
  // YUV using the given transform.
  WP2Status CompositeOver(const YUVPlane& background,
                          const CSPTransform& transform);
  // Composites the given buffer on top of this buffer. Buffer size must match.
  // Assumes that the samples were converted from premultiplied RGB to YUV
  // using the given transform.
  WP2Status CompositeUnder(const YUVPlane& foreground,
                           const CSPTransform& transform);

  // Computes the distortion between two images. 'bit_depth' is for YUV because
  // alpha is always unsigned 8 bits. Not all 'metric_type' are implemented.
  // Results are in dB, stored in 'result' in the A/Y/U/V/All order. Returns
  // WP2_STATUS_OK or an error.
  WP2Status GetDistortion(const YUVPlane& ref, BitDepth bit_depth,
                          MetricType metric_type, float result[5]) const;

  // Returns true if the U and V planes contain only zeros.
  bool IsMonochrome() const;

  // TODO(maryla): the A plane could be a Plane8.
  Plane16 Y, U, V, A;  // NOLINT
};

void swap(YUVPlane& a, YUVPlane& b);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_UTILS_PLANE_H_
