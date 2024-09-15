// Copyright 2020 Google LLC
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
//  Screen content encoding (e.g. screenshots)
//
// Author: Maryla (maryla@google.com)

#ifndef WP2_ENC_ANIM_SCREEN_ENC_H_
#define WP2_ENC_ANIM_SCREEN_ENC_H_

#include "src/common/global_params.h"
#include "src/enc/anim/anim_enc.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/encode.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Encodes screen content as a mix of lossy and lossless, using a 0 second
// animation with 2 frames. SLOW!
WP2Status EncodeScreenContent(
    const ArgbBuffer& input, Writer* const output,
    const EncoderConfig& config = EncoderConfig::kDefault);

// Visible for testing.
WP2Status EncodeScreenContent(
    const ArgbBuffer& input, const Plane<bool>& is_lossy, uint32_t block_size,
    Writer* const output,
    const EncoderConfig& config = EncoderConfig::kDefault);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_ANIM_SCREEN_ENC_H_
