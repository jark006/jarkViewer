// Copyright 2021 Google LLC
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
//   CodedBlock, for encoding
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_ENC_BLOCK_ENC_H_
#define WP2_ENC_BLOCK_ENC_H_

#include <cstdint>
#include <memory>
#include <string>

#include "src/common/global_params.h"
#include "src/common/lossy/block.h"
#include "src/common/lossy/context.h"
#include "src/common/lossy/predictor.h"
#include "src/common/lossy/quant_mtx.h"
#include "src/common/lossy/transforms.h"
#include "src/common/symbols.h"
#include "src/utils/plane.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/debug.h"
#include "src/wp2/encode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

class SymbolReader;
class SymbolCounter;
class UpdatingSymbolCounter;
class SymbolRecorder;

//------------------------------------------------------------------------------

// Context passed around and updated once we know the right parameters for each
// block. Currently quite empty but can be removed or extended when needed.
class BlockContext {
 public:
  // 'use_aom' defines the usage of AOM residuals.
  WP2Status Init(bool use_aom, uint32_t num_segments, bool explicit_segment_ids,
                 uint32_t width) {
    use_aom_ = use_aom;
    WP2_CHECK_STATUS(segment_id_predictor_.InitWrite(
        num_segments, explicit_segment_ids, width));
    return WP2_STATUS_OK;
  }
  WP2Status CopyFrom(const BlockContext& other) {
    use_aom_ = other.use_aom_;
    WP2_CHECK_STATUS(
        segment_id_predictor_.CopyFrom(other.segment_id_predictor_));
    return WP2_STATUS_OK;
  }
  void Reset() {}
  bool use_aom() const { return use_aom_; }
  SegmentIdPredictor& segment_id_predictor() { return segment_id_predictor_; }
  const SegmentIdPredictor& segment_id_predictor() const {
    return segment_id_predictor_;
  }

 private:
  bool use_aom_ = false;
  SegmentIdPredictor segment_id_predictor_;
};

//------------------------------------------------------------------------------

// Cost in bits for a given block, channel and symbol set, if estimated.
struct BlockRate {
  bool is_defined;
  float rate;  // Valid only if 'is_defined' is true.
};

// All the measures and estimations necessary to compute the score of a block
// for a given channel.
struct BlockRates {
  // Computes the aggregated score given by the distortion and the available
  // estimated bit costs.
  float GetScore() const;

  // Returns true if this uses the same '*_rate' and 'lambda' as 'other'.
  bool HasSameScoreFormulaAs(const BlockRates& other) const;
  // Returns true if this uses at most the same '*_rate' as 'other'.
  bool HasScoreFormulaSubsetOf(const BlockRates& other) const;

  uint64_t distortion = 0;  // Squared error sum.
  BlockRate predictor_rate = {false, 0.f};
  BlockRate transform_rate = {false, 0.f};
  BlockRate segment_id_rate = {false, 0.f};
  BlockRate residuals_rate = {false, 0.f};
  float lambda = 0.f;  // Bit cost multiplier for scoring.
};

//------------------------------------------------------------------------------

// Class containing several SymbolCounters for reuse (to avoid too many
// mallocs).
class Counters : public WP2Allocable {
 public:
  WP2Status Init(uint32_t effort, bool use_aom_residuals,
                 const SymbolRecorder& recorder);
  WP2Status CopyFrom(const Counters& other, const SymbolRecorder& recorder);
  const SymbolRecorder* recorder() const { return recorder_; }
  // Returns a pointer to the counter with symbols related to:
  SymbolCounter* predictor() { return predictor_.get(); }
  SymbolCounter* transform() { return transform_.get(); }
  SymbolCounter* segment_id() { return segment_id_.get(); }
  SymbolCounter* residuals() { return residuals_; }

 private:
  static constexpr uint32_t kSlowCounterEffortThreshold = 5;
  uint32_t effort_;
  bool use_aom_residuals_;
  std::unique_ptr<SymbolCounter> predictor_;
  std::unique_ptr<SymbolCounter> transform_;
  std::unique_ptr<SymbolCounter> segment_id_;
  // Either the fast or slow version is used depending on 'effort'.
  std::unique_ptr<SymbolCounter> residuals_fast_;
  std::unique_ptr<UpdatingSymbolCounter> residuals_slow_;
  SymbolCounter* residuals_;
  const SymbolRecorder* recorder_ = nullptr;
};

//------------------------------------------------------------------------------
// CodedBlock

// Class for encoder.
class CodedBlock : public CodedBlockBase {
 public:
  CodedBlock() = default;
  CodedBlock(const CodedBlock&) = delete;
  CodedBlock(CodedBlock&&) = default;
  ~CodedBlock() = default;

  // Set the views on the source and destination buffers based on 'blk_'.
  // Position and size must be set prior to calling these.
  void SetSrcInput(const YUVPlane& in);  // Used by GetDisto().

  // Decides whether to use yuv420 based on coeffs_[kUChannel or kVChannel].
  WP2Status DecideChromaSubsampling(const EncoderConfig& config,
                                    const BlockContext& context,
                                    uint32_t tile_pos_x, uint32_t tile_pos_y,
                                    bool has_alpha, const QuantMtx& quant_u,
                                    const QuantMtx& quant_v,
                                    Counters* counters);

  // Subtracts the 'prediction' from 'in_' and stores the residuals in 'res'.
  void GetResiduals(Channel channel, uint32_t tf_i, const Plane16& prediction,
                    BlockCoeffs16* res) const;

  // Applies the TransformPair to 'res', quantizes the 'coeffs_' and
  // reconstructs the pixels to 'out_' using the 'prediction'.
  // 'tmp' is used for temporary computation. It can be the same as 'res' if
  // the caller doesn't mind losing the data.
  void TransformAndReconstruct(const EncoderConfig& config,
                               const BlockContext& context,
                               const Segment& segment, Channel channel,
                               uint32_t tf_i, bool reduced_transform,
                               const Plane16& prediction,
                               const BlockCoeffs16* res, BlockCoeffs16* tmp,
                               Counters* counters);

  // Predicts, quantizes and reconstructs channel 'channel'. If
  // 'reduced_transform' is true, 2x downsampling of residuals is performed.
  // Applies to...
  // ... the given sub transform 'tf_i'
  void Quantize(const EncoderConfig& config, const BlockContext& context,
                const Segment& segment, Channel channel, uint32_t tf_i,
                bool reduced_transform, Counters* counters);
  // ... all sub transforms
  void QuantizeAll(const EncoderConfig& config, const BlockContext& context,
                   const Segment& segment, Channel channel,
                   bool reduced_transform, Counters* counters);
  // ... all sub transforms except the first one (noop if there is only one)
  void QuantizeAllButFirst(const EncoderConfig& config,
                           const BlockContext& context, const Segment& segment,
                           Channel channel, bool reduced_transform,
                           Counters* counters);

  // Reconstructs 'channel' from dequantized residuals 'res' and 'prediction'
  // to 'out_'. 'prediction' can also point to 'out_' or be empty.
  // Upon return, res[] contains the inverse-transformed coeffs.
  void Reconstruct(Channel channel, uint32_t tf_i, bool reduced_transform,
                   BlockCoeffs16* res,
                   const Plane16& prediction = Plane16()) const;

  // Calls the base method and store some extra info
  // in 'blk'. Not a virtual.
  void ToBlockInfo(bool use_aom_coeffs, BlockInfo* blk) const;

  // Returns the sum of square error between 'p_in_' and 'p_out_' for the pixels
  // inside the block (padded).
  uint64_t GetDisto(Channel channel) const;

  // Pixel transfer. 'yuv' is the full plane, not the block's sub-view.
  // No bound-check or clipping is performed.
  void ExtractFrom(const YUVPlane& yuv, Channel channel) const;

  // Returns an estimation of the number of bits necessary to encode the
  // predictor modes.
  float PredictorRate(Channel channel, SymbolCounter* counter) const;

  // Returns an estimation of the number of bits necessary to encode the
  // transform.
  float TransformRate(Channel channel, SymbolCounter* counter) const;

  // Returns an estimation of the number of bits necessary to encode the
  // segment id.
  float SegmentIdRate(const BlockContext& context,
                      SymbolCounter* counter) const;

  // Estimated cost in bits of storing residuals for the given channel.
  float ResidualRate(const BlockContext& context, Channel channel,
                     uint32_t num_channels, SymbolCounter* counter) const;

  // Pseudo-Visual debug: force the split_tf, tf or pred of the block.
  enum class SplitTf { kForcedUnsplit, kForcedSplit, kUnknown };
  SplitTf GetForcedSplitTf(const EncoderConfig& config,
                           const Rectangle& tile_rect) const;
  TransformPair GetForcedTransform(const EncoderConfig& config,
                                   const Rectangle& tile_rect) const;
  const Predictor* GetForcedPredictor(const EncoderConfig& config,
                                      const Rectangle& tile_rect,
                                      const Predictors& preds,
                                      Channel channel) const;

 public:
  //////////////// Methods for Visual Debug ////////////////

  // Visual debug: stores original residuals (before quantization) at block
  // position.
  void StoreOriginalResiduals(
      const EncoderConfig& config, uint32_t tile_pos_x, uint32_t tile_pos_y,
      int16_t original_res[kMaxNumTransformsPerBlock][kMaxBlockSizePix2],
      Plane16* dst_plane) const;
  // Visual debug: stores prediction scores
  void StorePredictionScore(const EncoderConfig& config,
                            const Rectangle& tile_rect, Channel channel,
                            const Predictor& pred, TransformPair tf,
                            uint32_t segment_id, const BlockRates& results,
                            bool is_best) const;

  // Visual debug: stores information around the chroma subsampling decision.
  WP2Status Store420Scores(const EncoderConfig& config, uint32_t pos_x,
                           uint32_t pos_y, float lambda_u, float lambda_v,
                           bool reduced, uint32_t disto, float rate_u,
                           float rate_v);
  enum class Debug420Decision { k444EarlyExit, k444, k420 };
  WP2Status Store420Decision(const EncoderConfig& config, uint32_t pos_x,
                             uint32_t pos_y, Debug420Decision decision) const;

  // Visual debug: store lambda multiplier map.
  WP2Status StoreLambdaMult(const EncoderConfig& config, uint32_t pos_x,
                            uint32_t pos_y) const;

  // Visual debug: stores propagated error or new error for U/V error diffusion.
  void StoreErrorDiffusion(const EncoderConfig& config, uint32_t tile_x,
                           uint32_t tile_y, Plane16* dst_plane) const;

  // Visual debug: stores raw (unquantized) residuals after ideal chroma from
  // luma prediction.
  void StoreBestCflResiduals(Channel channel, int16_t yuv_min, int16_t yuv_max,
                             Plane16* dst_plane, std::string* debug_str) const;

  // Visual debug: stores the best possible prediction of the given channel,
  // assuming we could transmit the best slope/bias instead of deducing them
  // from context.
  void StoreBestCflPrediction(Channel channel, int16_t yuv_min, int16_t yuv_max,
                              Plane16* dst_plane, std::string* debug_str) const;
  // Visual debug: stores the best slope for Chroma from luma ('a' in 'chroma =
  // a * luma + b'), after doing a linear regression between the output luma and
  // input chroma.
  void StoreBestCflSlope(Channel channel, int16_t yuv_min, int16_t yuv_max,
                         Plane16* dst_plane, std::string* debug_str) const;
  // Visual debug: stores the best intercept for Chroma from luma ('b' in
  // 'chroma = a * luma + b'), after doing a linear regression between the
  // output luma and input chroma.
  void StoreBestCflIntercept(Channel channel, int16_t yuv_min, int16_t yuv_max,
                             Plane16* dst_plane, std::string* debug_str) const;

 private:
  void CflLinearRegression(Channel channel, int16_t yuv_min, int16_t yuv_max,
                           float* a, float* b, std::string* debug_str) const;

 public:
  // Input and output samples must be views of buffers external to CodedBlock
  // pointing to the rectangle covered by 'blk_'. Except for 'predict_from_',
  // they will not be accessed outside of these bounds.
  YUVPlane in_;  // source samples (input), within blk_

  float pred_scores_[4] = {0};  // per channel, only available during encoding

  // Diffusion errors, indexed by Y/U/V channels.
  int16_t dc_error_[3] = {0};
  int16_t dc_error_next_[3] = {0};

  // Per-block lambda modulation. Larger multiplier will favor rate over
  // distortion for the block: should be assigned to highly textured blocks.
  float lambda_mult_ = 1.f;
};

//------------------------------------------------------------------------------

// Pred/tf/split score struct.
struct BlockScore {
  // For sorting.
  bool operator<(const BlockScore& other) const { return IsBetterThan(other); }

  bool IsDefined() const { return params.pred != nullptr; }
  // Sums all channels.
  float GetScore() const;
  float GetDistortion() const;
  // Returns true if 'other' has a better (=lower) score than this.
  bool IsBetterThan(const BlockScore& other) const;
  // Same as above but has weaker assertions to compare partial results.
  // 'other' must be final.
  bool MightBeBetterThan(const BlockScore& other) const;

  // Encoding settings.
  CodedBlock::CodingParams params;
  uint8_t segment_id = 0;

  // Result for {luma Y, none}, {chroma U, chroma V} or {alpha, none}:
  BlockRates results[2];
};

// Pred/tf/split score computation.
class BlockScorer {
 public:
  // Initializes the object for the whole tile.
  WP2Status Init(const EncoderConfig& config, const GlobalParams& gparams,
                 Rectangle tile_rect);
  // Sets the block to be encoded with the various configuration combinations.
  // Forgets the best combinations found so far and resets the cached residuals.
  void Set(const BlockContext& context, CodedBlock* cb, Counters* counters);

  // Computes the scores and the distortions of the block for each combination
  // of the configurations below.
  // Can be called several times in a row as long as the configurations differ.
  // If 'refined_preds' is not null, it will be filled with "true" for each
  // refined predictor mode.
  WP2Status ComputeScore(const VectorNoCtor<bool>** refined_preds = nullptr);

  // Access results.
  const BlockScore& GetBestCombination() const;

  // Sets the block to the best combination found and reconstruct it.
  void ReconstructBestCombination();

  // For convenience.
  const GlobalParams& GetGlobalParams() const { return *gparams_; }

  // Configurations. Each must be set prior to calling ComputeScore().
  Vector<Channel> channels_;             // Y, A or U+V.
  Vector<bool> splits_;                  // False, true or false+true.
  Vector<const Predictor*> predictors_;  // List of predictors to try.
  Vector<TransformPair> transforms_;     // List of transforms to try.
  Vector<uint8_t> segment_ids_;          // List of segment ids to try.

 private:
  // 'forced' indicates some encoding param is set to a given value.
  WP2Status ComputeScoreForEachPredictor(bool forced);
  WP2Status ComputeScoreForEachTransform(bool forced);
  WP2Status ComputeScoreForEachSegment(bool forced);
  WP2Status ComputeScoreForEachChannel(bool forced);

  // Depending on the 'config_', find which sub-predictors might give good
  // results based on the scores of the 'best_combinations_angle_'.
  void RefineBestAnglePredictors();

  // Returns true if the 'combination' can be skipped with no side effect.
  bool CanBeSkipped(const BlockScore& combination) const;

  // Const settings common to the whole tile.
  const EncoderConfig* config_;
  const GlobalParams* gparams_;
  Rectangle tile_rect_;

  // Block.
  CodedBlock* cb_;
  const BlockContext* context_;
  Counters* counters_;
  float lambdas_[4];  // Bit cost multipliers for scoring of Y, U, V, A.

  // Cached results to avoid computing the same residuals twice during different
  // configuration combinations of the same block and channel.
  const Predictor* cached_predictor_;  // Predictor used to generate the
                                       // 'prediction_cache_'. Null if not yet.
  bool cached_split_tf_;  // Param when the 'prediction_cache_' was generated.
                          // Only valid if 'cached_predictor_' is not null.
  BlockCoeffs16 prediction_cache_[2];  // Cached top left sub-block prediction.
  BlockCoeffs16 residuals_cache_[2];   // Residuals of 'prediction_cache_'.

  CodedBlock::CodingParams last_params_;  // Last reconstructed samples.
  uint8_t last_segment_id_;  // Only valid if 'last_params_' != CodingParams().

  // Best combination found so far for 'cb_'.
  BlockScore best_combination_;
  BlockScore best_combination_no_angle_;  // Same but only among no-angle preds.
  VectorNoCtor<BlockScore> best_combinations_angle_;  // Best angle preds.
  bool refine_;  // If false, do not bother keeping 'best_combination_no_angle_'
                 // nor 'best_combinations_angle_'.

  VectorNoCtor<bool> refined_preds_;  // Mode index to "should-be-evaluated".
                                      // Stored here to avoid reallocations.
};

//------------------------------------------------------------------------------

// Used to store the configurations of BlockScorer for one channel.
struct BlockModes {
  Vector<bool> splits_tried_during_preds;
  Vector<bool> splits_tried_after_preds;
  Vector<const Predictor*> main_preds;
  Vector<const Predictor*> sub_preds;
  Vector<TransformPair> tf_tried_during_preds;
  Vector<TransformPair> tf_tried_after_preds;
  Vector<uint8_t> segment_ids_tried_during_preds;  // When empty, keep the value
  Vector<uint8_t> segment_ids_tried_after_preds;   // of CodedBlock::id_ as is.
};

// Finds the best BlockModes combination. Then reconstructs the samples with it.
WP2Status OptimizeModes(const EncoderConfig& config, const Rectangle& tile_rect,
                        Channel channel, const Predictors& preds,
                        const BlockModes& m, const BlockContext& context,
                        CodedBlock* cb, Counters* counters,
                        BlockScorer* scorer);

// Calls OptimizeModes() and DecideChromaSubsampling() then QuantizeAll().
WP2Status OptimizeModesChroma(const EncoderConfig& config,
                              const Rectangle& tile_rect, bool has_alpha,
                              const FrontMgrBase& mgr, const Predictors& preds,
                              ChromaSubsampling chroma_subsampling,
                              const BlockModes& modes,
                              const BlockContext& context, CodedBlock* cb,
                              Counters* counters, DCDiffusionMap* dc_error_u,
                              DCDiffusionMap* dc_error_v, BlockScorer* scorer);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_ENC_BLOCK_H_ */
