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
// Tile-related structs.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_DEC_TILE_DEC_H_
#define WP2_DEC_TILE_DEC_H_

#include <cstdint>

#include "src/common/progress_watcher.h"
#include "src/dec/filters/block_map_filter.h"
#include "src/utils/data_source.h"
#include "src/utils/plane.h"
#include "src/utils/thread_utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"

namespace WP2 {

class GlobalParams;

//------------------------------------------------------------------------------

struct Tile {
  bool chunk_size_is_known = false;    // True when the chunk size is decoded.
  uint32_t chunk_size = 0;             // Value is valid if chunk_size_is_known.
  DataSource* input = nullptr;         // Input data. Points to private_input
  ExternalDataSource private_input;    // if the whole chunk is available.
  DataSource::DataHandle data_handle;  // Keep the chunks of several tiles from
                                       // the data source without copy.

  Rectangle rect;                 // Tile bounds within frame (px, not padded).
  bool output_is_yuv = false;     // True if 'yuv_output' contains the pixels,
                                  // false if 'rgb_output' does.
  YUVPlane yuv_output;            // Lossy output buffer view (padded).
  ArgbBuffer rgb_output;          // Lossless output buffer view (not padded).
  uint32_t num_decoded_rows = 0;  // For incremental decoding; the number of
                                  // already decoded rows in this tile.
  FilterBlockMap block_map;  // Info (segment, bpp etc.) per block. Lossy only.
  ProgressScale row_progress;  // Advance by 1 for each decoded row.

  void Clear();
  void Draw(const WP2::DecoderConfig& config);  // For debugging.
};

//------------------------------------------------------------------------------

struct TilesLayout {
  uint32_t num_tiles_x = 0;
  uint32_t num_tiles_y = 0;
  uint32_t tile_width = 0;  // Pixels, not padded.
  uint32_t tile_height = 0;
  Vector<Tile> tiles;
  uint32_t num_assignable_tiles = 0;  // Number of tiles with full input data.
  uint32_t first_unassigned_tile_index = 0;  // Next tile to assign a worker to.
  uint32_t num_decoded_tiles = 0;            // Total number of decoded tiles.
  ThreadLock assignment_lock;                // In case of concurrency.

  // Returns the tile containing the pixel located at (x,y).
  const Tile& GetTileAt(uint32_t x, uint32_t y) const;

  // frame's global parameters (externally owned)
  const GlobalParams* gparams = nullptr;
};

// Returns the number of tiles (per axis too if not null) depending on the
// 'frame_width' and 'frame_height' (in pixels, not padded).
uint32_t GetNumTiles(uint32_t frame_width, uint32_t frame_height,
                     uint32_t tile_width, uint32_t tile_height,
                     uint32_t* num_tiles_x = nullptr,
                     uint32_t* num_tiles_y = nullptr);

// Returns the tile area within the frame (in pixels, not padded).
// The specified 'tile_index' is ordered by rows, top-left being 0.
Rectangle GetTileRect(uint32_t frame_width, uint32_t frame_height,
                      uint32_t tile_width, uint32_t tile_height,
                      uint32_t tile_index);

// Assigns the 'tiles_layout' with the buffers split into a view per tile.
// 'rgb_output' and/or 'yuv_output' can be empty.
WP2Status GetTilesLayout(uint32_t frame_width, uint32_t frame_height,
                         uint32_t tile_width, uint32_t tile_height,
                         const ProgressRange& progress, ArgbBuffer* rgb_output,
                         YUVPlane* yuv_output, TilesLayout* tiles_layout);

// Returns the maximum number of bytes that the chunk of a tile can occupy
// (without the chunk size header). 'tile_rect' in pixels, not padded.
uint32_t GetTileMaxNumBytes(uint32_t rgb_bit_depth, const GlobalParams& gparams,
                            const Rectangle& tile_rect);

//------------------------------------------------------------------------------

class TileDecoder : public WorkerBase {
 public:
  TileDecoder() = default;
  TileDecoder(TileDecoder&&) = default;

  // If this is 'self_assigning_' and there are assignable tiles left, assigns
  // the next 'tile_' to this worker. Otherwise, set 'tile_' to null.
  WP2Status TryAssignNextTile();
  // As long as 'tile_' is not null, decode it and TryAssignNextTile().
  WP2Status Execute() override;

  // Decodes pixels as row-ordered raw samples.
  WP2Status BypassTileDec();

 protected:
  // Decodes pixels as row-ordered raw samples.
  WP2Status BypassTileDecRgb8b();
  WP2Status BypassTileDecRgb10b();
  WP2Status BypassTileDecYuv();

 public:
  const BitstreamFeatures* features_ = nullptr;
  const DecoderConfig* config_ = nullptr;
  const GlobalParams* gparams_ = nullptr;
  TilesLayout* tiles_layout_ = nullptr;  // Used to get next assignable tile.
  Tile* tile_ = nullptr;                 // Currently assigned tile.
  bool self_assigning_ = true;  // If true tries to assign a new tile when done.
};

//------------------------------------------------------------------------------

// Instanciates and sets up a number of 'workers' depending on the number of
// tiles and threads.
WP2Status SetupWorkers(const BitstreamFeatures& features,
                       const DecoderConfig& config, TilesLayout* tiles_layout,
                       Vector<TileDecoder>* workers);

// Skips global params and then calls SkipTiles.
WP2Status SkipGlobalParamsAndTiles(DataSource* data_source,
                                   const BitstreamFeatures& features,
                                   const Rectangle& window);
// Skips all tiles fitting in 'window' from the 'data_source'.
WP2Status SkipTiles(const GlobalParams& gparams, DataSource* data_source,
                    const BitstreamFeatures& features, const Rectangle& window);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_TILE_DEC_H_
