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
//  Data and visual debugging tools.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_WP2_DEBUG_H_
#define WP2_WP2_DEBUG_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "./base.h"
#include "./format_constants.h"

namespace WP2 {

template <typename T>
class Plane;
class CSPTransform;

//------------------------------------------------------------------------------

// Amount of bits and number of occurrences of a label.
// Also stores the histogram (value, number of occurrences) and the type
// (Bit, Abit, RValue ...)
struct LabelStats {
  double bits;
  uint32_t num_occurrences;
  std::map<uint32_t, uint32_t> histo;
  enum class Type { Bit, ABit, RValue, UValue, ASymbol, Symbol } type;
};

// Structure for storing side info and stats.
struct BlockInfo {
  Rectangle rect;      // position and dimension in pixel units
  uint8_t segment_id;  // segment id

  bool has_lossy_alpha;  // true if lossy or mix of lossy/lossless alpha is used
  bool is420;            // reduced chroma transform
  bool split_tf[4];      // if true, tfs are smaller than block (per channel)
  uint8_t tf_x, tf_y;    // luma transform type
  bool y_context_is_constant;

  uint8_t y_pred;        // luma prediction
  uint8_t a_pred;        // alpha prediction
  uint8_t uv_pred;       // UV prediction mode
  float pred_scores[4];  // Per channel (only available from EncoderInfo)

  // residuals, in natural order per channel per split transform
  int16_t coeffs[4][4][kMaxBlockSizePix2];
  // original spatial residuals
  int16_t original_res[4][4][kMaxBlockSizePix2];
  // how are encoded the 'coeffs' per channel per split transform
  int8_t encoding_method[4][4];

  double bits;                                   // approx. number of bits used
  std::vector<std::string> residual_info[4][4];  // Residual info per channel
                                                 // per split transform.

#if defined(WP2_BITTRACE)
  // Approx. number of bits used for given categories (mode, coeffs ...).
  std::map<const std::string, LabelStats> bit_traces;
#endif
};

//------------------------------------------------------------------------------

// Structure given to the encoder as an instance in EncoderConfig to access
// debugging information about the blocks. Needs WP2_BITTRACE.
struct EncoderInfo {
  // Blocks specified in the 'force_partition' will be placed first.
  // Remaining areas will be filled with the regular partitioning algorithm.
  // Coordinates are in pixels and must be multiples of kMinBlockSizePix.
  std::vector<Rectangle> force_partition;  // (input)
  // Predetermined encoding parameters.
  struct ForcedParam {
    enum class Type {
      kTransform,
      kSegment,
      kPredictor,
      kSplitTf,
      kNumTypes,
    };
    Type type;
    // xy coordinates of the block in pixels.
    uint32_t x, y;
    // transform id, segment id, predictor id or split_tf (bool)
    uint32_t value;
    Channel channel;  // only relevant for kPredictor
  };
  std::vector<ForcedParam> force_param;

#if defined(WP2_BITTRACE)
  std::vector<BlockInfo> blocks;  // Information about blocks (output).
  const BlockInfo* FindBlock(uint32_t x, uint32_t y) const {
    for (const auto& b : blocks) {
      if (b.rect.Contains(x, y)) return &b;
    }
    return nullptr;
  }
#endif  // WP2_BITTRACE

  bool store_blocks = false;  // Fills 'blocks' if true (input).

  // If 'visual_debug' is not null and, depending on its value, 'debug_output'
  // will contain visual data. A list of possible strings can be found in vwp2.
  const char* visual_debug = nullptr;  // (input)
  ArgbBuffer debug_output;             // (output)

  // Depending on 'visual_debug', the 'selection' coordinates can be used to
  // gather information about a pixel or a zone into 'selection_info'.
  Rectangle selection;         // Unused if width or height < 1. (input)
  std::string selection_info;  // May contain line breaks.       (output)

  bool disable_transforms = false;  // Disable transforms other than DCT.
  bool disable_preds = false;       // Disable predictors other than DC.
  bool disable_split_tf = false;    // Disable split transform.
};

// Structure given to the decoder as an instance in DecoderConfig to access
// debugging information about the bitstream. Additional stats about labels and
// blocks can be retrieved with WP2_BITTRACE.
struct DecoderInfo {
#if defined(WP2_BITTRACE)

  // For each label, store the amount of bits, and the number of occurrences.
  std::map<const std::string, LabelStats> bit_traces;  // (output)
  std::vector<BlockInfo> blocks;                       // (output)

#endif  // WP2_BITTRACE

  bool store_blocks = false;  // Fills 'blocks' if true. (input)

  size_t header_size = 0;  // Byte size of the header. (output)
  uint32_t num_segments = 0;
  bool explicit_segment_ids = false;
  std::vector<std::string> y_predictors;
  std::vector<std::string> uv_predictors;
  std::vector<std::string> a_predictors;

  // If 'visual_debug' is not null and, depending on its value, 'debug_output'
  // will contain visual data. A list of possible strings can be found in vwp2.
  const char* visual_debug = nullptr;  // (input)
  ArgbBuffer debug_output;             // (input if "original" vdebug) (output)

  // Depending on 'visual_debug', the 'selection' coordinates can be used to
  // gather information about a pixel or a zone into 'selection_info'.
  Rectangle selection;         // Unused if width or height < 1. (input)
  std::string selection_info;  // May contain line breaks.       (output)

  // If not null, will be filled with the cost of each pixel in bits.
  // Requires WP2_BITTRACE if the image is lossy.
  Plane<float>* bits_per_pixel = nullptr;

  // If not null, will be filled with a copy of the internal colorspace
  // transform if present in the bitstream.
  CSPTransform* csp = nullptr;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_WP2_DEBUG_H_
