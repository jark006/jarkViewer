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
//  Internal header for constants related to WP2 file format.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_WP2_FORMAT_CONSTANTS_H_
#define WP2_WP2_FORMAT_CONSTANTS_H_

#include <cstdint>
#include <type_traits>

namespace WP2 {

// maximum width/height allowed (inclusive), in pixels
// width/height limit
static constexpr uint32_t kMaxBufferDimension = (1u << 24);
// width * height limit
static constexpr uint32_t kMaxBufferArea = (1u << 28);

static constexpr uint32_t kSignature = 0x6ffff4;
static constexpr uint32_t kMaxPreviewSize = 333;
static constexpr uint32_t kHeaderMinSize = 10;
static constexpr uint32_t kHeaderMaxSize = 16;
static constexpr uint32_t kTagMask = 0x60ffe9;

// Maximum (inclusive) size of a buffer chunk.
static constexpr uint32_t kMaxChunkSize = 1u << 29;  // 512 MB

static constexpr const char kFileExtension[] = "wp2";

// Number of bits used to store image dimension or frame offset.
static constexpr uint32_t kImageDimNumBits = 14;
// Maximum width/height allowed (inclusive), in pixels. We do not need to remove
// 1 to fit in kImageDimNumBits bits as we always remove one when storing
// dimensions (as they cannot be 0).
static constexpr uint32_t kImageDimMax = (1 << kImageDimNumBits);

// Qualities up to 95 included are considered lossy; above is lossless.
static constexpr uint32_t kMaxLossyQuality = 95;
static constexpr uint32_t kMaxQuality = 100;
static constexpr uint32_t kQualityHintNumBits = 4;
static constexpr uint32_t kAlphaQualityRange = kMaxLossyQuality + 2;
static constexpr int kMinEffort = 0;
static constexpr int kMaxEffort = 9;

// For animations:
// ANMF chunk
static constexpr uint32_t kANMFMinHeaderSize = 2;   // 8 bits + duration varint
static constexpr uint32_t kANMFMaxHeaderSize = 11;  // 8 bits + 5 clamped varint
static constexpr uint8_t kANMFTagNumBits = 6;
static constexpr uint32_t kANMFTagDispose = 0x33;
static constexpr uint32_t kANMFTagFrame = 0x15;
static constexpr uint32_t kMaxFrameDurationMs = 0x7Fu + (0x100u << 7);  // ~32s
// Maximum number of frames in an animation, including preframes.
static constexpr uint32_t kMaxNumFrames = 65282;  // inclusive
// Number of allowed consecutive preframes (meaning with a duration of 0 ms).
static constexpr uint32_t kMaxNumPreframes = 4;
// Number of allowed consecutive non-disposed regular frames (ignore preframes).
// To take preframes into account:
//   (1 + kMaxNumDependentFrames) * (1 + kMaxNumPreframes) - 1
static constexpr uint32_t kMaxNumDependentFrames = 16;

// Maximum inclusive angle step magnitude. MAX_ANGLE_DELTA in AV1.
// An angle predictor at angle 'angle' will generate sub-predictors at angles
// 'angle' +/- kDirectionalMaxAngleDelta * 'angle_unit' where 'angle_unit'
// subdivides 22.5 degrees in (2 * kDirectionalMaxAngleDeltaYA + 1) angles.
constexpr uint32_t kDirectionalMaxAngleDeltaYA = 3u;
constexpr uint32_t kDirectionalMaxAngleDeltaUV = 1u;
constexpr uint32_t kYBasePredNum = 9;
constexpr uint32_t kABasePredNum = 8;
constexpr uint32_t kUVBasePredNum = 4;
constexpr uint32_t kAnglePredNum = 10;

enum BasePredictor {
  BPRED_DC,
  BPRED_DC_L,         // dc-left
  BPRED_DC_T,         // dc-top
  BPRED_MEDIAN_DC,    // median-dc-all
  BPRED_MEDIAN_DC_L,  // median-dc-left
  BPRED_MEDIAN_DC_T,  // median-dc-top
  BPRED_DOUBLE_DC,    // double-dc-all
  BPRED_DOUBLE_DC_L,  // double-dc-left
  BPRED_DOUBLE_DC_T,  // double-dc-top
  BPRED_SMOOTH,
  BPRED_SMOOTH_H,
  BPRED_SMOOTH_V,
  BPRED_TM,
  BPRED_LAST
};

// Algorithm used for image partitioning.
enum PartitionMethod {
  // Combine several heuristics in successive block size passes:
  MULTIPASS_PARTITIONING,  // (fast)
  // For each pos starting from top left, take the best block size depending on:
  BLOCK_ENCODE_PARTITIONING,  // block encoding score (slow)
  SPLIT_PARTITIONING,         // whole or split block encoding score (slow)
  TILE_ENCODE_PARTITIONING,   // tile encoding score (super slow)
  // Decide recursively how best to split blocks.
  SPLIT_RECURSE_PARTITIONING,
  // Per 32x32 area, take the best one of a few block layouts:
  AREA_ENCODE_PARTITIONING,      // (slow)
  SUB_AREA_ENCODE_PARTITIONING,  // same but for each block in area (very slow)
  // Per tile, take the best one of all possible block layouts:
  EXHAUSTIVE_PARTITIONING,  // (extremely slow)
  // Fixed block size (except on edges). Also depends on the partition set.
  ALL_4X4_PARTITIONING,
  ALL_8X8_PARTITIONING,
  ALL_16X16_PARTITIONING,
  ALL_32X32_PARTITIONING,
  // Choose one above based on compression effort and image size.
  AUTO_PARTITIONING,
  NUM_PARTITION_METHODS
};

// The set of allowed block sizes for image partitioning.
enum PartitionSet {  // The smallest block size is 4x4.
  SMALL_SQUARES,     // Up to 8x8
  SMALL_RECTS,       // Up to 16x16
  ALL_RECTS,         // Up to 32x32, ratio at most 4:1
  THICK_RECTS,       // Up to 32x32, ratio at most 2:1
  MEDIUM_SQUARES,    // Up to 16x16
  ALL_SQUARES,       // Up to 32x32
  SOME_RECTS,        // Up to 32x32, subset of frequently used rects
  NUM_PARTITION_SETS
};

static constexpr uint32_t kMaxNumSegments = 8;

typedef enum {
  WP2_TF_ITU_R_BT2020_10BIT = 0,
  WP2_TF_ITU_R_BT709,
  WP2_TF_UNSPECIFIED,
  WP2_TF_GAMMA_22,
  WP2_TF_GAMMA_28,
  WP2_TF_SMPTE_170M,
  WP2_TF_SMPTE_240M,
  WP2_TF_LINEAR,
  WP2_TF_LOG,
  WP2_TF_SQRT,
  WP2_TF_IEC_61966_2_4,
  WP2_TF_ITU_R_BT1361_EXTENDED,
  WP2_TF_IEC_61966_2_1,
  WP2_TF_ITU_R_BT2020_12BIT,
  WP2_TF_SMPTE_ST_2084,
  WP2_TF_SMPTE_ST_428_1,
  WP2_TF_ARIB_STD_B67_HLG
} TransferFunction;

// Predefined ICC chunk
static constexpr uint32_t kPredefinedICC1Size = 524;
extern const uint8_t kPredefinedICC1[];

static constexpr uint32_t kPredefinedICC2Size = 3144;
static constexpr uint32_t kPredefinedICC2Offset = 67;  // variable byte
extern const uint8_t kPredefinedICC2[];

// Channels
typedef enum { kYChannel = 0, kUChannel, kVChannel, kAChannel } Channel;

static constexpr uint32_t kRgbBits = 8u;
static constexpr uint32_t kRgbMax = (1 << kRgbBits) - 1;  // Inclusive.
static constexpr uint32_t kAlphaBits = 8u;
static constexpr uint32_t kAlphaMax = (1 << kAlphaBits) - 1;  // Inclusive.

// Max precision for YUV values, excluding the sign.
static constexpr uint32_t kYuvMaxPrec = 9u;

// Prediction block dimensions:
static constexpr uint32_t kMinBlockSizePix = 4u;   // min block size in pixels
static constexpr uint32_t kMaxBlockSizePix = 32u;  // max block size in pixels
// maximum block size  (for stack reservation)
static constexpr uint32_t kMaxBlockSizePix2 =
    (kMaxBlockSizePix * kMaxBlockSizePix);
// blocks dimensions (w or h) in kMinBlockSizePix units are [1..kMaxBlockSize]
// That's 64 possible block types.
static constexpr uint32_t kMaxBlockSize = (kMaxBlockSizePix / kMinBlockSizePix);
static constexpr uint32_t kMaxBlockSize2 = (kMaxBlockSize * kMaxBlockSize);

// Tile sizes.
static constexpr uint32_t kTileShapeBits = 2;
static constexpr uint32_t kMaxTilePixels = 512 * 512;
static constexpr uint32_t kMaxTileSize =
    kMaxTilePixels / (2 * kMaxBlockSizePix);
// Extreme aspect ratios are undesirable as they create more borders with less
// context.
static constexpr uint32_t kMaxAspectRatio = 64;
static_assert(kMaxTileSize * kMaxTileSize / kMaxTilePixels <= kMaxAspectRatio,
              "Extreme aspect ratios are no good");
static constexpr uint32_t kMaxTileChunkSize =
    (kMaxTilePixels * (kAlphaBits + (kYuvMaxPrec + 1) * 3) + 7) / 8;
static_assert(kMaxTileChunkSize <= kMaxChunkSize,
              "Tile bypass may not fit in a chunk");

typedef enum {
  TILE_SHAPE_SQUARE_128,
  TILE_SHAPE_SQUARE_256,
  TILE_SHAPE_SQUARE_512,
  // As wide as possible (with a max of kMaxTileSize) and height set so that
  // it has at most kMaxTilePixels.
  TILE_SHAPE_WIDE,
  TILE_SHAPE_AUTO,
  NUM_TILE_SHAPES,
} TileShape;

// max coded level. number of prefix bits for the dictionaries.
static constexpr uint32_t kMaxCoeffBits = 10;

// convert pixels units to min-block-size units (rounding up)
static constexpr uint32_t SizeBlocks(uint32_t size_pix) {
  return ((size_pix + kMinBlockSizePix - 1u) / kMinBlockSizePix);
}

// Prediction block size
static constexpr uint32_t kPredWidth = 4;
static constexpr uint32_t kPredHeight = 4;
static constexpr uint32_t kPredSize = (kPredWidth * kPredHeight);
static constexpr uint32_t kLinearPredBits = 10;  // For calculation.
// Type of context.   Missing context pixels are filled in with neighboring
// values except for kContextSmallNoFillIn.
// For kContextExtendRight, the context is extended horizontally to
// the right instead of along the right boundary.
// For kContextExtendLeft, left context is twice as high.
enum ContextType {
  kContextSmall,          // left + top + right
  kContextSmallNoFillIn,  // same as above, but using CodedBlock::kMissing
  kContextExtendRight,    // left + top + extended right
  kContextExtendLeft,     // extended left + left
  kNumContextType
};
// Size of the context going left of a block, top, and top right extending the
// top context. Extending by kMaxBlockSizePix is good enough for 45 degree
// predictors but we need more for lower angles, hence 2 * kMaxBlockSizePix.
static constexpr uint32_t kMaxContextTRExtent = 2 * kMaxBlockSizePix;
// Size of the usual context for prediction (it goes all around a block).
static constexpr uint32_t ContextSize(ContextType context_type, uint32_t bw,
                                      uint32_t bh) {
  return (context_type == kContextSmall ||
          context_type == kContextSmallNoFillIn)
             ? (bw + 2 * (bh + 1))
         : (context_type == kContextExtendRight)
             ? (bh + 1 + bw + kMaxContextTRExtent)
             : (2 * bh);
}
static constexpr uint32_t kContextSize =
    ContextSize(kContextSmall, kPredWidth, kPredHeight);
// Maximum context size (for stack reservation).
static constexpr uint32_t kMaxContextSize =
    ContextSize(kContextExtendRight, kMaxBlockSizePix, kMaxBlockSizePix);
// Max context size when using sub transforms (split_tf).
static constexpr uint32_t kMaxSubContextSize = ContextSize(
    kContextExtendRight, kMaxBlockSizePix / 2, kMaxBlockSizePix / 2);
static_assert(kMaxContextSize >= ContextSize(kContextSmall, kMaxBlockSizePix,
                                             kMaxBlockSizePix),
              "Wrong kMaxContextSize");

// Maximum (inclusive) number of bits to use to represent a frequency in ANS
// quantization.
static constexpr uint32_t kMaxFreqBits = 14;

// Index to Adaptation-Speed mapping.
// TODO(skal): fine-tune, learn.
// Natural photo prefer slower adaptation (speed ~= 2..4)
// Web pictures prefer 3..6
static constexpr uint32_t kAdaptationSpeeds[] = {
    500, 800, 1000, 1300, 1800, 3000, 5000, 65535 /* <~unique symbol */
};
static constexpr uint32_t kNumAdaptationSpeeds =
    sizeof(kAdaptationSpeeds) / sizeof(kAdaptationSpeeds[0]);
static constexpr uint32_t kMinAOMAdaptSpeed = 1024u;
static constexpr uint32_t kMaxAOMAdaptSpeed = 8192u;
static constexpr uint32_t kAOMAdaptSpeedShift = 5;

// maximum quality factor (inclusive)
static constexpr uint16_t kQFactorMax = 237;
// quality-factor scale (used for fixed-point precision)
static inline constexpr uint16_t kQualityToQFactor(float q) {
  return (uint16_t)(q * kQFactorMax / kMaxLossyQuality);
}
static_assert(kQualityToQFactor(0) == 0, "bad scaling for q=0");
static_assert(kQualityToQFactor(kMaxLossyQuality) == kQFactorMax,
              "bad scaling for q=95");

}  // namespace WP2

//------------------------------------------------------------------------------
// Lossless

namespace WP2L {

// Maximum number of colors in the palette.
// TODO(vrabaud) replace with something related to the image size (ratio?).
// We cannot use too big a value because of ANS limitations and this value gives
// good enough results.
static constexpr uint32_t kMaxPaletteSize = (1u << 12);
static constexpr uint32_t kMaxColorCacheBits = 10;
static constexpr uint32_t kMaxSegmentCacheBits = 12;
static constexpr uint32_t kSegmentSizes[] = {2};
// Small palettes are stored verbatim.
static constexpr uint32_t kSmallPaletteLimit = 3;

// Constant defining the special codes for LZ77 distances, for pixels in a
// neighboring window.
// TODO(vrabaud) get rid of it by defining the most common distances/length as
// part of a dictionary.
static constexpr uint32_t kCodeToPlaneCodes = 120u;

// This constant is just to make the code understandable (like kMaxAlpha and
// 255). Having a bigger value actually does not help.
static constexpr uint32_t kLZ77LengthMin = 1u;
static_assert(kLZ77LengthMin == 1u, "kLZ77LengthMin must be 1.");
// Escape code for the LZ77 length.
// Any value can be used, it does not have to be a power of two.
static constexpr uint32_t kLZ77LengthEscape = (1u << 14);
static constexpr uint32_t kLZ77PrefixCodeSize = 1;

// Min and max number of sampling bits for histograms.
static constexpr uint32_t kHistogramBitsMin = 2;
static constexpr uint32_t kHistogramBitsMax = 9;
// Maximum number of histogram images (sub-blocks). This impacts the compression
// but most images require at the very most 200 histograms. It also impacts the
// amount of RAM used for decoding.
static constexpr uint32_t kMaxHistogramImageSize = 1500;
// Maximum absolute values of the residuals. Those values should be chosen so
// that values in this range can fit in the ANS range (it's statically checked).
static constexpr int16_t kMaxAbsResidual8 = 3 * (1 << 8);
static constexpr int16_t kMaxAbsResidual10 = 3 * (1 << 10);

// Minimum and maximum value of transform bits (to downscale the image).
static constexpr uint32_t kTransformBitsMin = 2;
static constexpr uint32_t kTransformBitsMax = 6;

// TODO(vrabaud) Play with this parameter. This value is the official spec one.
// Window size to consider the vertical mode in Group4 compression.
static constexpr uint32_t kGroup4Window = 3;

// LZW constants.
static constexpr uint32_t kLZWCapacityMax = 10000u;

// Precision of the cross color transform.
static constexpr uint32_t kColorTransformBits = 5;
// TODO(vrabaud) Play with those values. kRedToBlueMax could be 128, it is just
// that the code is computing as far as this value.
static constexpr uint32_t kRedToBlueMax = 63;
static constexpr uint32_t kGreenToBlueMax = 127;
static constexpr uint32_t kGreenToRedMax = 127;

enum class TransformType {
  kPredictor,
  kPredictorWSub,  // predictors with sub-modes (unused)
  kCrossColor,
  kYCoCgR,  // reversible YCoCg (unused)
  kSubtractGreen,
  kColorIndexing,
  kGroup4,
  kLZW,
  kNum
};
// Possible image transform combinations that can be applied.
// Must be filled with TransformType::kNum after the last transform.
static constexpr uint32_t kPossibleTransformCombinationSize = 3;
static constexpr TransformType
    kPossibleTransformCombinations[][kPossibleTransformCombinationSize] = {
        // No transform.
        {TransformType::kNum, TransformType::kNum, TransformType::kNum},
        // Only one.
        {TransformType::kGroup4, TransformType::kNum, TransformType::kNum},
        {TransformType::kLZW, TransformType::kNum, TransformType::kNum},
        {TransformType::kSubtractGreen, TransformType::kNum,
         TransformType::kNum},
        {TransformType::kColorIndexing, TransformType::kNum,
         TransformType::kNum},
        {TransformType::kPredictor, TransformType::kNum, TransformType::kNum},
        {TransformType::kCrossColor, TransformType::kNum, TransformType::kNum},
        // Allowed pairs.
        {TransformType::kSubtractGreen, TransformType::kPredictor,
         TransformType::kNum},
        {TransformType::kSubtractGreen, TransformType::kCrossColor,
         TransformType::kNum},
        {TransformType::kColorIndexing, TransformType::kPredictor,
         TransformType::kNum},
        {TransformType::kPredictor, TransformType::kCrossColor,
         TransformType::kNum},
        // Triplet.
        {TransformType::kSubtractGreen, TransformType::kPredictor,
         TransformType::kCrossColor}};
static constexpr uint32_t kPossibleTransformCombinationsNum =
    std::extent<decltype(kPossibleTransformCombinations), 0>::value;

}  // namespace WP2L

#endif /* WP2_WP2_FORMAT_CONSTANTS_H_ */
