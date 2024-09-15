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
// Lossless decoder: internal header.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_DEC_LOSSLESS_LOSSLESSI_DEC_H_
#define WP2_DEC_LOSSLESS_LOSSLESSI_DEC_H_

#include <cstdint>

#include "src/common/constants.h"
#include "src/common/lossless/color_cache.h"
#include "src/common/symbols.h"
#include "src/dec/symbols_dec.h"
#include "src/dec/tile_dec.h"
#include "src/utils/ans.h"
#include "src/utils/hash_map.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/debug.h"
#include "src/wp2/decode.h"
#include "src/wp2/format_constants.h"

namespace WP2L {

typedef enum { READ_DATA = 0, READ_HDR = 1, READ_DIM = 2 } DecodeState;

class Transform {
 public:
  TransformType type_;    // transform type.
  uint32_t bits_ = 0;     // subsampling bits defining transform window.
  uint32_t width_pic_;    // initial picture width
  uint32_t height_pic_;   // initial picture height
  uint32_t width_ = 0;    // transform width (if any)
  uint32_t height_ = 0;   // transform height (if any)
  WP2::Vector_s16 data_;  // transform data.
};

class Metadata {
 public:
  void Init(uint32_t num_pixels, bool has_alpha, WP2SampleFormat format);

  LosslessSymbolsInfo symbols_info_;
  CacheConfig cache_config_;
  ColorCachePtr color_cache_;
  WP2::SegmentCache segment_cache_;

  uint32_t histogram_mask_;
  int histogram_subsample_bits_;
  int histogram_xsize_;
  WP2::Vector_s16 histogram_image_;
  WP2::SymbolReader sr_;
};

class Decoder {
 public:
  Decoder();
  void Init(const WP2::DecoderConfig& config, const WP2::GlobalParams& gparams,
            bool image_is_premultiplied, WP2::ANSDec* dec, WP2::Tile* tile);
  // Decodes the image header. Returns false in case of error.
  WP2Status DecodeHeader();
  // Decodes an image. It's required to decode the lossless header before
  // calling this function.
  WP2Status DecodeImage();
  // Decodes the next 'n_lines' lines of the image. DecodeHeader() must have
  // been called before first calling this function.
  WP2Status DecodeLines(uint32_t n_lines);

  // Allocates internal buffers.
  WP2Status AllocateInternalBuffers(uint32_t final_width);

  // Reads palette data from the stream.
  // Visible for testing.
  WP2Status ReadPalette(Transform* transform);

 private:
  // Reads the stats useful for the symbols for later ANS decoding.
  WP2Status ReadANSStats(uint32_t width, uint32_t height,
                         const LosslessSymbolsInfo& symbols_info);
  WP2Status ApplyInverseTransforms(uint32_t num_rows, const int16_t* rows);
  // Processes (transforms, scales & color-converts) the rows decoded after the
  // last call until row number 'row'.
  WP2Status ProcessRows(uint32_t row);
  // Decodes an image and post-processes the rows.
  WP2Status DecodeAndProcess(uint32_t width, uint32_t last_row, bool do_process,
                             WP2::Vector_s16* data_vec,
                             WP2::Planef* bits_per_pixel = nullptr,
                             WP2::DecoderInfo* decoder_info = nullptr,
                             int16_t** src_out = nullptr);
  // Decodes any image that is not the main image (segment image, transform
  // images (predictors, palette ...)).
  WP2Status DecodeHelperImage(uint32_t width, uint32_t height,
                              const LosslessSymbolsInfo& symbols_info,
                              WP2::Vector_s16* data_vec);
  // Decodes a sub-image encoded with the Group 4 algorithm.
  WP2Status DecodeGroup4(uint32_t width, uint32_t last_row,
                         WP2::Planef* bits_per_pixel, int16_t** src_out);
  // Decodes a sub-image encoded with the LZW algorithm.
  WP2Status DecodeLZW(uint32_t width, uint32_t last_row,
                      WP2::Planef* bits_per_pixel, int16_t** src_out);

  // Decodes an image, once the pre-processing has been done.
  WP2Status DecodeImageData(uint32_t width, uint32_t height, uint32_t last_row,
                            WP2::Planef* bits_per_pixel);
  // Reads a transform of a certain size from the stream.
  WP2Status ReadTransform(uint32_t width, uint32_t height, TransformType type,
                          Transform* transform);

  // Visual debug. 'symbol_type' can be a SymbolType for classical algorithm,
  // a Group4Mode for Group4 algorithm, or 0 for LZW algorithm.
  WP2Status RegisterSymbolForVDebug(int symbol_type, uint32_t pos,
                                    uint32_t distance, uint32_t length,
                                    float cost, WP2::DecoderInfo* decoder_info);
  // Visual debug. 'rows' should contain the raw decoded pixels before inverse
  // transforms are applied. 'decoder_info' can be nullptr (in which case
  // nothing is done).
  // 'index' is the transform index up to which the transforms are applied.
  WP2Status RegisterTransformedRowForVDebug(int index, uint32_t num_rows,
                                            uint32_t num_bits,
                                            const int16_t* rows,
                                            WP2::DecoderInfo* decoder_info);
  // Display the predicted pixels for the predictor and cross-color transforms
  WP2Status RegisterPredictedRowForVDebug(uint32_t index, uint32_t num_rows,
                                          uint32_t num_bits,
                                          const int16_t* rows,
                                          WP2::DecoderInfo* decoder_info);
  // Display the predictor and cross-color transforms.
  WP2Status RegisterTransformForVDebug(const Transform& transform,
                                       WP2::DecoderInfo* decoder_info);

  // Visual debug for header info.
  WP2Status HeaderVDebug();

  // Maximum number of rows we can hold in the cache.
  static constexpr uint32_t kNumARGBCacheRows = WP2::kMaxBlockSizePix;

  DecodeState state_ = READ_DIM;

  WP2::Tile* tile_ = nullptr;

  WP2::Vector_s16 pixels_;         // Internal data
  int16_t* argb_cache_ = nullptr;  // Scratch buffer for temporary BGRA

  WP2::ANSDec* dec_ = nullptr;

  uint32_t last_row_ = 0;      // last input row decoded so far.
  uint32_t last_pixel_ = 0;    // last pixel decoded so far. However,
                               // it may not be transformed, scaled and
                               // color-converted yet.
  uint32_t last_out_row_ = 0;  // last row output so far.

  Metadata hdr_;

  Transform transforms_[kPossibleTransformCombinationSize];
  EncodingAlgorithm encoding_algorithm_ = EncodingAlgorithm::kClassical;
  uint16_t group4_first_color_ = 0;  // starting color for group4 compression
  MoveToFrontCache mtf_;
  uint32_t num_colors_ = 0;
  WP2::Planef bits_per_pixel_;
#ifndef WP2_REDUCE_BINARY_SIZE
  WP2::ArgbBuffer predicted_;
#endif
  WP2::DecoderConfig config_;
  const WP2::GlobalParams* gparams_ = nullptr;
  // If true, all samples will be emitted as premultiplied (and then
  // unmultiplied if the user asks for ARGB).
  // If false, all samples will be emitted as unmultiplied (and then
  // premultiplied if the user asks for Argb).
  bool image_is_premultiplied_ = false;
  // Whether the samples in this tile were in Argb (true) or ARGB (false) format
  // before being encoded losslessly.
  bool tile_is_premultiplied_ = false;
  WP2::Vector<ColorSegment> lzw_segments_;  // LZW segments
  ColorSegment lzw_last_seg_;               // last decoded segment
};

}  // namespace WP2L

#endif /* WP2_DEC_LOSSLESS_LOSSLESSI_DEC_H_ */
