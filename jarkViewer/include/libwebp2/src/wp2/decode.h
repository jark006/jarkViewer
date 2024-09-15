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
//  Main decoding functions for WP2 images.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_WP2_DECODE_H_
#define WP2_WP2_DECODE_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "./base.h"
#include "./debug.h"
#include "./format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------
// Features gathered from the bitstream

struct BitstreamFeatures {
  uint32_t raw_width;   // Width in pixels, as read from the bitstream.
  uint32_t raw_height;  // Height in pixels, as read from the bitstream.

  Orientation orientation;  // Rotation or mirroring applied during decoding.
  uint32_t width;           // Width after the orientation is applied.
  uint32_t height;          // Height after the orientation is applied.
  TileShape tile_shape;     // Tile sizing pattern.
  uint32_t rgb_bit_depth;   // RGB depth in bits. 8 or 10. Alpha is always 8.

  bool is_opaque;         // True if no alpha. False has no guarantee.
  bool is_premultiplied;  // If true, color information in translucent pixels
                          // was partially or totally discarded at encoding.
  bool is_animation;      // True if the bitstream is an animation.
  bool has_preview;       // True if the bitstream has a preview image
                          // (irrespective of the preview color).

  RGB12b preview_color;  // Average color in RGB444 format.

  bool has_icc;            // Indicates the presence of ICC information.
  bool has_trailing_data;  // Trailing data is present (XMP, EXIF, ...).
  TransferFunction transfer_function;

  bool loop_forever;         // Play once if false, forever if true (anim only).
  Argb38b background_color;  // Color to dispose the canvas with (anim only).

  size_t header_size;  // Byte size of header without preview and ICC chunks.

 public:
  // Extracts features from the bitstream. Returns:
  //   - WP2_STATUS_OK when the features are successfully retrieved.
  //   - WP2_STATUS_NOT_ENOUGH_DATA when more data is needed to retrieve the
  //     features from the header.
  //   - Any other error status in case of parsing failure.
  WP2_NO_DISCARD WP2Status Read(const uint8_t* data, size_t data_size) {
    return InitInternal(WP2_ABI_VERSION, data, data_size);
  }

 private:
  // Internal, version-checked, entry point.
  WP2Status InitInternal(int, const uint8_t*, size_t);
};

// For animations.
struct FrameFeatures {
  uint32_t duration_ms = 0;  // Milliseconds during which this frame is visible.
  Rectangle window;          // The canvas area modified by this frame
                             // (BitstreamFeatures::orientation included).
  bool is_last = false;      // If true, there is no frame after this one.

  // Index of first frame needed to decode this frame. If the value is the same
  // as this frame's index, it means this frame is independent of any other one.
  uint32_t last_dispose_frame_index = 0;
};

//------------------------------------------------------------------------------
// Main decoding call

struct DecoderConfig {
  // 0 to disable multi-threading. Any other value represents the maximum number
  // of simultaneous extra threads (can be as large as 2^31 - 1).
  uint32_t thread_level = 0;

  // If enabled, the filters smooth lossy compression artifacts out.
  // Not applying them can reduce decoding duration for time-sensitive cases.
  bool enable_deblocking_filter = true;    // Reduces blocky artifacts.
  bool enable_directional_filter = false;  // Reduces noise and ringing.
  bool enable_restoration_filter = false;  // Denoises and enhances edges.
  bool enable_alpha_filter = true;  // Edge-preserving blur for alpha only.

  // Amplitude factor for the grain generation [0(=off) .. 100]
  uint8_t grain_amplitude = 0;

  // Output pixels earlier or in bigger batches. Only used by Decoder classes.
  enum class IncrementalMode {
    // TODO(yguyon): Add a FULL_FRAME mode (no partial frame decoding)
    FULL_TILE = 0,         // Decode each frame tile only when fully available.
    PARTIAL_TILE_CONTEXT,  // Decode frame tiles even if partially available,
                           // with a context switch or fibers, if compiled.
    // TODO(yguyon): Add a PARTIAL_TILE_THREAD mode (suspend a thread instead)
  };
  IncrementalMode incremental_mode = IncrementalMode::PARTIAL_TILE_CONTEXT;

  // Inherit ProgressHook and reference the instance here to get notified of
  // progress and/or to abort decoding.
  ProgressHook* progress_hook = nullptr;

  DecoderInfo* info = nullptr;  // if not null, report internal stats and info

  static const DecoderConfig kDefault;
};

// This function decodes the bitstream into 'output_buffer' as Argb samples.
// If it's an animation, it will only decode the first frame.
// TODO(maryla): add support for decoding first frame with preframes.
// Returns decoding status (which should be WP2_STATUS_OK if the decoding was
// successful). Note that 'output_buffer' cannot be null.
// If 'output_buffer' already fits the image exactly, it won't be reallocated.
// Otherwise, if it doesn't point to external memory, it will be (re)allocated
// to the exact width and height of the image.
WP2_EXTERN WP2_NO_DISCARD WP2Status
Decode(const uint8_t* data, size_t data_size, ArgbBuffer* output_buffer,
       const DecoderConfig& config = DecoderConfig::kDefault);
WP2_EXTERN WP2_NO_DISCARD WP2Status
Decode(const std::string& data, ArgbBuffer* output_buffer,
       const DecoderConfig& config = DecoderConfig::kDefault);

//------------------------------------------------------------------------------
// Specialized call

// Decodes the image bitstream in '*input_data' directly into the pre-allocated
// buffer 'output_buffer'. The maximum storage available in this buffer is
// indicated by 'output_buffer_size'. The parameter 'output_stride' specifies
// the distance (in bytes) between scanlines. Hence, 'output_buffer_size' is
// expected to be at least 'output_stride x picture-height'.
// If an error occurred (insufficient storage, ...) the corresponding status is
// returned. Otherwise, the return value is WP2_STATUS_OK.

// The output format is Argb 32b (premultiplied). The ordering of samples in
// memory is:     A, r, g, b, A, r, g, b, A, r, g, b...
// in scan order with 8b per channel (endian-independent).
WP2_EXTERN WP2_NO_DISCARD WP2Status DecodeArgb_32(
    const uint8_t* input_data, size_t input_data_size, uint8_t* output_buffer,
    uint32_t output_stride, size_t output_buffer_size,
    const DecoderConfig& config = DecoderConfig::kDefault);

// Decodes to a custom color space defined by fixed-point 'rgb_to_ccsp_matrix'
// (row-ordered) and its 'rgb_to_ccsp_shift' (applied after). The output samples
// come from alpha-premultiplied RGB and maximum output precision is 10 bits.
// Examples can be found in ../extras/extras.h.
// The 'c0', 'c1' and 'c2' channels are output in buffers of signed 16 bits.
// The 'a' alpha buffer, if not null, is stored in 8 bits (unsigned) every 16
// bits (filled with 255 if the image does not contain any non-opaque alpha).
// The '_step' parameters specify the distances in samples between scanlines and
// must be multiples of two. '_buffer_size' is the byte length of a channel.
// TODO(yguyon): Specify endianness?
WP2_EXTERN WP2_NO_DISCARD WP2Status
Decode(const uint8_t* input_data, size_t input_data_size,
       const int16_t rgb_to_ccsp_matrix[9], uint32_t rgb_to_ccsp_shift,
       int16_t* c0_buffer, uint32_t c0_step, size_t c0_buffer_size,
       int16_t* c1_buffer, uint32_t c1_step, size_t c1_buffer_size,
       int16_t* c2_buffer, uint32_t c2_step, size_t c2_buffer_size,
       int16_t* a_buffer = nullptr, uint32_t a_step = 0,
       size_t a_buffer_size = 0, Metadata* metadata = nullptr,
       const DecoderConfig& config = DecoderConfig::kDefault);

//------------------------------------------------------------------------------
// Preview

// If 'output_buffer' is not already allocated, allocates it to the size of the
// WP2 image stored in 'data'. Otherwise uses the pre-allocated 'output_buffer'
// dimensions. Then fills 'output_buffer' with the decoded preview image if any,
// or else with the preview color.
// Returns WP2_STATUS_OK or an error.
WP2_EXTERN WP2_NO_DISCARD WP2Status ExtractPreview(const uint8_t* data,
                                                   size_t data_size,
                                                   ArgbBuffer* output_buffer);

//------------------------------------------------------------------------------
// WebP2 container parsing

// Types of byte chunk that can be found in a WebP2 bitstream, in order:
enum class ChunkType { kHeader, kPreview, kIcc, kFrame, kXmp, kExif };

// Retrieves the chunk of 'chunk_type' within the 'bitstream_data' and outputs
// it through the given 'writer'.
// 'frame_index' (0-based) is ignored unless 'chunk_type' is kFrame.
// Returns WP2_STATUS_OK, a parsing error or a Writer issue.
// If the chunk is absent, returns WP2_STATUS_OK without writing anything.
WP2_NO_DISCARD WP2Status GetChunk(const uint8_t bitstream_data[],
                                  size_t bitstream_size, ChunkType chunk_type,
                                  Writer* writer, uint32_t frame_index = 0);

// Parses the whole bitstream to count the number of frames.
// Returns WP2_STATUS_OK or an error.
// 'num_frames' is a lower bound in case of corrupted or truncated bitstream.
WP2_NO_DISCARD WP2Status GetNumFrames(const uint8_t bitstream_data[],
                                      size_t bitstream_size,
                                      size_t* num_frames);

//------------------------------------------------------------------------------
// Animation and incremental decoding
//
// This API allows streamlined decoding of animated and/or partial data.
//  - Use an ArrayDecoder if input is a contiguous array either fully available
//    at start or that gets bigger as data comes in.
//  - Use a StreamDecoder if input data is streamed (available by
//    non-overlapping, in-order chunks).
//  - Inherit from CustomDecoder if data must be fetched (for example to read a
//    file). Fetch(), Discard() and Reset() must be overridden.
//
// If supplied, 'output_buffer' MUST NOT be changed until decoding is complete.
// It can either be a view pointing to some external memory (if so, it must
// match the image dimensions), or it is automatically allocated by the Decoder.
//
// If present and when fully decoded, ICC, EXIF and/or XMP chunks will be stored
// in 'output_buffer.metadata'.
//
// Code example for an animation encoded into a bitstream DATA of SIZE bytes:
//
//   WP2::ArrayDecoder decoder(DATA, SIZE);
//   uint32_t duration;
//   while (decoder.ReadFrame(&duration)) {
//     // A frame is ready to be fully displayed for 'duration' milliseconds.
//     // GetPixels() contains it until the next ReadFrame() call.
//   }
//   if (decoder.GetStatus() != WP2_STATUS_OK) { /* Handle error */ }
//
// Code example for a still image encoded into chunks retrieved by GET_CHUNK():
//
//   WP2::StreamDecoder decoder;
//   while (GET_CHUNK(&CHUNK)) {
//     decoder.AppendInput(CHUNK.DATA, CHUNK.SIZE);
//     if (decoder.ReadFrame()) {
//       // decoder.GetPixels() can be fully displayed now.
//       break;  // Still image = only one expected frame.
//     }
//     if (decoder.Failed()) break;
//     // decoder.GetPixels() within decoder.GetDecodedArea() can be shown now.
//   }
//   if (decoder.GetStatus() != WP2_STATUS_OK) { /* Handle error */ }

class Decoder {
 public:
  // Deletes the Decoder object and associated memory. Also deletes the output
  // unless the 'output_buffer' was specified by the user.
  virtual ~Decoder();

  // Decodes the given bitstream until a frame is ready, then halts.
  // Returns true if the current frame is completely decoded without any error.
  // If so, 'duration_ms' can be retrieved if not null (0 ms for a still image).
  // Does not halt decoding nor return true for skipped frames.
  bool ReadFrame(uint32_t* duration_ms = nullptr);

  // Returns WP2_STATUS_OK if the decoding is complete, in other words if the
  // end of the bitstream was reached (including the trailing metadata if any).
  // Otherwise returns WP2_STATUS_NOT_ENOUGH_DATA or an error that occurred
  // during ReadFrame().
  WP2_NO_DISCARD WP2Status GetStatus() const;

  // Returns true if an error happened.
  bool Failed() const;

  // Returns the image features or nullptr if not yet available.
  const BitstreamFeatures* TryGetDecodedFeatures() const;

  // Returns the frame features at 'frame_index' (0-based) or nullptr if not yet
  // available.
  const FrameFeatures* TryGetFrameDecodedFeatures(uint32_t frame_index) const;

  // Returns true and the 'offset' and/or 'length' in bytes in the bitstream of
  // the frame at 'frame_index' or false if not yet available.
  bool TryGetFrameLocation(uint32_t frame_index, size_t* offset,
                           size_t* length = nullptr) const;

  // Returns the 0-based index of the current frame, which is totally decoded if
  // the last call to ReadFrame() returned true, or partially decoded otherwise.
  // It is equal to or greater than the total number of skipped frames.
  // Meaningless if all frames were decoded or skipped.
  uint32_t GetCurrentFrameIndex() const;

  // Returns the number of decoded FrameFeatures. Never decreases (even rewind).
  uint32_t GetNumFrameDecodedFeatures() const;

  // Returns the available rectangular area of the current frame so far.
  // Pixels included in this area will not change until next frame.
  // The area will not decrease until next frame.
  Rectangle GetDecodedArea() const;

  // Returns the buffer (or the view to the buffer) containing the pixels.
  const ArgbBuffer& GetPixels() const { return *output_; }

  // Disables pixel decoding for the next 'num_frames_to_skip' (starting at
  // GetCurrentFrameIndex(), included), but still reads frame features.
  // Useful for going directly to a timestamp or getting frames info without
  // decoding everything ('kMaxNumFrames' to skip all remaining frames).
  void SkipNumNextFrames(uint32_t num_frames_to_skip);

  // Resets the anim decoder to the first frame but keeps known frame features.
  // The decoder will expect the same data, from the beginning of the bitstream.
  void Rewind();

  // Pointer-to-implementation forward declaration.
  struct State;

 protected:
  Decoder(State* state, const DecoderConfig& config, ArgbBuffer* output_buffer);
  virtual void DecodeAvailableData();

  State* const state_;

 private:
  WP2Status DecodeAvailableTiles();
  WP2Status DiscardAvailableTiles();

  const DecoderConfig& config_;
  ArgbBuffer* output_;
  ArgbBuffer internal_output_;
  BitstreamFeatures features_;
};

class ArrayDecoder : public Decoder {
 public:
  // Constructs a Decoder with the given 'config' and 'output_buffer'.
  // The latter can be null or empty (it will be allocated by the Decoder) or
  // a view to a user-allocated, correctly-sized buffer (use BitstreamFeatures
  // to find the width/height in the bitstream beforehand).
  explicit ArrayDecoder(const DecoderConfig& config = DecoderConfig::kDefault,
                        ArgbBuffer* output_buffer = nullptr);
  // Same as the constructor above plus calling SetInput(data, size).
  explicit ArrayDecoder(const uint8_t* data, size_t size,
                        const DecoderConfig& config = DecoderConfig::kDefault,
                        ArgbBuffer* output_buffer = nullptr);

  // Updates the input data buffer.
  // Note that the value of the 'data' pointer can change between calls to
  // ReadFrame(), for instance when the data buffer is resized to fit larger
  // data. 'size' must never decrease between calls.
  void SetInput(const uint8_t* data, size_t size);
};

class StreamDecoder : public Decoder {
 public:
  // Constructs a Decoder with the given 'config' and 'output_buffer'.
  explicit StreamDecoder(const DecoderConfig& config = DecoderConfig::kDefault,
                         ArgbBuffer* output_buffer = nullptr);

  // Registers the next chunk of data from the stream.
  // If 'data_is_persistent', 'data' must outlive the next AppendInput() call.
  // If not 'data_is_persistent' (default), 'data' will be copied internally.
  // If Rewind() is called, AppendInput() must also be called with 'data'
  // pointing to the beginning of the bitstream.
  void AppendInput(const uint8_t* data, size_t size,
                   bool data_is_persistent = false);
};

class CustomDecoder : public Decoder {
 public:
  // Constructs a Decoder with the given 'config' and 'output_buffer'.
  explicit CustomDecoder(const DecoderConfig& config = DecoderConfig::kDefault,
                         ArgbBuffer* output_buffer = nullptr);

 private:
  // This will get called during ReadFrame() to fetch more data.
  // Override to set 'available_bytes' and 'num_available_bytes', which should
  // be at least 'num_requested_bytes' if possible.
  // No data will be copied and 'num_available_bytes' must not decrease between
  // calls (meaning Fetch() must return at least the same bytes, possibly more)
  // until Discard() is called. The buffer can change as long as the values
  // stay the same, and the buffer does not need to outlive ReadFrame().
  virtual void Fetch(size_t num_requested_bytes,
                     const uint8_t** available_bytes,
                     size_t* num_available_bytes) = 0;

  // This will get called during ReadFrame() to signal that 'num_bytes' (which
  // were previously fetched or not yet) will not be used anymore, and can be
  // freed. Fetch() must not return them in the future, only the data that comes
  // after. Fetch() is called right after Discard() to make sure the remaining
  // available bytes (if any) are up-to-date.
  virtual void Discard(size_t num_bytes) = 0;

  // This will get called during Rewind() to signal that the bitstream will need
  // to be read from the beginning, including previously discarded bytes.
  virtual void Reset() = 0;

  // Internal.
  void DecodeAvailableData() override;
  friend class CustomDataSource;
};

}  // namespace WP2

#endif /* WP2_WP2_DECODE_H_ */
