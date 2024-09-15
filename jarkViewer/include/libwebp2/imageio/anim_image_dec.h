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
//  All-in-one library to decode (possibly animated) input images.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_IMAGEIO_ANIM_IMAGE_DEC_H_
#define WP2_IMAGEIO_ANIM_IMAGE_DEC_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "./file_format.h"
#include "./imageio_util.h"
#include "src/utils/utils.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"
#include "src/wp2/format_constants.h"

#ifdef HAVE_CONFIG_H
#include "src/wp2/config.h"
#endif

namespace WP2 {

// Extracts the first XMP and ICCP blocks found in 'data' GIF bitstream to
// 'metadata' if they are not already there.
WP2Status ReadGIFMetadata(const uint8_t* data, size_t data_size,
                          Metadata* metadata);

// Can decode a still image or an animation. 'buffer' must not be changed
// between calls to ReadFrame(). Incremental decoding is not supported.
class ImageReader {
 public:
  // Reads 'file_path' and stores the content. Clears 'buffer'.
  ImageReader(const char* file_path, ArgbBuffer* buffer,
              FileFormat data_format = FileFormat::AUTO,
              LogLevel log_level = LogLevel::DEFAULT,
              size_t max_num_pixels = kMaxBufferArea);
  // 'data' must not change till the last ReadFrame() call. Clears 'buffer'.
  ImageReader(const uint8_t* data, size_t data_size, ArgbBuffer* buffer,
              FileFormat data_format = FileFormat::AUTO,
              LogLevel log_level = LogLevel::DEFAULT,
              size_t max_num_pixels = kMaxBufferArea);

  // 'data' must not change till the last ReadFrame() call. Clears 'buffer'.
  ImageReader(const std::string& data, ArgbBuffer* buffer,
              FileFormat data_format = FileFormat::AUTO,
              LogLevel log_level = LogLevel::DEFAULT,
              size_t max_num_pixels = kMaxBufferArea);

  // Decodes a frame to 'buffer'. 'is_last' and 'duration_ms' can be retrieved
  // if not null. If it's not an animation, returns the image as the only frame
  // with kInfiniteDuration. Returns WP2_STATUS_OK upon success.
  static constexpr uint32_t kInfiniteDuration = 0;
  WP2Status ReadFrame(bool* is_last = nullptr, uint32_t* duration_ms = nullptr);

  // Returns the number of times the animation should be played.
  // Should be called once all frames have been read.
  static constexpr uint32_t kInfiniteLoopCount = 0;
  uint32_t GetLoopCount() const;

  // Class to inherit for each format.
  class Impl : public WP2Allocable {
   public:
    Impl(ArgbBuffer* buffer, const uint8_t* data, size_t data_size,
         LogLevel log_level, size_t max_num_pixels)
        : data_(data),
          data_size_(data_size),
          buffer_(buffer),
          log_level_(log_level),
          max_num_pixels_(max_num_pixels) {}
    virtual ~Impl() = default;
    // Should return WP2_STATUS_OK if a frame is decoded or an error otherwise.
    // Must set 'loop_count_'.
    // Metadata must be set during the first call so that ReadImage() also reads
    // metadata from animations even if only the first frame is read.
    virtual WP2Status ReadFrame(bool* is_last, uint32_t* duration_ms) = 0;

    uint32_t GetLoopCount() const { return loop_count_; }

    WP2Status CheckData() const;  // verify data_ and buffer_ fields

   protected:
    WP2Status CheckDimensions(uint32_t width, uint32_t height) const;

    const uint8_t* const data_;
    const size_t data_size_;
    ArgbBuffer* const buffer_;  // user-supplied output
    uint32_t loop_count_ = kInfiniteLoopCount;
    const LogLevel log_level_;
    const size_t max_num_pixels_;
  };

 protected:
  // Sets 'impl_'. Sets 'status_' in case of error.
  void SetImplPNG(ArgbBuffer* buffer, LogLevel log_level,
                  size_t max_num_pixels);
  void SetImplJPEG(ArgbBuffer* buffer, LogLevel log_level,
                   size_t max_num_pixels);
  void SetImplJXL(ArgbBuffer* buffer, LogLevel log_level,
                  size_t max_num_pixels);
  void SetImplPNM(ArgbBuffer* buffer, LogLevel log_level,
                  size_t max_num_pixels);
  void SetImplTIFF(ArgbBuffer* buffer, LogLevel log_level,
                   size_t max_num_pixels);
  void SetImplGIF(ArgbBuffer* buffer, LogLevel log_level,
                  size_t max_num_pixels);
  void SetImplWEBP(ArgbBuffer* buffer, LogLevel log_level,
                   size_t max_num_pixels);
  void SetImplWP2(ArgbBuffer* buffer, LogLevel log_level, size_t max_num_pixels,
                  bool exact);
  void SetImplBMP(ArgbBuffer* buffer, LogLevel log_level,
                  size_t max_num_pixels);

  // Calls a function above depending on 'data_format'.
  void SetImpl(FileFormat data_format, LogLevel log_level,
               size_t max_num_pixels, ArgbBuffer* buffer);

  WP2::DecoderConfig config_;
  std::unique_ptr<Impl> impl_;
  DataView data_;    // Input. Points to 'file_bytes_' or to user-owned memory.
  Data file_bytes_;  // Empty unless a file was read.
  WP2Status status_ = WP2_STATUS_OK;
  bool is_done_ = false;
};

}  // namespace WP2

#endif  // WP2_IMAGEIO_ANIM_DEC_H_
