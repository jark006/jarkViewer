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
// Frame header (ANMF chunk) decoding and verification.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_INCR_DECODER_INFO_H_
#define WP2_DEC_INCR_DECODER_INFO_H_

#include <cstddef>

#include "src/dec/incr/decoder_state.h"
#include "src/utils/data_source.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"

namespace WP2 {

//------------------------------------------------------------------------------

// If fully available, decodes the ANMF chunk, verifies its integrity and stores
// the new frame features. 'glimpse' implies using the glimpsed frame index
// instead of the current frame index.
WP2Status DecodeFrameInfo(const DecoderConfig& config, DataSource* data_source,
                          const BitstreamFeatures& features, bool glimpse,
                          Decoder::State* state);

// Checks that the position is valid and stores it as 'frame->num_bytes'. To be
// called for each 'frame' as soon as the 'bitstream_position_at_end_of_frame'
// is known.
WP2Status SaveFrameNumBytes(size_t bitstream_position_at_end_of_frame,
                            Decoder::State::InternalFrameFeatures* frame);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_DEC_INCR_DECODER_INFO_H_ */
