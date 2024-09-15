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
// Lossless encoder: internal header.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_ENC_LOSSLESS_LOSSLESSI_ENC_H_
#define WP2_ENC_LOSSLESS_LOSSLESSI_ENC_H_

#include <cstdint>

#include "src/common/constants.h"
#include "src/common/progress_watcher.h"
#include "src/common/symbols.h"
#include "src/enc/lossless/backward_references_enc.h"
#include "src/enc/lossless/palette.h"
#include "src/utils/ans_enc.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {
class SymbolManager;
class SymbolRecorder;
class SymbolWriter;
}  // namespace WP2

namespace WP2L {

// Struct containing signed 4-channel data. The stride is 4 * width.
// It is meant to store the original color image, as well as its transformed
// versions. The number of channel bits represents the depth of the original
// data, before the transforms.
struct Buffer_s16 {
  int16_t* GetRow(uint32_t y) { return &data[4 * y * width]; }
  const int16_t* GetRow(uint32_t y) const { return &data[4 * y * width]; }
  int16_t* GetPosition(uint32_t x, uint32_t y) {
    return &data[4 * (y * width + x)];
  }
  const int16_t* GetPosition(uint32_t x, uint32_t y) const {
    return &data[4 * (y * width + x)];
  }
  uint32_t width;
  uint32_t height;
  int16_t* data = nullptr;
  bool has_alpha;         // true if alpha matters
  uint32_t channel_bits;  // number of bits per non-alpha channel
};
struct EncodeInfo;
struct CrunchConfig;

class Encoder {
 public:
  Encoder(const WP2::EncoderConfig& config, const WP2::ArgbBuffer& picture,
          bool has_alpha, bool is_premultiplied);
  WP2Status Allocate();
  WP2Status AllocateTransformBuffer(const WP2::Vector<CrunchConfig>& configs);
  // Applies and writes the transforms as well as some more info about the
  // transforms (number, range ...).
  // If 'config_prev' is not nullptr, its transforms are supposed to be the
  // first transforms of 'config' and are skipped: they are not applied and they
  // are stored in  'enc' by copying them from cached data (that was cached from
  // a previous call with 'cache_data' set to true).
  WP2Status ApplyAndWriteTransforms(const CrunchConfig& config,
                                    const CrunchConfig* config_prev,
                                    bool cache_data,
                                    WP2::ProgressRange progress,
                                    LosslessSymbolsInfo* symbols_info,
                                    WP2::ANSEnc* enc,
                                    WP2::ANSDictionaries* dicts);
  // Encodes the image for a given configuration.
  // 'enc_best' and 'dicts_init' contain the encoded info so far.
  // 'cost_best' is the cost in bits the final encoding must beat. If a better
  // cost is found, it is updated and 'enc_best' contains the best encoding.
  // Otherwise, 'enc_best' is set to empty.
  // 'dicts_init' is used as temporary data and modified.
  WP2Status EncodeImageInternal(const CrunchConfig& config,
                                const WP2::ProgressRange& progress,
                                const LosslessSymbolsInfo& symbols_info_init,
                                float* cost_best, WP2::ANSEnc* enc_init,
                                EncodeInfo* encode_info,
                                WP2::ANSDictionaries* dicts_init);

  const WP2::EncoderConfig& config_;  // user configuration and parameters
  const WP2::ArgbBuffer& pic_;        // input picture (tile view)
  bool has_alpha_;  // false if the whole image is opaque (not only this tile)
  bool image_is_premultiplied_;  // true if the whole image is considered
                                 // premultiplied, even if this tile is not

  LosslessSymbolsInfo symbols_info_;

  Palette palette_;

  // Some 'scratch' (potentially large) objects.
  BackwardRefsPool ref_pool_;  // Backward Refs array for temporaries.
  HashChain hash_chain_;       // HashChain data for constructing
                               // backward references.
 private:
  // Applies and writes the transforms one by one and nothing else.
  // If 'config_prev' is not nullptr, its transforms are supposed to be the
  // first transforms of 'config' and are skipped.
  WP2Status DoApplyAndWriteTransforms(const CrunchConfig& config,
                                      const CrunchConfig* config_prev,
                                      WP2::ProgressRange progress,
                                      LosslessSymbolsInfo* symbols_info,
                                      WP2::ANSEnc* enc,
                                      WP2::ANSDictionaries* dicts,
                                      EncodingAlgorithm* encoding_algorithm);
  // Implementations of the different transforms.
  void ApplyYCoCgR();
  void ApplySubtractGreen();
  WP2Status ApplyPredictFilter(const int16_t min_values[4],
                               const int16_t max_values[4],
                               bool can_use_sub_modes,
                               const WP2::ProgressRange& progress,
                               uint32_t pred_bits, WP2::ANSEnc* enc,
                               WP2::ANSDictionaries* dicts);
  WP2Status ApplyCrossColorFilter(const WP2::ProgressRange& progress,
                                  uint32_t ccolor_transform_bits,
                                  WP2::ANSEnc* enc,
                                  WP2::ANSDictionaries* dicts);
  WP2Status MapImageFromPalette(const CrunchConfig& config);
  // Copy the original pic_ when need.
  WP2Status MakeInputImageCopy(const CrunchConfig& config);

  int16_t* argb_scratch_;          // Scratch memory for argb rows
                                   // (used for prediction).
  int16_t* transform_data_;        // Scratch memory for transform data.
  WP2::Vector_s16 transform_mem_;  // Currently allocated memory.

  // When testing different sets of transforms, the results (argb_buffer_,
  // stream, dictionaries) can be re-used from this cached variables.
  WP2::ANSEnc cached_transforms_enc_;
  WP2::ANSDictionaries cached_transforms_dict_;
  // Image data that is initialized and updated between crunch configurations if
  // possible.
  Buffer_s16 argb_buffer_;
};

//------------------------------------------------------------------------------
// internal functions. Not public.

// Encodes any image that is not the main image (segment image, transform images
// (predictors, palette ...)).
WP2Status EncodeHelperImage(WP2::ANSEncBase* enc, WP2::ANSDictionaries* dicts,
                            const int16_t* argb, HashChain* hash_chain,
                            BackwardRefsPool* ref_pool, uint32_t width,
                            uint32_t height,
                            const LosslessSymbolsInfo& symbols_info, int effort,
                            const WP2::ProgressRange& progress);

// Optional extra info that can be exported by EncodeImage.
// Callers should allocate themselves the vectors that they wish filled in.
// Empty vectors will not be filled in.
struct EncodeInfo {
  WP2_NO_DISCARD bool CopyFrom(const EncodeInfo& other);

  // The number of tokens in the ANSEnc at which point each line has finished
  // being written is returned in it. If non-empty, its size should match the
  // height of the image.
  WP2::Vector_u32 line_tokens;
  // Cost in bits of storing each pixel of the image (not including bits of
  // headers like palettes, ans stats...), in row major order. If non-empty,
  // its size should match the number of pixels of the image.
  WP2::Vector_f bits_per_pixel;
};

// Encodes the picture.
// 'has_alpha' might be true for another tile even if this one does not.
// 'image_is_premultiplied' can be true for the whole image even if this tile
// is not, and the format of the input picture is not necessarily matched.
WP2Status EncodeImage(const WP2::EncoderConfig& config,
                      const WP2::ArgbBuffer& picture, bool has_alpha,
                      bool image_is_premultiplied,
                      const WP2::ProgressRange& progress, WP2::ANSEnc* enc,
                      EncodeInfo* encode_info = nullptr);

// Stores the pixels of the image, without caring about the header.
// 'segments' is an ARGB image where the cluster is stored in G and other
// components are pre-filled with 0.
WP2Status StorePixels(uint32_t width, uint32_t height, uint32_t histo_bits,
                      const BackwardRefs& refs,
                      const WP2::ProgressRange& progress,
                      const int16_t* segments, WP2::ANSEncBase* enc,
                      WP2::SymbolManager* sw,
                      EncodeInfo* encode_info = nullptr);
// Same as above except for an image where there are no entropy segments.
WP2Status StorePixels(uint32_t width, uint32_t height, const BackwardRefs& refs,
                      WP2::ANSEncBase* enc, WP2::SymbolManager* sw);
// Stores the different symbol headers and sets up 'sw' with the right symbol
// info.
WP2Status WriteHeaders(const WP2::SymbolRecorder& recorder,
                       const LosslessSymbolsInfo& symbols_info,
                       uint32_t num_pixels, int effort, WP2::ANSEncBase* enc,
                       WP2::ANSDictionaries* dicts, WP2::SymbolWriter* sw,
                       float* cost = nullptr);

// Converts an offset to a code taking into account the proximity to the
// current pixel.
uint32_t OffsetToPlaneCode(uint32_t width, uint32_t offset);

//------------------------------------------------------------------------------
// Image transforms in predictor_enc.cc

// Writes the predictor sub modes (sub angles for now).
// 'sub_modes' is a 4-channel image with the sub-mode being stored in index 2.
// If 'has_both_modes' is true, the mode is obtained from 'modes' as
//      modes[4 * x + 2] >> kNumPredictorsBits.
// If false, it is simply modes[4 * x + 2].
WP2Status WriteSubModes(const int16_t* modes, bool needs_shift,
                        const WP2::Vector_s8& sub_modes, int effort,
                        WP2::ANSEncBase* enc, WP2::ANSDictionaries* dicts);

// Computes the signed residuals after applying the predictors.
// 'can_use_sub_modes' indicates whether sub-modes are allowed by the transform
// and should be tried to find the best predictors.
// If sub-modes are used, 'sub_modes' is not empty and contains the sub-modes.
WP2Status CreateResidualImage(uint32_t bits, bool exact, bool can_use_sub_modes,
                              const int16_t min_values[4],
                              const int16_t max_values[4], int effort,
                              const WP2::ProgressRange& progress,
                              Buffer_s16* buffer, int16_t* argb_scratch,
                              int16_t* modes, WP2::Vector_s8* sub_modes);

WP2Status ColorSpaceTransform(uint32_t bits, int effort, Buffer_s16* buffer,
                              int16_t* image);

//------------------------------------------------------------------------------

// Set of parameters to be used in each iteration of the cruncher.
static constexpr uint32_t kCrunchLZ77Max = 2;
struct CrunchConfig {
  uint32_t lz77s_types_to_try[kCrunchLZ77Max];
  uint32_t lz77s_types_to_try_size;
  // Image transforms to apply in order. Each transform is only used once.
  // It is a pointer into the reference list of transform combinations.
  const TransformType* transforms = nullptr;
  uint32_t GetNumTransforms() const;
  // whether pixels should be encoded premultiplied with the alpha value
  bool use_premultiplied;
  // Number of bits to subsample the original image dimensions by to get the
  // histogram and transform image dimensions.
  uint32_t histo_bits;
  uint32_t transform_bits;
  // Use a MoveToFrontCache in group4 to signal new colors.
  bool group4_use_move_to_front;
  Palette::Sorting palette_sorting_type;
  // Returns whether one of the transforms is kPredictorWSub.
  bool HasSubModes() const {
    for (uint32_t i = 0; i < kPossibleTransformCombinationSize; ++i) {
      if (transforms[i] == TransformType::kPredictorWSub) {
        return true;
      }
    }
    return false;
  }

  // Return true if caching the current CrunchConfig results will help 'config'.
  bool ShouldBeCachedFor(const CrunchConfig& config) const;
};

WP2Status EncoderAnalyze(const WP2::ProgressRange& progress, Encoder* encoder,
                         WP2::Vector<CrunchConfig>* configs);

//------------------------------------------------------------------------------
// Group 4.

WP2Status Group4Encode(const Buffer_s16& buffer, uint32_t num_colors,
                       bool use_move_to_front, int effort,
                       const WP2::ProgressRange& progress, WP2::ANSEncBase* enc,
                       EncodeInfo* encode_info);

//------------------------------------------------------------------------------
// LZW.

// Encodes an image 'input_buffer' to 'enc'.
// The 'palette' is the set of all unique colors from the 'input_buffer'.
WP2Status LZWEncode(const Buffer_s16& input_buffer, const Palette& palette,
                    int effort, WP2::ANSEncBase* enc, EncodeInfo* encode_info);

}  // namespace WP2L

//------------------------------------------------------------------------------

#endif /* WP2_ENC_LOSSLESS_LOSSLESSI_ENC_H_ */
