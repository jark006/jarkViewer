// Copyright 2021 Google LLC
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
//   Everything related to visual debug
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef THIRD_PARTY_LIBWEBP_WP2_PUBLIC_SRC_COMMON_VDEBUG_H_
#define THIRD_PARTY_LIBWEBP_WP2_PUBLIC_SRC_COMMON_VDEBUG_H_

#include <cassert>
#include <string>

#include "src/utils/csp.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

struct DecoderConfig;
struct EncoderConfig;
struct Tile;

#if !defined(WP2_REDUCE_BINARY_SIZE)

// Returns true if 'visual_debug' in the 'config.info' exists and
// contains the 'token'. Tokens are separated by '/'.
template <typename TConfig>
bool VDMatch(const TConfig& in, const char token[]) {
  if (in.info != nullptr) {
    return VDMatch(in.info->visual_debug, token);
  }
  return false;
}
template <>
bool VDMatch<const char*>(const char* const& in, const char token[]);
template <>
bool VDMatch<std::string>(const std::string& in, const char token[]);

// Returns the channel corresponding to the current debug view. Asserts that
// the debug view contains the name of a channel ("y", "u", "v" or "a") in its
// path.
template <typename TConfig>
Channel VDChannel(const TConfig& config) {
  const std::string& visual_debug = config.info->visual_debug;
  if (VDMatch(visual_debug, "y")) return kYChannel;
  if (VDMatch(visual_debug, "u")) return kUChannel;
  if (VDMatch(visual_debug, "v")) return kVChannel;
  assert(VDMatch(visual_debug, "a"));
  return kAChannel;
}

// Returns true if the point 'config.info->selection' is in 'rect'.
template <typename TConfig>
static bool VDSelected(const Rectangle& rect, const TConfig& config) {
  return (config.info != nullptr && config.info->selection.width > 0 &&
          config.info->selection.height > 0 &&
          config.info->selection.x >= rect.x &&
          config.info->selection.x < rect.x + rect.width &&
          config.info->selection.y >= rect.y &&
          config.info->selection.y < rect.y + rect.height);
}

// Returns true if the given rectangle is selected in visual debug.
template <typename TConfig>
bool VDSelected(uint32_t tile_x, uint32_t tile_y, const Rectangle& rect,
                const TConfig& config) {
  return VDSelected({tile_x + rect.x, tile_y + rect.y, rect.width, rect.height},
                    config);
}

// Handles VisualDebug for "compressed" and "original".
void ApplyVDebugBeforeAfter(const DecoderConfig& config,
                            const CSPTransform& csp_transform, const Tile& tile,
                            ArgbBuffer* const debug_output);

void VDDrawUndefinedPattern(ArgbBuffer* const debug_output);

// Returns the parameter if 'config->info.visual_debug' matches "p="+parameter
// or 'default_value' otherwise.
float VDGetParam(const EncoderConfig& config, float default_value);

// Prints a string to top-left position (x,y). '\n' are taken into account
void Print(const std::string& msg, int x, int y,
           Argb32b color, ArgbBuffer* const out);

#else

template <typename TConfig>
inline bool VDMatch(const TConfig&, const char*) {
  return false;
}
template <typename TConfig>
inline Channel VDChannel(const TConfig&) {
  return kYChannel;
}
template <typename TConfig>
inline bool VDSelected(uint32_t, uint32_t, const Rectangle&, const TConfig&) {
  return false;
}
inline float VDGetParam(const EncoderConfig&, float default_value) {
  return default_value;
}

#endif  // WP2_REDUCE_BINARY_SIZE

}  // namespace WP2

#endif  // THIRD_PARTY_LIBWEBP_WP2_PUBLIC_SRC_COMMON_VDEBUG_H_
