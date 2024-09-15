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
//  Common functions
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_WP2_BASE_H_
#define WP2_WP2_BASE_H_

#define WP2_VERSION 0x000001    // MAJOR(8b) + MINOR(8b) + REVISION(8b)
#define WP2_ABI_VERSION 0x0001  // MAJOR(8b) + MINOR(8b)

//------------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#ifndef WP2_EXTERN
// This explicitly marks library functions and allows for changing the
// signature for e.g., Windows DLL builds.
#if defined(__GNUC__)
#define WP2_EXTERN extern __attribute__((visibility("default")))
#else
#define WP2_EXTERN extern
#endif /* __GNUC__ >= 4 */
#endif /* WP2_EXTERN */

#if defined(__GNUC__)
// With C++17, we could use the [[nodiscard]] attribute.
#define WP2_NO_DISCARD __attribute__((warn_unused_result))
#else
#define WP2_NO_DISCARD
#endif

// Macro to check ABI compatibility (same major revision number)
#define WP2_ABI_IS_INCOMPATIBLE(a, b) (((a) >> 8) != ((b) >> 8))

// Return the library's version number, packed in hexadecimal using 8bits for
// each of major/minor/revision. E.g: v2.5.7 is 0x020507.
WP2_EXTERN int WP2GetVersion();

// Return the library's ABI version number, similar to WP2GetVersion()
WP2_EXTERN int WP2GetABIVersion();

// Return true if the link and compile version are compatible.
// Should be checked first before using the library.
static inline bool WP2CheckVersion() {
  return !WP2_ABI_IS_INCOMPATIBLE(WP2GetABIVersion(), WP2_ABI_VERSION);
}

//------------------------------------------------------------------------------
// Status enumeration

typedef enum
#if __cplusplus >= 201703L
    [[nodiscard]]  // Triggers a warning on calls not using the returned value
                   // of functions returning a WP2Status by value.
#endif
{ WP2_STATUS_OK = 0,
  WP2_STATUS_VERSION_MISMATCH,
  WP2_STATUS_OUT_OF_MEMORY,        // memory error allocating objects
  WP2_STATUS_INVALID_PARAMETER,    // a parameter value is invalid
  WP2_STATUS_NULL_PARAMETER,       // a pointer parameter is NULL
  WP2_STATUS_BAD_DIMENSION,        // picture has invalid width/height
  WP2_STATUS_USER_ABORT,           // abort request by user
  WP2_STATUS_UNSUPPORTED_FEATURE,  // unsupported feature
  // the following are specific to decoding:
  WP2_STATUS_BITSTREAM_ERROR,        // bitstream has syntactic error
  WP2_STATUS_NOT_ENOUGH_DATA,        // premature EOF during decoding
  WP2_STATUS_BAD_READ,               // error while reading bytes
  WP2_STATUS_NEURAL_DECODE_FAILURE,  // neural decoder failed
  // the following are specific to encoding:
  WP2_STATUS_BITSTREAM_OUT_OF_MEMORY,  // memory error while flushing bits
  WP2_STATUS_INVALID_CONFIGURATION,    // encoding configuration is invalid
  WP2_STATUS_BAD_WRITE,                // error while flushing bytes
  WP2_STATUS_FILE_TOO_BIG,             // file is bigger than 4G
  WP2_STATUS_INVALID_COLORSPACE,       // encoder called with bad colorspace
  WP2_STATUS_NEURAL_ENCODE_FAILURE,    // neural encoder failed

  WP2_STATUS_LAST  // list terminator. always last.
} WP2Status;

// returns a printable string for the name of the status
WP2_EXTERN const char* WP2GetStatusMessage(WP2Status status);
// returns a short description of the status
WP2_EXTERN const char* WP2GetStatusText(WP2Status status);

//------------------------------------------------------------------------------
//  WP2SampleFormat

// The format of pixels passed for import into ArgbBuffer.
// These enum names describe the samples order are they are laid out
// in memory. For instance WP2_ARGB_32 will refer to 8b samples
// ordered as: A,R,G,B,A,R,G,B, ...
// The 'X' means that the alpha values are to be ignored and assumed to be set
// to 0xff everywhere (the image has no transparency). This assumption is not
// checked nor enforced.
// Non-capital names (e.g.:MODE_Argb) relates to pre-multiplied RGB channels.
// If all samples are 8 bits or less, they are stored in uint8_t. If one is > 8,
// they are all stored in uint16_t.
typedef enum {
  // 32b/pixel formats
  WP2_Argb_32 = 0,
  WP2_ARGB_32,
  WP2_XRGB_32,

  WP2_rgbA_32,
  WP2_RGBA_32,
  WP2_RGBX_32,

  WP2_bgrA_32,
  WP2_BGRA_32,
  WP2_BGRX_32,

  // 24b/pixel formats
  WP2_RGB_24,
  WP2_BGR_24,

  // HDR format: 8 bits for A, 10 per RGB.
  WP2_Argb_38,

  // TODO(skal): RGB565_16? rgbA4444_16? RGBA4444_16?
  WP2_FORMAT_NUM
} WP2SampleFormat;

// Returns the number of bytes per sample for the given format.
WP2_EXTERN uint32_t WP2FormatBpp(WP2SampleFormat format);

// Returns the number of bits used per RGB channels internally.
WP2_EXTERN uint32_t WP2Formatbpc(WP2SampleFormat format);

// Returns true if the 'format' has an alpha channel.
// Also returns its index if 'alpha_channel_index' is not null and there is one
// or there is a spot for alpha (e.g. RGBX). Otherwise 'alpha_channel_index'
// is set to WP2FormatBpp(format).
WP2_EXTERN bool WP2FormatHasAlpha(WP2SampleFormat format,
                                  uint32_t* alpha_channel_index = nullptr);

// Returns true if the format has premultiplied rgb values (rgb = a * RGB).
WP2_EXTERN bool WP2IsPremultiplied(WP2SampleFormat format);

namespace WP2 {

//------------------------------------------------------------------------------
// distortion type

typedef enum {
  PSNR,
  PSNRHVS,
  PSNR_YUV,  // PSNR_YUV is PSNR in YCbCr space for now (could be any other YUV)
  SSIM,
  SSIM_YUV,
  MSSSIM_ORIGINAL,  // Original MSSSIM metric from Simoncelli & al paper.
  MSSSIM,           // MSSIM metric with a larger weight for the 1:1 scale.
  LSIM,
  NUM_METRIC_TYPES
} MetricType;

//------------------------------------------------------------------------------
// Color structs

// The red, green and blue channels are premultiplied by the alpha component.
struct Argb32b {
  uint8_t a;        // Precision:  8 bits
  uint8_t r, g, b;  // Precision:  8 bits
};

struct Argb38b {
  uint8_t a;         // Precision:  8 bits
  uint16_t r, g, b;  // Precision:  10 bits
};

struct RGB12b {
  uint8_t r, g, b;  // Precision:  4 bits
};

struct Ayuv38b {
  uint8_t a;        // Precision:  8 bits
  int16_t y, u, v;  // Precision:  10 bits
};

// Colorspaces.
enum class Csp { kYCoCg, kYCbCr, kCustom, kYIQ };
constexpr uint32_t kNumCspTypes = 4;

//------------------------------------------------------------------------------

// Struct owning a chunk of memory which will be freed upon destruction.
struct Data {
  Data() = default;
  Data(Data&& other) noexcept;
  Data(const Data&) = delete;
  ~Data() { Clear(); }
  Data& operator=(Data&& other) noexcept;

  bool IsEmpty() const { return (size == 0); }

  // Deallocates the memory, and re-initializes the structure.
  void Clear();
  // Increases or decreases allocated memory to 'data_size' bytes.
  // If 'keep_bytes' is true, the 'bytes' will contain the same data as before.
  WP2_NO_DISCARD WP2Status Resize(size_t data_size, bool keep_bytes);
  // Resizes to 'data_size' then copies all 'data'.
  WP2_NO_DISCARD WP2Status CopyFrom(const uint8_t* data, size_t data_size);
  // Resizes to 'size + data_size' then copies all 'data'.
  WP2_NO_DISCARD WP2Status Append(const uint8_t* data, size_t data_size);

  uint8_t* bytes = nullptr;
  size_t size = 0;
};

void swap(Data& a, Data& b);

//------------------------------------------------------------------------------

// Image properties (color profile, XMP and EXIF metadata).
struct Metadata {
  Metadata() = default;
  Metadata(const Metadata&) = delete;
  Metadata(Metadata&&) = default;

  Data iccp;
  Data xmp;
  Data exif;

  // Returns true is there is no metadata stored.
  bool IsEmpty() const;
  // Deallocate the memory, and re-initialize the structure.
  void Clear();
  // Copy exif, iccp and xmp. Returns WP2_STATUS_OK or an error.
  WP2_NO_DISCARD WP2Status CopyFrom(const Metadata& src);
};

void swap(Metadata& a, Metadata& b);

//------------------------------------------------------------------------------

// Abstract interface. Inherit to write the bytes somewhere.
class Writer {
 public:
  Writer() = default;
  Writer(Writer&&) = delete;
  Writer(const Writer&) = delete;
  virtual ~Writer() = default;
  Writer& operator=(Writer&&) = delete;
  Writer& operator=(const Writer&) = delete;

  // Potentially called at the beginning of a sequence of Append(), to signal
  // the expected 'size' for the incoming new data. This can be used as a hint.
  // Should return false if an error happened.
  WP2_NO_DISCARD virtual bool Reserve(size_t size) { return true; }
  // Will be called when 'data_size' bytes of 'data' are ready.
  // Can be used to save the output of the encoding, for example.
  // Should return false if an error happened.
  WP2_NO_DISCARD virtual bool Append(const void* data, size_t data_size) = 0;
};

// Writes to memory. Handles allocation and dealloaction.
class MemoryWriter : public Writer {
 public:
  ~MemoryWriter() override { Reset(); }
  void Reset();  // Releases memory.

  WP2_NO_DISCARD bool Reserve(size_t size) override;
  WP2_NO_DISCARD bool Append(const void* data, size_t data_size) override;

  uint8_t* mem_ = nullptr;  // Final buffer (of capacity 'max_size' >= 'size').
  size_t size_ = 0;         // Final size.
  size_t max_size_ = 0;     // Total capacity.
};

// Writes to a std::string.
class StringWriter : public Writer {
 public:
  explicit StringWriter(std::string* const str) : str_(str) {}
  WP2_NO_DISCARD bool Reserve(size_t size) override;
  WP2_NO_DISCARD bool Append(const void* data, size_t data_size) override;

 private:
  std::string* const str_;
};

// Writes to a Data instance.
class DataWriter : public Writer {
 public:
  explicit DataWriter(Data* const data) : data_(data), size_(0) {}

  WP2_NO_DISCARD bool Reserve(size_t size) override;
  WP2_NO_DISCARD bool Append(const void* data, size_t data_size) override;
  size_t size() const { return size_; }  // Number of appended bytes.

 private:
  Data* const data_;  // Pointer to first byte and total capacity.
  size_t size_;       // Final size (is not increased by Reserve() calls).
};

// Counts the bytes.
class Counter : public Writer {
 public:
  WP2_NO_DISCARD bool Append(const void* data, size_t data_size) override;

  size_t size_ = 0;  // Number of appended bytes.
};

//------------------------------------------------------------------------------
// Rectangle struct. Rectangle(0, 0, 1, 1) represents the top-left most pixel.

struct Rectangle {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;

  Rectangle() = default;
  Rectangle(uint32_t rect_x, uint32_t rect_y, uint32_t rect_width,
            uint32_t rect_height)
      : x(rect_x), y(rect_y), width(rect_width), height(rect_height) {}
  bool operator==(const Rectangle& other) const {
    return x == other.x && y == other.y && width == other.width &&
           height == other.height;
  }
  bool operator!=(const Rectangle& other) const { return !(*this == other); }

  inline uint64_t GetArea() const { return (uint64_t)width * height; }
  inline bool Contains(uint32_t px, uint32_t py) const {
    return (px >= x && px < x + width) && (py >= y && py < y + height);
  }
  inline bool Contains(const WP2::Rectangle& r) const {
    if (r.GetArea() == 0) return Contains(r.x, r.y);
    return Contains(r.x, r.y) &&
           Contains(r.x + r.width - 1, r.y + r.height - 1);
  }
  inline bool Intersects(const WP2::Rectangle& r) const {
    return ((x >= r.x && x < r.x + r.width) || (r.x >= x && r.x < x + width)) &&
           ((y >= r.y && y < r.y + r.height) || (r.y >= y && r.y < y + height));
  }
  // Returns the maximum rectangle covered by both this and 'r'.
  Rectangle ClipWith(const Rectangle& r) const;
  // Returns the minimum rectangle covering both this and 'r'.
  Rectangle MergeWith(const Rectangle& r) const;
  // Returns the areas covered by this but not covered by 'r'.
  void Exclude(const Rectangle& r, Rectangle* top, Rectangle* bot,
               Rectangle* lft, Rectangle* rgt) const;
};

// Follows orientation tags defined in JEITA CP-3451C (section 4.6.4.A)
enum class Orientation {
  kOriginal = 0,
  kFlipLeftToRight,  // Mirror horizontally = vertical symmetry axis
  k180,
  kFlipTopToBottom,      // Mirror vertically = horizontal symmetry axis
  kFlipBotLeftToTopRgt,  // 90 clockwise then mirror horizontally
  k90Clockwise,
  kFlipTopLeftToBotRgt,  // 270 clockwise then mirror horizontally
  k270Clockwise
};

//------------------------------------------------------------------------------
// Main Input/Output buffer structure

class ArgbBuffer {
 public:
  explicit ArgbBuffer(WP2SampleFormat format_in = WP2_Argb_32) noexcept;
  ArgbBuffer(const ArgbBuffer&) = delete;  // No invisible copy, for safety.
  ArgbBuffer(ArgbBuffer&&) noexcept;
  ~ArgbBuffer() { Deallocate(); }
  ArgbBuffer& operator=(ArgbBuffer&&) noexcept;

  // Returns true if this buffer is empty (has no pixel).
  bool IsEmpty() const { return (pixels_ == nullptr); }
  // Deallocates this buffer's memory (including metadata) and resets it empty.
  void Deallocate();

  // Returns an opaque pointer to the first channel element of row 'y'.
  const void* GetRow(uint32_t y) const;
  void* GetRow(uint32_t y);
  const uint8_t* GetRow8(uint32_t y) const;
  uint8_t* GetRow8(uint32_t y);
  const uint16_t* GetRow16(uint32_t y) const;
  uint16_t* GetRow16(uint32_t y);
  // Returns memory location for sample x,y as a generic pointer. Rather slow.
  const uint8_t* GetPosition(uint32_t x, uint32_t y) const;
  uint8_t* GetPosition(uint32_t x, uint32_t y);

  // Sets the format but only if the buffer is empty.
  WP2Status SetFormat(WP2SampleFormat format_in);

  // Returns true if this buffer points to a non-owned memory (View).
  bool IsView() const { return is_external_memory_; }
  // Returns true if this buffer points to slow memory.
  bool IsSlow() const { return is_slow_memory_; }

  // Resizes this buffer to dimensions 'new_width' x 'new_height'.
  // If it's a view, success depends on whether it's already exactly well-sized.
  // Otherwise it will try to allocate the exact size, if not already the case.
  // The content of this buffer either stays the same or is uninitialized.
  // Previous metadata is discarded and deallocated.
  // Returns WP2_STATUS_OK upon success (no memory error or invalid arguments).
  WP2_NO_DISCARD WP2Status Resize(uint32_t new_width, uint32_t new_height);

  // Calls Resize() with dimensions of 'src' and copies the content of 'src'
  // into this buffer. Only WP2_Argb_32 format is accepted. Previous metadata is
  // discarded and deallocated, new metadata is not copied.
  // Returns WP2_STATUS_OK upon success.
  WP2_NO_DISCARD WP2Status CopyFrom(const ArgbBuffer& src);

  // Same as CopyFrom() but allows this buffer to be in any format. 'src' should
  // be in WP2_Argb_32 format. Previous metadata is discarded and deallocated,
  // new metadata is not copied. Returns WP2_STATUS_OK upon success.
  WP2_NO_DISCARD WP2Status ConvertFrom(const ArgbBuffer& src);

  // Calls Resize() and copies 'samples' in 'input_format' into this buffer,
  // converting if necessary (only towards WP2_Argb_32).
  // Returns WP2_STATUS_OK upon success.
  WP2_NO_DISCARD WP2Status Import(WP2SampleFormat input_format,
                                  uint32_t new_width, uint32_t new_height,
                                  const uint8_t* samples,
                                  uint32_t samples_stride);

  // Copies a number of pixels equal to this buffer's width from 'samples' in
  // 'input_format' into this buffer at 'row_index', converting if necessary
  // (only towards WP2_Argb_32). Buffer should be already allocated.
  // Returns WP2_STATUS_OK upon success.
  WP2_NO_DISCARD WP2Status ImportRow(WP2SampleFormat input_format,
                                     uint32_t row_index,
                                     const uint8_t* samples);

  // Fills the area or just a rectangle border defined by 'window' with 'color'.
  // The rectangle will be clipped to the dimension of the buffer.
  void Fill(const Rectangle& window, Argb32b color);
  void Fill(const Rectangle& window, Argb38b color);
  void Fill(Argb32b color);  // fills the whole buffer
  void Fill(Argb38b color);  // fills the whole buffer
  // Draw edges in the rectangle. 'edges_to_draw' is an OR combination of:
  //  1: top-edge,    2: left-edge  4: bottom-edge  8: right edge
  // 16: horizontal middle edge   32: vertical middle edge
  void DrawRect(const Rectangle& window, Argb32b color,
                uint32_t edges_to_draw = 0x0f);
  // 'color' must contain WP2FormatBpp(format) bytes.
  void Fill(const Rectangle& window, const uint8_t color[]);
  void Fill(const Rectangle& window, const uint16_t color[]);

  // Composites this buffer on the given background color.
  WP2_NO_DISCARD WP2Status CompositeOver(Argb32b background);
  // Composites this buffer on the given background buffer. Buffer size must
  // match.
  WP2_NO_DISCARD WP2Status CompositeOver(const ArgbBuffer& background);
  // Composites the given buffer on top of this buffer. Buffer size must match.
  // If 'window' is not empty, only this area is composited.
  WP2_NO_DISCARD WP2Status CompositeUnder(const ArgbBuffer& foreground,
                                          const Rectangle& window = {});

  // Deallocates then initializes this buffer as a view pointing to 'src'.
  // Previous metadata is discarded and deallocated, new metadata is not copied
  // nor pointed to. The view may modify the pixels of 'src'.
  // Returns WP2_STATUS_OK upon success.
  WP2_NO_DISCARD WP2Status SetView(const ArgbBuffer& src);
  // Same as above but the view is a 'window' of 'src' buffer.
  WP2_NO_DISCARD WP2Status SetView(const ArgbBuffer& src,
                                   const Rectangle& window);

  // Deallocates then initializes this buffer as a view pointing to the external
  // 'samples'. If this buffer 'is_slow', fewer read/write operations will
  // happen during encoding/decoding, at the expense of processing time and/or
  // internal memory usage. Returns WP2_STATUS_OK upon success.
  WP2_NO_DISCARD WP2Status SetExternal(uint32_t new_width, uint32_t new_height,
                                       uint8_t* samples, uint32_t new_stride,
                                       bool is_slow = false);
  WP2_NO_DISCARD WP2Status SetExternal(uint32_t new_width, uint32_t new_height,
                                       uint16_t* samples, uint32_t new_stride,
                                       bool is_slow = false);

  // Scans the buffer for the presence of non fully opaque alpha values.
  // Returns true in such case. Otherwise returns false (indicating that the
  // alpha plane can be ignored altogether e.g.).
  bool HasTransparency() const;

  // Computes PSNR, SSIM, LSIM or PSNRHVS distortion metric between two images.
  // Results are in dB, stored in result[] in the same order as the channels
  // in the format, with result[4] being the aggregate metric for all channels.
  // For PSNRHVS, the result[] content is A/Y/U/V/All. Warning: this function is
  // rather CPU-intensive.
  WP2_NO_DISCARD WP2Status GetDistortion(const ArgbBuffer& ref,
                                         MetricType metric_type,
                                         float result[5]) const;
  // Same as above but computes distortion metric on a sub window.
  WP2_NO_DISCARD WP2Status GetDistortion(const ArgbBuffer& ref,
                                         const Rectangle& window,
                                         MetricType metric_type,
                                         float result[5]) const;
  // Same as above, but computes the distortion at a given x,y point (slow!).
  // metric_type should not be PSNRHVS.
  WP2_NO_DISCARD WP2Status GetDistortion(const ArgbBuffer& ref, uint32_t x,
                                         uint32_t y, MetricType metric_type,
                                         float* result) const;

  // Computes the distortion when the image is composited on a white or black
  // background and returns the worst of the two. For images with no
  // transparency, distortion is only computed once since the background doesn't
  // matter.
  WP2_NO_DISCARD WP2Status GetDistortionBlackOrWhiteBackground(
      const ArgbBuffer& ref, MetricType metric_type, float result[5]) const;
  // Same as above but computes distortion metric on a sub window.
  WP2_NO_DISCARD WP2Status GetDistortionBlackOrWhiteBackground(
      const ArgbBuffer& ref, const Rectangle& window, MetricType metric_type,
      float result[5]) const;

  // Simple in-place 2x down-sampler. Cheap! (box-average)
  void SimpleHalfDownsample();

  // Should always be WP2_Argb_32 for encoding.
  WP2_NO_DISCARD WP2SampleFormat format() const { return format_; }
  // Number of pixels on the horizontal axis.
  WP2_NO_DISCARD uint32_t width() const { return width_; }
  // Number of pixels on the vertical axis.
  WP2_NO_DISCARD uint32_t height() const { return height_; }
  // Difference in bytes from a scanline to the next one.
  WP2_NO_DISCARD uint32_t stride() const { return stride_; }
  // Returns the rectangle representing the buffer.
  Rectangle AsRect() const { return {0, 0, width_, height_}; }

 public:
  Metadata metadata_;  // ICC / XMP / EXIF metadata.

 private:
  void FillImpl(const Rectangle& window, const void* color);
  WP2Status SetExternalImpl(uint32_t new_width, uint32_t new_height,
                            void* samples, uint32_t new_stride, bool is_slow);
  // If 'include_alpha_in_all', result[4] will take alpha into account (if any).
  // Otherwise alpha can still have an influence since we use premultiplied
  // RGB, except if the image is all black.
  WP2_NO_DISCARD WP2Status GetDistortion(const ArgbBuffer& ref,
                                         MetricType metric_type,
                                         bool include_alpha_in_all,
                                         float result[5]) const;

  // To be used after "using std::swap;".
  friend void swap(ArgbBuffer& a, ArgbBuffer& b);

  WP2SampleFormat format_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t stride_ = 0;

  void* pixels_ = nullptr;           // Pointer to samples.
  bool is_external_memory_ = false;  // If false, 'pixels' is 'private_memory'.
  void* private_memory_ = nullptr;   // Internally allocated memory.
  bool is_slow_memory_ = false;  // If true, the memory is considered 'slow' and
                                 // the number of reads/writes will be reduced.
};

//------------------------------------------------------------------------------

// Can be inherited to get notified of progress and/or to abort encoding or
// decoding.
class ProgressHook {
 public:
  virtual ~ProgressHook() = default;
  // Will be regularly called with 'progress' advancing from 0.f (decoding
  // didn't start) to 1.f (decoding is done). For animations, 'progress' will
  // be reset to 0.f at each frame beginning; otherwise it never decreases.
  // Return true to continue, false to stop decoding.
  virtual bool OnUpdate(double progress) = 0;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_WP2_BASE_H_ */
