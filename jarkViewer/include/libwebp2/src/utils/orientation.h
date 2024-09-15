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
//  Orientation, rotation functions
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_UTILS_ORIENTATION_H_
#define WP2_UTILS_ORIENTATION_H_

#include <cstdint>

#include "src/utils/plane.h"
#include "src/wp2/base.h"

namespace WP2 {

// Rotations are clockwise and with the origin (0, 0) at the top left corner.

// Applies the 'orientation' on 'width, height' and returns the updated size.
uint32_t OrientateWidth(Orientation orientation, uint32_t width,
                        uint32_t height);
uint32_t OrientateHeight(Orientation orientation, uint32_t width,
                         uint32_t height);

// All coordinates stay positive and must be below 'old_width', 'old_height'.
uint32_t OrientatePointX(Orientation orientation, uint32_t old_width,
                         uint32_t old_height, uint32_t x, uint32_t y);
uint32_t OrientatePointY(Orientation orientation, uint32_t old_width,
                         uint32_t old_height, uint32_t x, uint32_t y);

// Keeps x,y as top-left.
Rectangle OrientateRectangle(Orientation orientation, uint32_t old_width,
                             uint32_t old_height, const Rectangle& rect);

// Orientates all pixels of 'buffer'.
WP2Status OrientateBuffer(Orientation orientation, ArgbBuffer* buffer);

// Orientates all pixels from 'src' to 'dst'. The latter is resized to fit the
// entire oriented image.
// Handles conversion from WP2_Argb_32 format to 'dst->format' if needed.
WP2Status OrientateBuffer(Orientation orientation, const ArgbBuffer& src,
                          ArgbBuffer* dst);

// Orientates pixels in 'rect_in_src' from 'src' to 'dst'. The latter must be
// allocated and big enough to fit the area.
// Handles conversion from WP2_Argb_32 format to 'dst->format' if needed.
WP2Status OrientateSubBuffer(Orientation orientation, const ArgbBuffer& src,
                             const Rectangle& rect_in_src, ArgbBuffer* dst);

// Same functions but for Plane16.
WP2Status OrientateBuffer(Orientation orientation, Plane16* buffer);
WP2Status OrientateBuffer(Orientation orientation, const Plane16& src,
                          Plane16* dst);
WP2Status OrientateSubBuffer(Orientation orientation, const Plane16& src,
                             const Rectangle& rect_in_src, Plane16* dst);

// Same functions but done separately for each channel of YUVPlane.
WP2Status OrientateBuffer(Orientation orientation, YUVPlane* buffer);
WP2Status OrientateBuffer(Orientation orientation, const YUVPlane& src,
                          YUVPlane* dst);

// Returns the inverse orientation such as it gives the original image if both
// the 'orientation' and its inverse are applied.
Orientation GetInverseOrientation(Orientation orientation);

}  // namespace WP2

#endif  // WP2_UTILS_ORIENTATION_H_
