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
//   CodedBlockBase, base class for decoding
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_LOSSY_BLOCK_H_
#define WP2_COMMON_LOSSY_BLOCK_H_

#include <cstdint>
#include <limits>
#include <string>

#include "src/common/global_params.h"
#include "src/common/lossy/block_size.h"
#include "src/common/lossy/context.h"
#include "src/common/lossy/predictor.h"
#include "src/common/lossy/quant_mtx.h"
#include "src/common/lossy/residuals.h"
#include "src/common/lossy/transforms.h"
#include "src/common/symbols.h"
#include "src/dsp/dsp.h"
#include "src/utils/csp.h"
#include "src/utils/plane.h"
#include "src/utils/utils.h"
#include "src/wp2/base.h"
#include "src/wp2/debug.h"
#include "src/wp2/decode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

class FrontMgrBase;
class Segment;
class RndMtxSet;
struct Tile;

//------------------------------------------------------------------------------
// Class for accessing cached contexts around a block

// Class that caches contexts for speed-up.
class ContextCache {
 public:
  ContextCache();

  // Gets the large context for a given block.
  const int16_t* GetLarge(const CodedBlockBase& cb, Channel channel,
                          ContextType context_type);
  // Gets small context for a sub transform block.
  // Assumes cb.GetCodingParams(channel).split_tf is true.
  const int16_t* GetSmall(const CodedBlockBase& cb, Channel channel,
                          uint32_t tf_i, ContextType context_type);
  // Sets contexts for all parameters as uncomputed.
  void Reset(bool only_small_right_or_bot_contexts = false);

 protected:
  bool CheckContextCache(const CodedBlockBase& cb, Channel channel,
                         bool split_tf, uint32_t tf_i,
                         ContextType context_type);

  bool check_cache_ = true;  // Check the cache is valid (in dbg mode only).
  // Large context per channel, per context type.
  int16_t large_[4][kNumContextType][kMaxContextSize];
  bool is_computed_large_[4][kNumContextType];
  // Small context per channel, per context type, per sub tf.
  int16_t small_[4][kNumContextType][kMaxSplitBlocks][kMaxSubContextSize];
  bool is_computed_small_[4][kNumContextType][kMaxSplitBlocks];
};

//------------------------------------------------------------------------------
// Struct to reduce memory footprint. A block can have at most 4 transforms and
// each transform may contain kMaxBlockSizePix2 coeffs but there are no more
// than kMaxBlockSizePix2 per block too. Allocate only what is necessary while
// keeping the convenient index access.

static constexpr uint32_t kMaxNumTransformsPerBlock = 4;
// Splits 1 and 2 are swapped to store half in [0] and half in [1].
static constexpr uint32_t kBlockCoeffOffset[] = {
    0, kMaxBlockSizePix2 / 2, kMaxBlockSizePix2 / 4, kMaxBlockSizePix2 * 3 / 4};
STATIC_ASSERT_ARRAY_SIZE(kBlockCoeffOffset, kMaxNumTransformsPerBlock);

template <typename T>
struct BlockCoeffs {
  T* operator[](uint32_t index) { return &memory[kBlockCoeffOffset[index]]; }
  const T* operator[](uint32_t index) const {
    return &memory[kBlockCoeffOffset[index]];
  }

 private:
  T memory[kMaxBlockSizePix2];  // Internal storage.
};
typedef BlockCoeffs<int16_t> BlockCoeffs16;
typedef BlockCoeffs<int32_t> BlockCoeffs32;

//------------------------------------------------------------------------------
// CodedBlockBase

class CodedBlockBase {
 public:
  CodedBlockBase() = default;
  CodedBlockBase(const CodedBlockBase&) = delete;
  CodedBlockBase(CodedBlockBase&&) = default;
  ~CodedBlockBase() = default;

  // Sets range of legal YUV values (inclusive, used for reconstruction) to
  // ['yuv_min', 'yuv_max'].
  void SetRange(int16_t yuv_min, int16_t yuv_max);

  // Specifies the destination for reconstruction.
  void SetReconstructedOutput(YUVPlane* out);

  // Used by GetContext(). If context_cache == nullptr, the previous instance
  // will be used. It must be != nullptr the first time it is used.
  void SetContextInput(const YUVPlane& in,
                       ContextCache* context_cache = nullptr);

  // Returns true if the context of the block (neighboring pixels used for
  // prediction) all have the same value for the given channel.
  bool ContextIsConstant(Channel channel) const;

  // Computes the prediction to 'out' for the sub block 'tf_i' (if
  // GetParams(channel).split_tf is true, otherwise tf_i must be 0).
  void PredictBlock(Channel channel, uint32_t tf_i, int16_t* out,
                    uint32_t step) const;

  // Decoder-only: Predict + Transform + Reconstruct in one minimal step.
  // Upon return, coeffs_[][] contains the transformed coeffs, for info.
  void Reconstruct(Channel channel, uint32_t tf_i);

  // Exports debug info during decoding.
  void ToBlockInfo(bool use_aom_coeffs, struct BlockInfo* blk) const;

  // Printable string for debug.
  std::string Print() const;

  // Context
  static constexpr int16_t kMissing = std::numeric_limits<int16_t>::max();

  // Retrieves boundary samples for the whole block.
  const int16_t* GetContext(Channel channel, ContextType context_type) const;
  const int16_t* GetContext(Channel channel, bool split_tf, uint32_t tf_i,
                            ContextType context_type) const;
  void SetContextCache(ContextCache* context_cache);
  void ResetContextCache() const;

  // Returns left and right occupancy, in kMinBlockSizePix units.
  // If top_context_extent is not nullptr, fills it with the possible extent of
  // the top context.
  void GetOccupancy(int8_t* left, int8_t* right,
                    int8_t* top_context_extent) const;

  // Coding parameters for a given channel.
  struct CodingParams {
    // Operators.
    bool operator==(const CodingParams& other) const;
    bool operator!=(const CodingParams& other) const;

    // Returns the frequency domain transform for the horizontal axis.
    WP2TransformType tf_x() const { return kTfX[tf]; }
    // Returns the frequency domain transform for the vertical axis.
    WP2TransformType tf_y() const { return kTfY[tf]; }

    // Predictor.
    const Predictor* pred = nullptr;
    // Frequency domain transform.
    TransformPair tf = kDctDct;
    // Whether to split the block into smaller transforms or not.
    bool split_tf = false;
  };

  const CodingParams& GetCodingParams(Channel channel) const;
  CodingParams* GetCodingParams(Channel channel);

  // Returns the number of sub-transforms.
  uint32_t GetNumTransforms(Channel channel) const;

  // Returns the number of coefficients per split transform, taking is420 into
  // account.
  uint32_t NumCoeffsPerTransform(Channel channel) const;

  // Returns true if the transform implies that the first coeff is DC.
  bool IsFirstCoeffDC(Channel channel) const;

  // Returns true if there is at least one non zero coeff in the block.
  bool HasCoeffs(Channel channel) const;

  // Accessors and setters (padded).
  uint32_t x() const { return blk_.x(); }
  uint32_t x_pix() const { return blk_.x_pix(); }
  uint32_t y() const { return blk_.y(); }
  uint32_t y_pix() const { return blk_.y_pix(); }
  uint32_t w() const { return blk_.w(); }
  uint32_t w_pix() const { return blk_.w_pix(); }
  uint32_t h() const { return blk_.h(); }
  uint32_t h_pix() const { return blk_.h_pix(); }
  Rectangle AsRect(uint32_t offset_x = 0, uint32_t offset_y = 0) const {
    return blk_.AsRect(offset_x, offset_y);
  }

  BlockSize dim() const { return blk_.dim(); }
  // Returns the size of the transform but taking into account the potential
  // size reduction due to is420_ with U/V.
  TrfSize tdim(Channel channel) const {
    return GetTransform(
        GetSplitSize(dim(), GetCodingParams(channel).split_tf),
        (channel == kUChannel || channel == kVChannel) && is420_);
  }
  Block blk() const { return blk_; }
  // Sets position/dimension based on 'block', occupancy based on 'mgr'.
  void SetDim(const Block& block, const FrontMgrBase& mgr);
  // When no mgr is provided, left block will be considered available and the
  // right block not available.
  void SetDimDefault(const Block& block, bool full_left_ctx = false);
  void SetXY(const FrontMgrBase& mgr, uint32_t x, uint32_t y);
  // Returns the plane we use to compute the context.
  const Plane16& GetContextPlane(Channel channel) const {
    return predict_from_.GetChannel(channel);
  }

  // Returns true if this block has alpha residuals.
  bool HasLossyAlpha() const;

  friend ContextCache;

 protected:
  // Returns the prediction context (surrounding pixels) for the block, or for a
  // sub block if 'split_tf' is true.
  // 'split_x' and 'split_y' are in pixels, relative to the block itself. They
  // should be (0, 0) is 'split_tf' is false.
  void GetContext(Channel channel, bool split_tf, uint32_t split_x,
                  uint32_t split_y, ContextType context_type,
                  int16_t context[]) const;

 public:
  //////////////// Methods for Visual Debug ////////////////

  // Visual debug: stores transform at block position
  void StoreTransform(const WP2::DecoderConfig& config, uint32_t tile_x,
                      uint32_t tile_y, ArgbBuffer* debug_output) const;
  // Visual debug: stores residuals at block position
  void StoreResiduals(const DecoderConfig& config, uint32_t tile_x,
                      uint32_t tile_y, const QuantMtx& quant, Channel channel,
                      Plane16* dst_plane) const;
  // Visual debug: stores prediction modes
  void StorePredictionModes(const WP2::DecoderConfig& config,
                            const Rectangle& tile_rect, Channel channel,
                            uint32_t tf_i, const Predictors& preds,
                            Plane16* raw_prediction,
                            ArgbBuffer* debug_output) const;
  // Visual debug: append the values of the original pixels as text
  void AppendOriginalPixels(const DecoderConfig& config, uint32_t tile_x,
                            uint32_t tile_y, const CSPTransform& csp_transform,
                            ArgbBuffer* debug_output) const;
  // Visual debug: append the values of the original pixels as text
  void AppendCompressedPixels(const DecoderConfig& config, uint32_t tile_x,
                              uint32_t tile_y, ArgbBuffer* debug_output) const;
  // Visual debug: stores coeff methods
  void StoreCoeffMethod(const WP2::DecoderConfig& config,
                        Plane16* dst_plane) const;
  // Visual debug: stores whether there are coeffs
  void StoreHasCoeffs(const WP2::DecoderConfig& config,
                      Plane16* dst_plane) const;
  // Visual debug: stores the slope for Chroma from luma ('a' in 'chroma =
  // a * luma + b'), after doing a linear regression on the context.
  void StoreCflSlope(Channel channel, int16_t yuv_min, int16_t yuv_max,
                     Plane16* dst_plane, std::string* debug_str) const;
  // Visual debug: stores the intercept for Chroma from luma ('b' in 'chroma =
  // a * luma + b'), after doing a linear regression on the context.
  void StoreCflIntercept(Channel channel, int16_t yuv_min, int16_t yuv_max,
                         Plane16* dst_plane, std::string* debug_str) const;

  // VisualDebug (for config 'blocks')
  WP2Status Draw(const DecoderConfig& config, const Tile& tile,
                 const GlobalParams& gparams, ArgbBuffer* debug_output) const;

 public:
  uint32_t left_occupancy_;   // occupancy along the left edge
  uint32_t right_occupancy_;  // occupancy along the right edge
  // Length of the top context beyond the top-right corner of the block.
  uint32_t top_context_extent_;
  uint8_t id_;  // segment id
  bool is420_;  // reduced transform

  uint8_t mtx_[2];        // index of rnd_mtx to use
  bool use_mtx_ = false;  // true if rnd_mtx should be used
  const RndMtxSet* mtx_set_ = nullptr;
  template <class T>
  bool CheckUseMtx(TrfSize trf, const T& mtx);

  // The input/output samples must be views of buffers external to CodedBlock
  // pointing to the rectangle covered by 'blk_'. Except for 'predict_from_',
  // they will not be accessed outside of these bounds.
  mutable YUVPlane out_;  // coded samples (output), within blk_

  // Encoder: Quantized residuals for Y/U/V/A, aka coefficients.
  // Decoder: inv-transformed coefficients (TODO(skal): remove?)
  BlockCoeffs16 coeffs_[4];
  // Size of the coefficients for Y/U/V/A per split transform.
  // Encoder:
  //   If size == 0, all coefficients are 0. If size != 0, the coefficient at
  //   size - 1 is the last non-zero one.
  // Decoder:
  //   total number of non-zero coefficients
  uint32_t num_coeffs_[4][kMaxNumTransformsPerBlock];

  BlockAlphaMode alpha_mode_ = kBlockAlphaLossless;
  // Approximate number of bytes taken up by lossless alpha in this block.
  uint32_t alpha_lossless_bytes_ = 0;
  bool y_context_is_constant_ = false;  // only used for encoding

  // Encoding method for the Y, U, V and A residuals per split transform.
  EncodingMethod method_[4][kMaxNumTransformsPerBlock];

  // Min/max YUV values, used to clip output when reconstructing.
  int16_t yuv_min_;
  int16_t yuv_max_;

  // Chroma from luma parameters for U, V, A when using CflSignalingPredictor.
  // Fractional precision is kIOFracBits (total number of bits is kIOBits).
  int16_t cfl_[3] = {0};

#if defined(WP2_BITTRACE)  // extra store for debugging
  // Original uncompressed residuals in spatial domain.
  int16_t (*original_res_)[4][kMaxNumTransformsPerBlock][kMaxBlockSizePix2] =
      nullptr;
#endif

 protected:
  ContextCache* context_cache_ = nullptr;
  YUVPlane predict_from_;  // context (input), within 1px left/top/right of blk_

  CodingParams params_[3];  // Y/UV/A
  Block blk_;               // position and dimension
};

}  // namespace WP2

#endif /* WP2_COMMON_LOSSY_BLOCK_H_ */
