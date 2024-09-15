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
// ContextTileDecoder definition.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_INCR_DECODER_CONTEXT_H_
#define WP2_DEC_INCR_DECODER_CONTEXT_H_

#include "src/dec/incr/decoder_state.h"
#include "src/dec/tile_dec.h"
#include "src/utils/context_switch.h"
#include "src/utils/data_source_context.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"

#if defined(WP2_USE_CONTEXT_SWITCH) && (WP2_USE_CONTEXT_SWITCH > 0)

namespace WP2 {

// Uses a MainContext and a suspendable LocalContext to decode partial tiles.
class ContextTileDecoder : public PartialTileDecoder {
 public:
  WP2Status Init(const BitstreamFeatures& features, const DecoderConfig& config,
                 TilesLayout* tiles_layout, Tile* partial_tile) override;
  WP2Status Start() override;
  WP2Status Continue() override;
  void Clear() override;
  Tile* GetPartialTile() const override;
  bool IsComplete() const override;

 protected:
  // To be called only through MainContext::CreateLocalContext().
  static void SuspendingDecode(void* local_context);

  // Resumes decoding and sets partial_tile_.status.
  void DecodePartialTile();

  // Separate struct to be accessed in SuspendingDecode().
  struct InterContextData {
    TileDecoder worker;
    SuspendableDataSource input;
    WP2Status status = WP2_STATUS_OK;
  };

  Tile* partial_tile_ = nullptr;
  InterContextData inter_context_data_;
  MainContext main_context_;  // Used to go back into decoding the partial tile.
                              // During dtor, must be destroyed first.
};

}  // namespace WP2

#endif  // WP2_USE_CONTEXT_SWITCH

#endif /* WP2_DEC_INCR_DECODER_CONTEXT_H_ */
