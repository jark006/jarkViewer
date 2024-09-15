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
// Internal constants.
//
// Authors: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_COMMON_CONSTANTS_H_
#define WP2_COMMON_CONSTANTS_H_

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "src/dsp/math.h"
#include "src/utils/ans.h"
#include "src/utils/utils.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

//------------------------------------------------------------------------------
// Lossy and common constants.

namespace WP2 {

// Maximum number of bytes used by a variable-length integer.
static constexpr uint32_t kMaxVarIntLength = 4;
// Maximum value expressible with a variable-length integer (included).
static constexpr uint32_t kMaxVarInt =
    0x7Fu + (0x80u << 7) + (0x80u << 14) + (0x100u << 21);  // ~514 MB
static_assert(kMaxChunkSize - 1u <= kMaxVarInt,
              "The maximum chunk size cannot be represented with var ints.");

// Maximum (inclusive) bytes per pixel.
static constexpr uint64_t kMaxNumBytesPerPixel =
#if defined(WP2_ENC_DEC_MATCH)
    // Each channel is sizeof(uint16_t) and its ANS hash sizeof(uint16_t).
    4 * 2 * sizeof(uint16_t);
#else
    4 * sizeof(uint16_t);
#endif

static constexpr char kChannelStr[4][2] = {"Y", "U", "V", "A"};
const char* const kPartitionMethodString[] = {
    "Multipass",   "Block-enc",    "Split-enc",  "Tile-enc",  "SplitRec-enc",
    "Area-enc",    "Sub-area-enc", "Exhaustive", "Fixed-4x4", "Fixed-8x8",
    "Fixed-16x16", "Fixed-32x32",  "Auto"};
STATIC_ASSERT_ARRAY_SIZE(kPartitionMethodString, WP2::NUM_PARTITION_METHODS);

// Number of sets of residual statistics. For now, there is one set per method.
// kMethod0 and kMethod1.
static constexpr uint32_t kNumResidualStats = 2;
// Maximum value used by StoreCoeffs to use the first dictionary.
// A value of kResidual1Max is an escape code to get to the second
// dictionary.
static constexpr uint32_t kResidual1Max = 12;
// Maximum value of consecutive 0s we store for residuals.
static constexpr uint32_t kResidualCons0Max = 6;
// Maximum quantized DC value that can be coded.
static constexpr uint32_t kMaxDcValue = (1u << 12) - 1u;
// Maximum quantized value that can be coded for AC coeffs.
static constexpr uint32_t kMaxCoeffValue =
    ((1u << kMaxCoeffBits) - 1u) + 3 + kResidual1Max;

static constexpr Argb38b kDefaultBackgroundColor{kAlphaMax, 0x0000u, 0x0000u,
                                                 0x0000u};

// Decoding progress proportion in [0:1] range.
static constexpr double kOnePercent = 0.01;   // For each header, ICC, ANMF etc.
static constexpr double kProgressEnd = 0.02;  // XMP+EXIF.
// Progress occupied by header + preview + ICC.
static constexpr double kProgressBeforeFrames = 3 * kOnePercent;
// Progress left after removing the header + preview + ICC + XMP+EXIF.
static constexpr double kProgressFrames =
    1. - kProgressBeforeFrames - kProgressEnd;
// Progress for all tiles within a frame minus ANMF + GlobalParams.
static constexpr double kProgressTiles = 1. - 2 * kOnePercent;

// Returns the tile width/height in pixels for a given TileShape and frame
// dimensions.
static inline uint32_t TileWidth(TileShape tile_shape, uint32_t frame_width,
                                 uint32_t frame_height) {
  switch (tile_shape) {
    case TILE_SHAPE_SQUARE_128:
      return 128;
    case TILE_SHAPE_SQUARE_256:
      return 256;
    case TILE_SHAPE_SQUARE_512:
      return 512;
    case TILE_SHAPE_WIDE:
      (void)frame_height;
      return std::min(Pad(frame_width, kMinBlockSizePix), kMaxTileSize);
    default:
      assert(false);
      return 0;
  }
}
static inline uint32_t TileHeight(TileShape tile_shape, uint32_t frame_width,
                                  uint32_t frame_height) {
  const uint32_t tile_width = TileWidth(tile_shape, frame_width, frame_height);
  switch (tile_shape) {
    case TILE_SHAPE_SQUARE_128:
    case TILE_SHAPE_SQUARE_256:
    case TILE_SHAPE_SQUARE_512:
      return tile_width;
    case TILE_SHAPE_WIDE:
      // Should this be smaller than width?
      return std::min(Pad(kMaxTilePixels / tile_width, kMinBlockSizePix),
                      kMaxTileSize);
    default:
      assert(false);
      return 0;
  }
}

}  // namespace WP2

//------------------------------------------------------------------------------
// Lossless constants.

namespace WP2L {

// Modes used in Group4 compression: pass is just about skipping the definition
// of a segment, horizontal mode is about defining the next two segments,
// vertical is about defining a segment with respect to the line above.
enum class Group4Mode { kVertical, kHorizontal, kPass };

// Different encoding algorithms used for image compression.
enum class EncodingAlgorithm { kClassical, kGroup4, kLZW };

// TODO(mouradfelipe): change location of LZW capacity definition.
// For now, it is defined as a value taken from the gifsicle GIF optimizer.
static constexpr uint32_t kLZWCapacity = 3963;

// Gives the maximum possible palette size for a given image size.
static inline uint32_t MaxPaletteSize(uint32_t num_pixels) {
  // We only allow palettes of at least 2 elements.
  if (num_pixels < 2) return 2;
  // The palette cannot be bigger than the number of pixels.
  return std::min(kMaxPaletteSize, num_pixels);
}
// Make sure that the palette + predictors can be encoded in ANS: this is
// definitely not a perfect static_assert but it is better than nothing.
static_assert(2 * kMaxPaletteSize < WP2::kANSMaxRange,
              "Max palette size is too big.");

// Prefix code size when writing the ARGB ranges.
constexpr uint32_t kARGBRangePrefixSize = 1;

}  // namespace WP2L

//------------------------------------------------------------------------------

#endif /* WP2_COMMON_CONSTANTS_H_ */
