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
//   WP2 animation encoder: internal header.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_ENC_ANIM_ANIM_ENC_H_
#define WP2_ENC_ANIM_ANIM_ENC_H_

#include <cstdint>

#include "src/common/progress_watcher.h"
#include "src/utils/csp.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

// A full final frame.
struct Frame {
  uint32_t GetWidth() const;
  uint32_t GetHeight() const;

  // Either 'rgb_pixels' or 'ccsp_pixels' is empty.
  ArgbBuffer rgb_pixels;

  YUVPlane ccsp_pixels;     // In custom color space.
  CSPMtx ccsp_to_rgb = {};  // Should give alpha-premultiplied RGB samples
                            // once applied to 'ccsp_pixels'.

  uint32_t duration_ms = 0;  // Must be > 0.

  bool force_dispose = false;
  bool force_lossless = false;
};

// Returns false if at least one of the 'frames' is translucent.
bool AllFramesAreOpaque(const Vector<Frame>& frames);

// Encodes the header of a frame into its "ANMF" chunk.
// 'image_width' and 'image_height' are the unoriented dimensions of the whole
// canvas in pixels. 'duration' is in milliseconds. 'window' is in pixels,
// unoriented, not padded, within the whole canvas.
WP2Status WriteANMF(uint32_t image_width, uint32_t image_height,
                    uint32_t duration, const Rectangle& window, bool dispose,
                    bool blend, bool is_last, Writer* output);

//------------------------------------------------------------------------------

// Used to compute and store which parts (subframes) of a frame are different
// from the previous one.
class RectangleGroup {
 public:
  explicit RectangleGroup(uint32_t max_num_rectangles);

  // Resets to 0 rectangle.
  void Clear();

  // Adds a new rectangle or increases an existing one, then simplifies the
  // rectangles while keeping the same covered area if possible.
  void AddPoint(uint32_t x, uint32_t y);

  // Clears and sets 1 rectangle.
  void SetRectangle(const Rectangle& rectangle);

  // TODO(yguyon): Remove accessors used only for testing.
  uint32_t GetNumRectangles() const;
  const Rectangle& GetRectangle(uint32_t index) const;
  Rectangle GetBoundingRectangle() const;
  uint64_t GetCumulativeArea() const;

 protected:
  // Keep one extra for temp and one extra for the regular frame (the one after
  // the preframes).
  static constexpr uint32_t kMaxStoredRectangles = kMaxNumPreframes + 2;

  // Merges the rectangles at 'a_index' and 'b_index' into 'a' and remove 'b'.
  void MergeRectangles(uint32_t a_index, uint32_t b_index);

  // Merges a pair of rectangles if it does not increase the covered area.
  bool TryToMergeARectangleWith(uint32_t rectangle_index,
                                uint32_t* other_rectangle_index);

  const uint32_t max_num_rectangles_;
  uint32_t num_rectangles_ = 0;
  Rectangle rectangles_[kMaxStoredRectangles];
};

//------------------------------------------------------------------------------

// A subframe can represent a whole frame or only part of it.
struct Subframe {
  // Returns the whole frame rect (different from 'window').
  Rectangle GetFrameRect() const;

  // Either 'rgb_pixels' or 'ccsp_pixels' is empty.
  ArgbBuffer rgb_pixels;  // In alpha-premultiplied format.

  YUVPlane ccsp_pixels;     // In custom color space.
  CSPMtx ccsp_to_rgb = {};  // Should give alpha-premultiplied RGB samples
                            // once applied to 'ccsp_pixels'.

  uint32_t duration_ms = 0;  // May be 0, for so called "preframes".
  // Window to be encoded. Areas of rgb_pixels/ccsp_pixels outside this window
  // are ignored.
  Rectangle window;
  // If true, at decoding time the full buffer should be filled with the
  // background color before decoding this subframe.
  bool dispose = false;
  // If true, at decoding time the content of this subframe (rgb_pixels or
  // ccsp_pixels) should be alpha blended on top of the previous frame instead
  // of overwriting it. If dispose is also true, this subframe must be blended
  // on the background color instead.
  bool blend = false;
};

class SubframeEncoder {
 public:
  // Setting 'image_has_alpha' to false forbids any alpha sample in the whole
  // animation. 'tile_shape' is also global to the whole animation and must be a
  // concrete size, not TILE_SHAPE_AUTO. It is used to sanity check that the
  // tile size for all subframes match this one.
  SubframeEncoder(bool image_has_alpha, bool image_is_premultiplied,
                  TileShape tile_shape);

  // The  EncoderConfig, and therefore quality setting, can vary per frame.
  WP2Status EncodeSubframe(const EncoderConfig& config,
                           const Subframe& subframe, bool is_last,
                           const ProgressRange& progress, Writer* output) const;

 private:
  TileShape tile_shape_;
  const bool image_has_alpha_;
  const bool image_is_premultiplied_;
};

//------------------------------------------------------------------------------

// Class that encodes frames, possibly splitting them into multiple subframes.
class FrameEncoder {
 public:
  FrameEncoder(const EncoderConfig& config, Argb38b background_color,
               const Vector<Frame>& frames, bool image_is_premultiplied,
               const ProgressRange& progress);

  // Encodes all frames. Returns WP2_STATUS_OK if no error occurred.
  WP2Status Encode(Writer* output);

 protected:
  // Saves pixels to the current canvas, for later comparison.
  WP2Status SaveSubframeToCanvas(const ArgbBuffer& subframe,
                                 const Rectangle& window);
  WP2Status SaveFrameToCanvas(const ArgbBuffer& frame,
                              const RectangleGroup& different_pixels);

  // Compares pixels to the current canvas.
  WP2Status GetPixelsDifferentFromCanvas(
      const ArgbBuffer& frame, RectangleGroup* different_pixels) const;

  // Same as above but for a custom color space.
  WP2Status SaveSubframeToCanvas(const YUVPlane& frame,
                                 const CSPMtx& ccsp_to_rgb,
                                 const Rectangle& window);
  WP2Status SaveFrameToCanvas(const YUVPlane& subframe,
                              const CSPMtx& ccsp_to_rgb,
                              const RectangleGroup& different_pixels);
  WP2Status GetPixelsDifferentFromCanvas(
      const YUVPlane& frame, const CSPMtx& ccsp_to_rgb,
      RectangleGroup* different_pixels) const;

  // Encodes a preframe or a regular frame.
  WP2Status EncodeSubframe(const Frame& frame, bool dispose, bool is_preframe,
                           bool is_previous_preframe, const Rectangle& window,
                           const ProgressRange& progress, Writer* output) const;

  // Encodes one final frame (including its preframes).
  WP2Status EncodeFrame(const Frame& frame, bool dispose,
                        const RectangleGroup& different_pixels,
                        const ProgressRange& progress, Writer* output) const;

  const EncoderConfig& config_;
  const Vector<Frame>& frames_;

  // The canvas is used to keep track of the current pixel values, for
  // comparison with the next frame. Can be in RGB or custom color space.
  ArgbBuffer rgb_canvas_;
  YUVPlane ccsp_canvas_;
  CSPMtx ccsp_to_rgb_ = {};  // defined if !ccsp_canvas_.IsEmpty()
  const bool image_has_alpha_;
  const Argb32b background_color_;

  SubframeEncoder subframe_encoder_;
  const ProgressRange progress_;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_ANIM_ANIM_ENC_H_
