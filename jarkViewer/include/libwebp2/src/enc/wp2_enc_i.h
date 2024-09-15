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
//   WP2 encoder: internal header.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_WP2_ENC_I_H_
#define WP2_WP2_ENC_I_H_

#include <array>
#include <cstdint>

#include "src/common/global_params.h"
#include "src/common/lossy/block_size.h"
#include "src/common/lossy/context.h"
#include "src/common/lossy/residuals.h"
#include "src/common/progress_watcher.h"
#include "src/common/symbols.h"
#include "src/enc/block_enc.h"
#include "src/enc/lossless/losslessi_enc.h"
#include "src/enc/symbols_enc.h"
#include "src/utils/ans_enc.h"
#include "src/utils/front_mgr.h"
#include "src/utils/plane.h"
#include "src/utils/split_iterator.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

// Resets 'EncoderConfig::info' if any. Not thread-safe.
WP2Status SetupEncoderInfo(uint32_t width, uint32_t height,
                           const EncoderConfig& config);

// Writes WP2 header to 'output' based on 'config' and other arguments.
WP2Status EncodeHeader(const EncoderConfig& config, uint32_t width,
                       uint32_t height, uint32_t rgb_bit_depth, bool has_alpha,
                       bool is_premultiplied, bool is_anim, bool loop_forever,
                       Argb38b background_color, RGB12b preview_color,
                       bool has_icc, bool has_trailing_data, Writer* output);

// Creates and compresses a preview from 'buffer' based on 'config' and writes
// it to 'output'.
WP2Status EncodePreview(const ArgbBuffer& buffer, const EncoderConfig& config,
                        const ProgressRange& progress, Writer* output);

// Writes 'iccp' to 'output'.
WP2Status EncodeICC(DataView iccp, Writer* output);

// Writes global parameters 'gparams' to 'output'.
WP2Status EncodeGLBL(const EncoderConfig& config, const GlobalParams& gparams,
                     bool image_has_alpha, Writer* output);

// Writes 'metadata' to 'output'.
WP2Status EncodeMetadata(const Metadata& metadata, Writer* output);

// Returns the TileShape, converting from TILE_SHAPE_AUTO to a concrete
// one if necessary.
TileShape FinalTileShape(const EncoderConfig& config);

// Returns the partition method used at encoding.
// 'tile_width' and 'tile_height' are in pixels, padded or not.
PartitionMethod FinalPartitionMethod(const EncoderConfig& config,
                                     uint32_t tile_width, uint32_t tile_height);

//------------------------------------------------------------------------------

// Choose encoding settings based on the given 'config'.
ChromaSubsampling DecideChromaSubsampling(const EncoderConfig& config,
                                          bool more_than_one_block);
bool DecideAOMCoeffs(const EncoderConfig& config, const Rectangle& tile_rect);
WP2Status DecideModes(const EncoderConfig& config, const GlobalParams& gparams,
                      BlockModes* y_modes, BlockModes* uv_modes,
                      BlockModes* a_modes = nullptr);

//------------------------------------------------------------------------------

WP2Status NeuralEncode(const ArgbBuffer& buffer, const EncoderConfig& config,
                       ANSEnc* enc);

//------------------------------------------------------------------------------

// Class specialized in writing transform residuals to a stream.
class ResidualWriter : public ResidualIO {
 public:
  // Initializes the underlying memory.
  WP2Status Init(bool use_aom_coeffs, bool has_alpha);

  // Default copy is enough to create a deep clone of the full state.
  void CopyFrom(const ResidualWriter& other) { operator=(other); }

  // Finds the best encoding method for the given 'num_coeffs'.
  // If 'cost' is not nullptr, it is set to the cost of storing the residuals
  // with this method (coefficients only, not including dictionaries).
  static void FindBestEncodingMethod(TrfSize dim, const int16_t* coeffs,
                                     uint32_t num_coeffs, bool first_is_dc,
                                     Channel channel, uint32_t num_channels,
                                     SymbolCounter* counter,
                                     EncodingMethod* encoding_method,
                                     float* cost = nullptr);

  // Records residuals from 'cb'.
  void RecordCoeffs(const CodedBlock& cb, Channel channel,
                    SymbolRecorder* recorder) const;

  // Writes the data (e.g. dictionaries) needed to interpret the symbols that
  // will later be written one by one.
  WP2Status WriteHeader(uint32_t num_coeffs_max_y, uint32_t num_coeffs_max_uv,
                        uint32_t num_transforms, bool has_lossy_alpha,
                        const SymbolRecorder& recorder, ANSDictionaries* dicts,
                        ANSEncBase* enc, SymbolWriter* sw);

  // Writes residuals from 'cb'.
  void WriteCoeffs(const CodedBlock& cb, Channel channel, ANSEncBase* enc,
                   SymbolManager* sm) const;

  // Returns a (very) crude estimation of the number of bits needed to code
  // the residuals.
  // The logic for storing the residuals is the same as WriteCoeffs
  // except symbol statistics and adaptive symbols do not use global statistics
  // but fixed ones.
  static float GetPseudoRate(Channel channel, uint32_t num_channels,
                             TrfSize dim, const int16_t* coeffs,
                             uint32_t num_coeffs, bool first_is_dc,
                             SymbolCounter* counter);
  // Returns a slightly less crude estimation of the number of bits needed to
  // code the residuals. Much slower than the pseudo rate above.
  static float GetRate(Channel channel, uint32_t num_channels, TrfSize dim,
                       const int16_t* coeffs, uint32_t num_coeffs,
                       bool first_is_dc, SymbolCounter* counter,
                       EncodingMethod* encoding_method = nullptr);
  static float GetRateAOM(const CodedBlock& cb, Channel channel,
                          SymbolCounter* counter);

 private:
  // Calls WriteHeader on SymbolWriter for symbols that do depend on a
  // specific residual method.
  WP2Status WriteHeaderForResidualSymbols(Channel channel,
                                          uint32_t num_coeffs_max,
                                          const SymbolRecorder& recorder,
                                          ANSEncBase* enc, SymbolWriter* sw,
                                          ANSDictionaries* dicts_in);

  // Stores the DC separately.
  // 'range' is the maximum absolute value of 'v'.
  // 'can_be_zero' specifies whether v can be 0.
  static void StoreDC(Channel channel, uint32_t num_channels, ANSEncBase* enc,
                      SymbolManager* sm, int16_t v, bool can_be_zero);
  // Stores residuals.
  // The residuals in 'coeffs' are stored in 'enc' and 'sm'.
  static void StoreCoeffs(const int16_t* coeffs, uint32_t num_coeffs,
                          bool first_is_dc, TrfSize dim, Channel channel,
                          uint32_t num_channels, EncodingMethod method,
                          ANSEncBase* enc, SymbolManager* sm,
                          bool is_pseudo_rate);
  uint32_t num_channels_;
};

// Class for writing the alpha plane.
class AlphaWriter {
 public:
  // Setup. 'tile_rect' is in pixels, not padded.
  WP2Status Init(const EncoderConfig& config, const GlobalParams& gparams,
                 const BlockContext& context, const YUVPlane& yuv,
                 const Rectangle& tile_rect, const ProgressRange& progress);

  // Deep copy. 'yuv, context, dicts' must be the clones of 'other' instances.
  WP2Status CopyFrom(const AlphaWriter& other, const BlockContext& context);

  // Decides cb->alpha_mode_. If lossy is used,
  // coefficients will be quantized and an encoding method decided.
  WP2Status DecideAlpha(CodedBlock* cb, const BlockModes& modes,
                        const ResidualWriter& residual_writer,
                        Counters* counters, BlockScorer* scorer);

  WP2Status ResetRecord();
  WP2Status Record(const CodedBlock& cb);
  void WriteBlockBeforeCoeffs(const CodedBlock& cb, SymbolManager* sm,
                              ANSEncBase* enc);
  WP2Status Write(const CodedBlock& cb, ANSEnc* enc);

  // Freezes and writes the dictionaries.
  WP2Status WriteHeader(uint32_t num_coeffs_max, ANSEncBase* enc);

  AlphaMode GetAlphaMode() const { return alpha_mode_; }

 private:
  WP2Status WriteLossless(const CodedBlock& cb, ANSEnc* enc);

  // Decides whether to use lossy residuals or plain black/white for a given
  // block. If using residuals, encoding params (predictors/transform) and
  // residuals are computed.
  WP2Status ProcessLossy(CodedBlock* cb, const BlockModes& modes,
                         const ResidualWriter& residual_writer,
                         const Vector_f& bits_per_pixel, Counters* counters,
                         BlockScorer* scorer);

  BlockAlphaMode GetBlockAlphaMode(const CodedBlock& cb,
                                   const Plane16& alpha) const;

  const GlobalParams* gparams_ = nullptr;
  Rectangle tile_rect_;  // In pixels, not padded.
  EncoderConfig config_;
  AlphaMode alpha_mode_;  // Global alpha mode.

  // *** Members variables for lossy encoding. ***
  // Mode predictor for individual blocks.
  AlphaModePredictor mode_predictor_;

  // *** Members variables for lossless encoding. ***
  // TODO(maryla): ArgbBuffer uses 4 channels even though we only need one...
  ArgbBuffer alpha_;
  // Separate encoder for lossless.
  ANSEnc lossless_enc_;
  WP2L::EncodeInfo lossless_encode_info_;
  // Next line to write.
  uint32_t next_line_ = 0;
  const BlockContext* context_;
};

// Class for holding ANS encoder and dictionaries.
class SyntaxWriter : public WP2Allocable {
 public:
  // 'has_alpha' is the same as buffer.HasTransparency. It is just here to
  // save CPU. 'tile_rect' is in pixels, not padded.
  WP2Status Init(ANSDictionaries* dicts, const EncoderConfig& config,
                 const GlobalParams& gparams, const YUVPlane& yuv,
                 ChromaSubsampling chroma_subsampling,
                 const Rectangle& tile_rect, uint32_t num_blocks,
                 bool use_aom_coeffs, bool use_splits,
                 const ProgressRange& alpha_progress);
  // Deep copy. 'dicts' must be copied beforehand.
  WP2Status CopyFrom(const SyntaxWriter& other, ANSDictionaries* dicts);
  // Initializes some data based on results from the previous pass. To be called
  // at the beginning of every pass.
  WP2Status InitPass();
  // Records the block's content, except for size and alpha.
  void Record(const CodedBlock& cb);
  // Records the block's size.
  void RecordSize(const FrontMgrDoubleOrderBase& mgr, BlockSize dim);
  void RecordSize(BlockSize dim, BlockSize max_possible_size,
                  PartitionSet partition_set);
  // Records the block's split.
  WP2Status RecordSplit(const SplitIteratorBase& mgr, uint32_t split_idx);
  // Writes the headers (frozen dictionaries, etc.).
  WP2Status WriteHeader(ANSEncBase* enc);
  // These write the block header (segment id, preds, etc.).
  void WriteBlockBeforeCoeffs(const CodedBlock& cb, SymbolManager* sm,
                              ANSEncBase* enc);
  // Writes the predictors used by the given block for the given channel.
  static void WritePredictors(const CodedBlock& cb, Channel channel,
                              SymbolManager* sm, ANSEncBase* enc);
  // Writes whether the block is split into smaller square transforms.
  static void WriteSplitTransform(const CodedBlock& cb, Channel channel,
                                  SymbolManager* sm, ANSEncBase* enc);
  // Writes whether at least one coeff is not zero.
  static void WriteHasCoeffs(const CodedBlock& cb, Channel channel,
                             SymbolManager* sm, ANSEncBase* enc);
  // Writes the transform if it is not implicit nor all zero.
  static void WriteTransform(const CodedBlock& cb, Channel channel,
                             SymbolManager* sm, ANSEncBase* enc);

  // Writes all blocks to 'enc'. 'size_order_indices' should be the same size
  // as 'cblocks', and contain the indices of a permutation of 'cblocks' that
  // puts it in size order (order in which block sizes are written).
  WP2Status WriteBlocks(const Vector<CodedBlock>& cblocks,
                        const Vector_u16& size_order_indices,
                        FrontMgrDoubleOrderBase* mgr, ANSEnc* enc);
  WP2Status WriteBlocksUseSplits(const Vector<CodedBlock>& cblocks,
                                 const Vector_u32& splits, ANSEnc* enc);

  SymbolWriter* symbol_writer() { return &symbol_writer_; }
  const SymbolRecorder& symbol_recorder() const { return symbol_recorder_; }
  Counters* counters() const { return &counters_; }
  const BlockContext& context() const { return context_; }

  // Finds the best encoding method for the given coeffs
  void FindBestEncodingMethods(CodedBlock* cb);

  // Decide alpha mode and encoding params if needed
  WP2Status DecideAlpha(CodedBlock* cb, const BlockModes& modes,
                        BlockScorer* scorer);
  WP2Status RecordAlpha(const CodedBlock& cb);

  // Fills 'segment_ids_' with default values taken from 'gparams_'.
  WP2Status SetInitialSegmentIds();

  ChromaSubsampling chroma_subsampling() const { return chroma_subsampling_; }

  // Final writing of the block, except for its size.
  WP2Status WriteBlock(const CodedBlock& cb, uint32_t block_index, ANSEnc* enc);

 private:
  // Resets all records, called by InitPass.
  WP2Status ResetRecord();

  void RecordBlockHeader(const CodedBlock& cb);

  const EncoderConfig* config_;
  ANSDictionaries* dicts_;
  AlphaWriter alpha_writer_;

  // Actual number of blocks and transforms if known or upper bound.
  uint32_t num_blocks_, num_transforms_;
  // Number of times Record() has been called.
  uint32_t recorded_blocks_;
  // Pass number.
  uint32_t pass_number_ = 0u;

  Rectangle tile_rect_;  // In pixels, not padded.

  ChromaSubsampling chroma_subsampling_;
  bool use_splits_;

  const GlobalParams* gparams_;

  BlockContext context_;

  SymbolWriter symbol_writer_;
  SymbolRecorder symbol_recorder_;
  mutable Counters counters_;  // temporary data made to be shared
  ResidualWriter residual_writer_;

  // Debugging.
  // Set to true to print debug information for checking accuracy of estimated
  // rates (bit costs).
  static const bool kDebugPrintRate = false;
  // For each block, estimated residual rate per channel followed by actual rate
  // per channel.
  Vector<std::array<float, 8>> residual_rate_;
  // Encodes the 'block' boundaries and the 'pixels'.
  void PutRawPixels(const CodedBlockBase& cb, const YUVPlane& pixels,
                    ANSEnc* enc);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_WP2_ENC_I_H_ */
