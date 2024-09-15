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
//   Analysis of source frame: partitioning, segmentation, ...
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_SRC_ENC_ANALYSIS_H_
#define WP2_SRC_ENC_ANALYSIS_H_

#include <cstdint>

#include "src/common/global_params.h"
#include "src/common/lossy/block.h"
#include "src/common/lossy/block_size.h"
#include "src/common/lossy/context.h"
#include "src/common/progress_watcher.h"
#include "src/utils/csp.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

class FeatureMap {
 public:
  static constexpr uint32_t kHistogramSize =
      kMinBlockSizePix * kMinBlockSizePix;

  Vector_u8 smap_;  // score map or segment-id map
  Vector_u8 cplx_;  // map of block's complexity (0 = easy, 255 = hard)
  struct NoiseStr {
    uint16_t strength[3];  // noise strength [0;100]
    uint16_t freq[3];      // noise frequency [0;6]
  };
  VectorNoCtor<NoiseStr> noise_map_;
  uint32_t step_;                         // step for smap_ and noise_map_
  uint32_t cluster_map_[kHistogramSize];  // map from score to segment id
};

// Returns whether to use color samples premultiplied by alpha internally.
bool DecidePremultiplied(WP2SampleFormat input_format,
                         const EncoderConfig& config);

// Returns whether to have lossless and/or lossy tiles depending on 'config'.
GlobalParams::Type DecideGlobalParamsType(const EncoderConfig& config);

// This function will populate *gparams and *gparams->features_, taking 'config'
// into account. It must be called prior to calling the tile encoding
// (LossyEncode(), LosslessEncode(), etc.).
WP2Status GlobalAnalysis(const ArgbBuffer& rgb, const YUVPlane& yuv,
                         const CSPTransform& transf,
                         const EncoderConfig& config, GlobalParams* gparams);

//------------------------------------------------------------------------------

// Returns a block layout. 'tile_rect' is in pixels, not padded.
// Input 'blocks' are considered forced.
WP2Status ExtractBlockPartition(const EncoderConfig& config,
                                const GlobalParams& gparams,
                                const YUVPlane& yuv, const Rectangle& tile_rect,
                                const ProgressRange& progress,
                                VectorNoCtor<Block>* blocks,
                                Vector_u32* splits);

// Finds a segmentation of the source in clusters with similar-features.
// gparam's 'segments_' vector is created but only their susceptibility
// characteristics (variance, risk_class, ...) are filled.
// The max number of segments is supplied by 'config' (fewer segments might
// be used if appropriate). The global feature map 'gparams->features_' is
// filled with the best segment ids and scores.
WP2Status FindSegments(const YUVPlane& yuv, const EncoderConfig& config,
                       GlobalParams* gparams);

// Using gparams.features_, assign noise/grain level to gparams->segments_
WP2Status AssignGrainParams(const YUVPlane& yuv, const EncoderConfig& config,
                            GlobalParams* gparams);
// Visual debug for grain.
void GrainVDebug(const EncoderConfig& config, uint32_t x_pix, uint32_t y_pix,
                 Channel channel, FeatureMap::NoiseStr& noise_info,
                 int32_t* coeffs);
void SegmentGrainVDebug(const EncoderConfig& config, const CodedBlock& cb,
                        uint32_t tile_pos_x, uint32_t tile_pos_y,
                        const Segment& segment);

// Returns the segment id of the 'block' based on the 'gparams'.
// The 'segment_score' can also be retrieved if not null.
uint8_t AssignSegmentId(const EncoderConfig& config,
                        const GlobalParams& gparams,
                        const Rectangle& padded_tile, const Block& block,
                        uint32_t* segment_score = nullptr);
// Assigns a segment to each of the 'blocks' (however blocks smaller than 8x8
// will be later assigned a predicted segment-id instead of this one).
WP2Status AssignSegmentIds(const EncoderConfig& config,
                           const GlobalParams& gparams,
                           const Rectangle& padded_tile,
                           Vector<CodedBlock>* cblocks);

// Segmentation debug information for a given rectangle.
void SegmentationGridVDebug(const EncoderConfig& config, const Rectangle& rect,
                            uint32_t tile_pos_x, uint32_t tile_pos_y,
                            float value);
// Segmentation debug information for a given block.
void SegmentationBlockVDebug(const EncoderConfig& config, const CodedBlock& cb,
                             uint32_t tile_pos_x, uint32_t tile_pos_y,
                             float score, const Segment& segment);

// Clusters the given 'histogram' using the k-means algorithm.
// 'clusters[]' must be 'histogram_size' long, and will contain the cluster id
// for each histogram bucket. Ids are in [0:num_clusters[, num_clusters being
// returned and is at most 'max_clusters'.
// 'centers[]' is the position of each cluster center.
uint32_t ClusterHistogram(const uint32_t histogram[], uint32_t histogram_size,
                          uint32_t max_clusters, uint32_t clusters[],
                          uint32_t centers[kMaxNumSegments]);

//------------------------------------------------------------------------------
// Near-lossless

// Allocate 'out_buffer' and pre-process 'in_buffer' near-losslessly, based
// on config's quality setting.
WP2Status PreprocessNearLossless(const ArgbBuffer& in_buffer,
                                 const EncoderConfig& config, bool is_alpha,
                                 ArgbBuffer* out_buffer);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_SRC_ENC_ANALYSIS_H_ */
