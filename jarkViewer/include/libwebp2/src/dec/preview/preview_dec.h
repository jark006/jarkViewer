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
//  Preview decoder
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_PREVIEW_PREVIEW_DEC_H_
#define WP2_DEC_PREVIEW_PREVIEW_DEC_H_

#include <cstdint>

#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Decompresses the preview from 'data' and draws it to the 'output_buffer'
// which must be previously allocated to the desired dimensions.
WP2Status DecodePreview(const uint8_t* data, uint32_t data_size,
                        ArgbBuffer* output_buffer);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_PREVIEW_PREVIEW_DEC_H_
