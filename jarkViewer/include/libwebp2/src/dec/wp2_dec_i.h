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
// WP2 decoder: internal header.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_DEC_WP2_DEC_I_H_
#define WP2_DEC_WP2_DEC_I_H_

#include <cstdint>
#include <memory>

#include "src/common/global_params.h"
#include "src/common/lossy/block.h"
#include "src/common/lossy/block_size.h"
#include "src/common/lossy/context.h"
#include "src/common/lossy/residuals.h"
#include "src/common/lossy/rnd_mtx.h"
#include "src/common/symbols.h"
#include "src/dec/lossless/losslessi_dec.h"
#include "src/dec/symbols_dec.h"
#include "src/dec/tile_dec.h"
#include "src/utils/ans.h"
#include "src/utils/csp.h"
#include "src/utils/data_source.h"
#include "src/utils/front_mgr.h"
#include "src/utils/plane.h"
#include "src/utils/split_iterator.h"
#include "src/utils/utils.h"
#include "src/wp2/base.h"
#include "src/wp2/debug.h"
#include "src/wp2/decode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------
// Decoding functions.
// "dec" is the input (ANS decoder), with "features" and "config".
// "picture" is the output (buffer view of the tile to decode).
// "num_decoded_rows" is for incremental decoding; kept up-to-date.
//   A row must be left intact once it is included in num_decoded_rows.

WP2Status LossyDecode(const BitstreamFeatures& features,
                      const DecoderConfig& config, TilesLayout* tiles_layout,
                      ANSDec* dec, Tile* tile);

WP2Status LosslessDecode(const BitstreamFeatures& features,
                         const DecoderConfig& config,
                         const GlobalParams& gparams, ANSDec* dec, Tile* tile);

WP2Status NeuralDecode(ArgbBuffer* picture, ANSDec* dec,
                       const BitstreamFeatures& features);

WP2Status Av1Decode(const BitstreamFeatures& features,
                    const DecoderConfig& config, const DataView& dec,
                    Tile* tile);

//------------------------------------------------------------------------------

// These internal methods are to avoid polluting public decode.h
void ResetDecoderInfo(DecoderInfo* info);
WP2Status MergeDecoderInfo(DecoderInfo* from, DecoderInfo* to);

//------------------------------------------------------------------------------
// Internal decode.

// Output of an ANMF chunk header decoding.
struct AnimationFrame {
  bool dispose = true;       // Fill the whole canvas with the background color
                             // before decoding this frame.
                             // Leave the canvas as is if false.
  bool blend = false;        // Blend frame into current canvas, based on alpha.
                             // Overwrite pixels (including alpha) if false.
  uint32_t duration_ms = 0;  // Milliseconds during which this frame is visible.
                             // 0 means a preframe.
  Rectangle window;          // Unoriented pixel boundaries of this frame.
  bool is_last = false;      // If true, there's no frame after this one.
};

// Initialize and register debug data.
WP2Status SetupDecoderInfo(const BitstreamFeatures& features,
                           const DecoderConfig& config);
WP2Status RegisterBitTrace(const DecoderConfig& config, uint32_t num_bytes,
                           WP2_OPT_LABEL);

// Fills DecoderInfo in 'config' based on provided GlobalParams.
WP2Status FillDecoderInfo(const GlobalParams& gparams,
                          const DecoderConfig& config);

// Returns the format closest to the one requested by the user and matching how
// the samples were compressed.
WP2SampleFormat ChooseRawFormat(const BitstreamFeatures& features,
                                WP2SampleFormat format_requested_by_user,
                                bool support_10b);

// Decode steps are split into functions for incremental decoding. These
// functions only call DataSource::MarkNumBytesAsRead(). This way the calling
// function can decide whether to retry later (UnmarkAllReadBytes()) or not
// (Discard(GetNumNextBytes())).
WP2Status DecodeHeader(DataSource* data_source, BitstreamFeatures* features);

WP2Status DecodePreview(DataSource* data_source,
                        const BitstreamFeatures& features,
                        ArgbBuffer* output_buffer);
WP2Status SkipPreview(DataSource* data_source,
                      const BitstreamFeatures& features);

WP2Status DecodeICC(DataSource* data_source, const BitstreamFeatures& features,
                    Data* iccp);

WP2Status DecodeANMF(const DecoderConfig& config, DataSource* data_source,
                     const BitstreamFeatures& features, uint32_t frame_index,
                     AnimationFrame* frame);

// if gparams = nullptr, just skip over the data without parsing
WP2Status DecodeGLBL(DataSource* data_source, const DecoderConfig& config,
                     const BitstreamFeatures& features,
                     GlobalParams* gparams = nullptr);

// Fills the area outside 'frame.window' with the 'features.background_color'.
WP2Status FillBorders(const BitstreamFeatures& features,
                      const AnimationFrame& frame, ArgbBuffer* output);
WP2Status FillBorders(const BitstreamFeatures& features,
                      const AnimationFrame& frame,
                      const CSPTransform& csp_transform, YUVPlane* output);

WP2Status DecodeTileChunkSize(uint32_t rgb_bit_depth,
                              const GlobalParams& params,
                              const Rectangle& tile_rect,
                              DataSource* data_source,
                              uint32_t* tile_chunk_size);

WP2Status DecodeMetadata(DataSource* data_source,
                         const BitstreamFeatures& features, Data* xmp,
                         Data* exif);

// These two Skip() functions do not attempt to read anything past the chunk
// size; if data length check through reading is necessary but data copy should
// be avoided, call the Decode() functions instead with null Data pointers.
WP2Status SkipICC(DataSource* data_source, const BitstreamFeatures& features);
WP2Status SkipMetadata(DataSource* data_source,
                       const BitstreamFeatures& features);

//------------------------------------------------------------------------------

// Class specialized in reading transform residuals.
class ResidualReader : public ResidualIO {
 public:
  ResidualReader() = default;
  // Updates the symbol reader with what will be needed to read residuals.
  WP2Status ReadHeader(SymbolReader* sr, uint32_t num_coeffs_max_y,
                       uint32_t num_coeffs_max_uv, uint32_t num_transforms,
                       bool has_alpha, bool has_lossy_alpha);

  // Reads residual coefficients.
  WP2Status ReadCoeffs(Channel channel, const int16_t dequant[],
                       SymbolReader* sr, CodedBlockBase* cb, BlockInfo* info);

 private:
  WP2Status ReadHeaderForResidualSymbols(uint32_t num_coeffs_max,
                                         Channel channel, SymbolReader* sr);
  uint32_t ReadCoeffsMethod01(Channel channel, const int16_t dequant[],
                              uint32_t tf_i, SymbolReader* sr,
                              CodedBlockBase* cb, BlockInfo* info);

  uint32_t num_channels_;
};

//------------------------------------------------------------------------------
// AlphaReader: Class for reading the alpha plane.

class AlphaReader : public WP2Allocable {
 public:
  // Image width and height are in pixels.
  explicit AlphaReader(ANSDec* dec, const Tile& tile);
  WP2Status Allocate();

  WP2Status ReadHeader(const GlobalParams& gparams);

  // Reads alpha information for the given block from the stream.
  void GetBlockHeader(SymbolReader* sr, CodedBlockBase* cb);
  // Reads alpha information for the given block from the stream.
  WP2Status GetBlock(CodedBlockBase* cb);
  // Reconstructs the alpha plane for the given block.
  void Reconstruct(CodedBlockBase* cb) const;
  // Copies the lossless alpha data for this block to the given plane (at 0, 0).
  void ReconstructLossless(const CodedBlockBase& cb, Plane16* plane) const;

  AlphaMode GetAlphaMode() const { return alpha_mode_; }

 private:
  WP2Status ReadLossless(CodedBlockBase* cb);

  const GlobalParams* gparams_ = nullptr;
  // ANS reader
  ANSDec* const dec_;
  // Alpha buffer. Alpha values are stored as a grayscale image.
  // TODO(maryla): this is pretty memory inefficient: in reality we don't need
  // four channels, and we don't need to keep the whole alpha image in memory
  // at once (we never need more than the max height of a block).
  Tile alpha_tile_;
  // Whether alpha is encoded with pure lossy, pure lossless, or a mix.
  AlphaMode alpha_mode_;
  // Next line to read in pixels.
  uint32_t next_line_ = 0;

  // Block alpha mode predictor when using lossy.
  AlphaModePredictor mode_predictor_;

  // *** Members variables for lossless decoding. ***
  WP2L::Decoder lossless_decoder_;
};

//------------------------------------------------------------------------------
// SyntaxReader: Class for parsing syntactic elements.

class SyntaxReader {
 public:
  SyntaxReader(ANSDec* dec, const Rectangle& rect);

  WP2Status ReadHeader(const BitstreamFeatures& features,
                       const GlobalParams& gparams, const Tile& tile);

  // Reads 'cb' size, segment id, coeffs etc. and sets 'out' as the
  // reconstructed pixels buffer destination.
  WP2Status GetBlock(CodedBlockBase* cb, YUVPlane* out, BlockInfo* info);
  BlockSize GetBlockSize(const FrontMgrDoubleOrderBase& mgr);
  WP2Status GetBlockSplit(const SplitIteratorBase& it, uint32_t* split_idx);

  PartitionSet partition_set() const { return gparams_->partition_set_; }
  bool partition_snapping() const { return gparams_->partition_snapping_; }

  const AlphaReader& alpha_reader() const { return *alpha_reader_; }
  bool use_splits() const { return use_splits_; }
  bool use_aom_coeffs() const { return residual_reader_.use_aom(); }

  void ApplyDecoderConfig(const DecoderConfig& config);

 private:
  void ReadSplitTransform(Channel channel, CodedBlockBase* cb);
  void ReadHasCoeffs(Channel channel, CodedBlockBase* cb);
  void ReadTransform(Channel channel, CodedBlockBase* cb);

  // Fill the is420_ data in the block.
  void ReadIs420(CodedBlockBase* cb);

  WP2Status LoadDictionaries();

  WP2Status ReadPredModes(CodedBlockBase* cb);

  const GlobalParams* gparams_ = nullptr;

  ANSDec* const dec_;

  uint32_t num_blocks_, num_transforms_;
  ChromaSubsampling chroma_subsampling_;
  bool use_splits_;

  SegmentIdPredictor segment_ids_;

  ResidualReader residual_reader_;

  std::unique_ptr<AlphaReader> alpha_reader_;

  SymbolsInfo symbols_info_;
  SymbolReader sr_;
  uint32_t width_;
  uint32_t height_;

 public:
  // Visual debug: stores bit cost for a block
  void StoreBitCost(const WP2::DecoderConfig& config, uint32_t tile_x,
                    uint32_t tile_y, const Block& block,
                    Plane16* dst_plane) const;
  // Decodes the 'block' boundaries and the 'pixels'. Check that they match.
  void ReadAndCompareRawPixels(const CodedBlockBase& cb, const YUVPlane& pixels,
                               ANSDec* dec);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_WP2_DEC_I_H_
