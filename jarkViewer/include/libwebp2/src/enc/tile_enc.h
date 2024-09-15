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
// Tile encoding
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_ENC_TILE_ENC_H_
#define WP2_ENC_TILE_ENC_H_

#include <cstdint>

#include "src/common/global_params.h"
#include "src/common/lossy/block_size.h"
#include "src/common/progress_watcher.h"
#include "src/utils/csp.h"
#include "src/utils/plane.h"
#include "src/utils/thread_utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/debug.h"
#include "src/wp2/encode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Compresses an image into tiles based on 'config' and writes them to 'output'.
// The input is either 'rgb_buffer' or 'yuv_buffer' depending on what is needed.
WP2Status EncodeTiles(uint32_t width, uint32_t height,
                      const ArgbBuffer& rgb_buffer, const YUVPlane& yuv_buffer,
                      const CSPTransform& transf, const EncoderConfig& config,
                      bool image_has_alpha, bool image_is_premultiplied,
                      const ProgressRange& progress, Writer* output);

//------------------------------------------------------------------------------

struct EncTile {
  // Tile position and dimensions within the frame in pixels, not padded.
  Rectangle rect;

  // Views of this tile.
  ArgbBuffer rgb_input;  // Not padded.
  YUVPlane yuv_input;    // Padded.

  // Output. AssembleToBitstream()+WriteBitstreamTo() to get the encoded bytes.
  ANSEnc enc;
  Data data;  // Output when ANSEnc is not used (for Av1Encode() only).
  ProgressRange progress;
};

struct EncTilesLayout {
  uint32_t num_tiles_x = 0;
  uint32_t num_tiles_y = 0;
  uint32_t tile_width = 0;
  uint32_t tile_height = 0;
  Vector<EncTile> tiles;
  uint32_t first_unassigned_tile_index = 0;  // Next tile to assign a worker to.
  ThreadLock assignment_lock;                // In case of concurrency.

  // Frame's global parameters (externally owned).
  const GlobalParams* gparams = nullptr;
  bool image_is_premultiplied = false;
};

class TileEncoder : public WorkerBase {
 public:
  // Assigns the next 'tile_' to this worker if possible.
  WP2Status AssignNextTile();
  // As long as 'tile_' is not null, decode it and AssignNextTile().
  WP2Status Execute() override;

  // Compresses the tile into 'enc'.
  WP2Status LossyEncode(const VectorNoCtor<Block>& forced_partition,
                        ANSEnc* enc);
  WP2Status LosslessEncode(ANSEnc* enc);
  WP2Status Av1Encode(Data* enc);

  const EncoderConfig* config_ = nullptr;
  bool use_lossless_ = false;
  EncTilesLayout* tiles_layout_ = nullptr;  // Used to get next assignable tile.
  EncTile* tile_ = nullptr;                 // Currently assigned tile.

  // Debug output.
  EncoderInfo* info_ = nullptr;
};

// Encodes pixels as row-ordered raw samples.
WP2Status BypassTileEnc(const GlobalParams& gparams, EncTile* tile,
                        Writer* output);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_ENC_TILE_ENC_H_ */
