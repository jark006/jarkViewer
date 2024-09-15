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
//  Global parameters used by lossy / lossless tiles
//
// Author: Skal (pascal.massimino@gmail.com)
//
#ifndef WP2_COMMON_GLOBAL_PARAMS_H_
#define WP2_COMMON_GLOBAL_PARAMS_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/predictor.h"
#include "src/common/lossy/rnd_mtx.h"
#include "src/common/lossy/segment.h"
#include "src/utils/csp.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

class ANSEnc;
class ANSDec;
class FeatureMap;
struct DecoderConfig;

//------------------------------------------------------------------------------
// Global parameter

// Quant offset allows adjusting U/V quantization compared to Y.
static constexpr uint32_t kMinQuantOffset = 1, kMaxQuantOffset = 16;
static constexpr uint32_t kNeutralQuantOffset = 8;  // Same quantization.
static constexpr uint32_t kDefaultQuantOffset = 7;  // Slightly less quantized.

static constexpr uint32_t kMaxYuvFilterMagnitude = 15;

class GlobalParams {
 public:
  WP2Status Write(bool image_has_alpha, ANSEnc* enc) const;
  WP2Status Read(bool image_has_alpha, ANSDec* dec);

  void Reset();

 public:
  // TODO(skal): should we have a GP_NONE for super-small images with default
  //             global params?
  enum Type { GP_LOSSY = 0, GP_LOSSLESS, GP_BOTH, GP_AV1, GP_LAST };
  Type type_ = GP_LOSSY;

  // True if the frame contains alpha (either lossless or lossy).
  // Can only be true if 'BitstreamFeatures::has_alpha' is true as well.
  bool has_alpha_ = false;
  // TODO: b/359162718 - Consider adding an is_premultiplied_ field here.

  // lossy part
  CSPTransform transf_;

  // True if segment ids are signaled per block. Otherwise they are
  // deduced from the block sizes.
  bool explicit_segment_ids_ = false;

  PartitionSet partition_set_ = PartitionSet::ALL_SQUARES;
  bool partition_snapping_ = false;

  bool use_rnd_mtx_ = false;
  RndMtxSet mtx_set_;

  // Chroma quantization offsets.
  uint32_t u_quant_offset_ = kDefaultQuantOffset;
  uint32_t v_quant_offset_ = kDefaultQuantOffset;

  YPredictors y_preds_;
  UVPredictors uv_preds_;
  APredictors a_preds_;
  Vector<Segment> segments_;

  // True if the image might contain lossy alpha.
  bool maybe_use_lossy_alpha_ = false;
  FeatureMap* features_ = nullptr;  // only used by the encoder

  // Post-processing.
  uint32_t yuv_filter_magnitude_ = 0;  // In [0:kMaxYuvFilterMagnitude].
  bool enable_alpha_filter_ = false;   // For lossy alpha only.

 public:  // lossless part
          // TODO(skal)
 public:
  const Predictors& GetPredictors(Channel channel) const {
    if (channel == kAChannel) return a_preds_;
    if (channel != kYChannel) return uv_preds_;
    return y_preds_;
  }

  // initialize y_preds_ / uv_preds_ / a_preds_
  WP2Status InitFixedPredictors();

  WP2Status InitRndMtxSet();

  // verifies that internal parameters are within valid ranges
  bool IsOk() const;

  // Returns the DC symbols' coding range.
  uint32_t GetMaxDCRange(Channel channel) const;

  // These functions are responsible for mapping the segment's risk_ factor
  // to a set of quant factors, and applying them on segments.
  WP2Status AssignQuantizations(const EncoderConfig& config);
  WP2Status AssignAlphaQuantizations(const YUVPlane& yuv,
                                     const EncoderConfig& config);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_COMMON_GLOBAL_PARAMS_H_
