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
// PartialTileDecoder interface and State struct from Decoder class.
//
// "Preframes" are frames with a duration of 0 ms. Only "final frames" are known
// to the user, being "regular frames" merged with their respective "preframes".
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_INCR_DECODER_STATE_H_
#define WP2_DEC_INCR_DECODER_STATE_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "src/common/progress_watcher.h"
#include "src/dec/filters/intertile_filter.h"
#include "src/dec/tile_dec.h"
#include "src/dec/wp2_dec_i.h"
#include "src/utils/data_source.h"
#include "src/utils/data_source_stream.h"
#include "src/utils/plane.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Interface for decoding a tile with partial data.
class PartialTileDecoder : public WP2Allocable {
 public:
  PartialTileDecoder() = default;
  virtual ~PartialTileDecoder() = default;

  // Initializes the worker.
  virtual WP2Status Init(const BitstreamFeatures& features,
                         const DecoderConfig& config, TilesLayout* tiles_layout,
                         Tile* partial_tile) = 0;
  // Starts work. Returns WP2_STATUS_OK or an error.
  virtual WP2Status Start() = 0;
  // Continues work. Returns WP2_STATUS_OK or an error.
  virtual WP2Status Continue() = 0;
  // Abandons the decoding process, if any.
  virtual void Clear() = 0;
  // Returns nullptr if there is no assigned partial tile.
  virtual Tile* GetPartialTile() const = 0;
  // Returns true if the tile is complete.
  virtual bool IsComplete() const = 0;
};

//------------------------------------------------------------------------------

// Pointer to implementation for Decoder state.
struct Decoder::State : public WP2Allocable {
 protected:
  State(const DecoderConfig& config, DataSource* const input)
      : data_source(input), progress(config.progress_hook) {}

 public:
  virtual ~State();

  static constexpr size_t kUnknown = 0;

  // What data or action is expected next. The order matters.
  enum Stage {
    WAITING_FOR_HEADER_CHUNK,
    ALLOCATE_OUTPUT_BUFFER,
    WAITING_FOR_PREVIEW_CHUNK,
    WAITING_FOR_ICC_CHUNK,
    WAITING_FOR_ANMF_CHUNK,
    PREPARE_TILES,
    WAITING_FOR_TILE_DATA,
    WAITING_FOR_METADATA_CHUNKS,
    FINISH_PROGRESS,
    DECODED
  } stage = WAITING_FOR_HEADER_CHUNK;
  // Store last success, pending or error status.
  WP2Status status = WP2_STATUS_NOT_ENOUGH_DATA;

  // The current preframe or regular frame is the frame to decode.
  uint32_t current_frame_index = 0;
  bool current_frame_is_ready = false;  // Current frame is fully decoded.

  // A glimpsed frame is only parsed for its ANMF chunk, in order to know if the
  // current frame should be discarded or not.
  uint32_t glimpsed_frame_index = 0;
  bool glimpsed_frame_anmf = false;  // Is glimpsed frame's header decoded.
  bool glimpsed_frame_glbl = false;
  GlobalParams glimpsed_frame_gparams;  // Partially valid (type, transf, alpha)
  uint32_t num_glimpsed_tiles = 0;      // Number of skipped tiles.
  size_t glimpsed_bitstream_position = 0;

  // Internal information about preframes and regular frames.
  // FrameFeatures are accessible through Decoder::TryGetFrameDecodedFeatures().
  struct InternalFrameFeatures : public FrameFeatures {
    size_t bitstream_position = kUnknown;  // ANMF chunk's first byte.
    size_t num_bytes = kUnknown;           // Includes this frame's ANMF chunk.
    AnimationFrame info;  // ANMF info of this preframe or regular frame.
    uint32_t final_frame_index = 0;
    bool operator==(const InternalFrameFeatures& other) const;
    bool operator!=(const InternalFrameFeatures& other) const;
  };

  // Known frame info.
  Vector<InternalFrameFeatures> frames;
  // Maps to the regular frame belonging to the final frame.
  Vector<uint32_t> final_to_regular_index;

  GlobalParams gparams;
  TilesLayout tiles_layout;  // Known once features are decoded.

  IntertileFilter intertile_filter;       // Smooths the tile edges out.
  uint32_t frame_num_converted_rows = 0;  // Total number of RGB pixel rows in
                                          // the current frame window.
  uint32_t frame_num_final_rows = 0;  // Total number of final pixel rows in the
                                      // current frame window, processed by the
                                      // IntertileFilter.

  Vector<TileDecoder> workers;        // Max one per tile.
  DataSource* data_source = nullptr;  // Main input, buffer can change.

  // Used to decode parts of a tile, if not null.
  std::unique_ptr<PartialTileDecoder> partial_tile_decoder;

  // Temp buffer to store the non-oriented Argb32 output.
  ArgbBuffer raw_output_buffer;
  // Whether we must use a temporary buffer for this whole animation
  // (because of rotation, slow buffer, or format difference).
  bool must_use_raw_output_buffer = true;
  // Whether we are currently using a temporary buffer, either because
  // must_use_raw_output_buffer is true or because the current frame
  // has the blend option. If both must_use_raw_output_buffer and blend are
  // true, an extra 'blended_buffer' is also used.
  bool using_raw_output_buffer = false;
  // View of the output buffer, cropped to fit the current frame.
  ArgbBuffer frame_output_buffer;
  // Temporary buffer used for blending when raw_output_buffer is already used.
  ArgbBuffer blended_buffer;
  bool using_blended_buffer = false;

  // YUV output buffer that will be sliced into one view per tile.
  // Fits the padded current frame ('frame_output_buffer').
  YUVPlane yuv_output_buffer;

  // Skipped frames won't be notified with FrameIsReady(), and may be discarded.
  // Frame features (ANMF chunks) will still be decoded, unless already stored.
  uint32_t skip_final_frames_before_index = 0;
  // Discarded frames will not have their (all or remaining) pixels decoded.
  // A skipped frame is discarded if not needed for the next non-skipped frame.
  uint32_t discard_frames_before_index = 0;

  // Decoding advancement.
  ProgressWatcher progress;  // Can generate WP2_STATUS_USER_ABORT.
  ProgressRange current_frame_progress, current_tiles_progress;

  // Moves to next stage if there is no error, otherwise returns false.
  bool NextStage();

  // Conversion between final frame index and frame (preframe or regular) index.
  uint32_t GetFinalFrameIndex(uint32_t frame_index) const;
  bool TryGetFirstFrameBelongingToFinalFrame(uint32_t final_frame_index,
                                             uint32_t* frame_index) const;

  // Updates 'discard_frames_before_index' based on
  // 'skip_final_frames_before_index' and known 'frames'.
  WP2Status ComputeNumFramesToDiscard();

  // If this returns true once, it will do so until Decoder::Rewind().
  bool ShouldDiscardAllFrames() const;

  // Returns the total number of decoded pixel rows in the current frame window,
  // ready to be processed by the IntertileFilter.
  uint32_t GetFrameNumDecodedRows() const;

  // Resets the state to the beginning of the bitstream. Previously decoded
  // features are remembered.
  virtual void Rewind();
};

struct DecoderStateArray : public Decoder::State {
 public:
  explicit DecoderStateArray(const DecoderConfig& config)
      : Decoder::State(config, &array_data_source) {}

  // Rewinds also the 'array_data_source' so no need to call SetInput() again.
  void Rewind() override;

  ExternalDataSource array_data_source;
  DataView last_input_set = {nullptr, 0};
};

struct DecoderStateStream : public Decoder::State {
 public:
  explicit DecoderStateStream(const DecoderConfig& config)
      : Decoder::State(config, &stream_data_source) {}
  StreamDataSource stream_data_source;
};

//------------------------------------------------------------------------------

// Used exclusively with CustomDecoder.
class CustomDataSource : public DataSource {
 public:
  explicit CustomDataSource(CustomDecoder* const fetcher) : fetcher_(fetcher) {}
  WP2Status ForceFetch();
  void Reset() override;

 private:
  bool Fetch(size_t num_requested_bytes) override;
  void OnDiscard(size_t num_bytes) override;
  CustomDecoder* const fetcher_;
};

struct DecoderStateCustom : public Decoder::State {
 public:
  DecoderStateCustom(const DecoderConfig& config,
                     CustomDecoder* const custom_decoder)
      : Decoder::State(config, &custom_data_source),
        custom_data_source(custom_decoder) {}
  CustomDataSource custom_data_source;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_DEC_INCR_DECODER_STATE_H_ */
