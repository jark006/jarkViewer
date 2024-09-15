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
//  Transforms
//
#ifndef WP2_COMMON_LOSSY_TRANSFORMS_H_
#define WP2_COMMON_LOSSY_TRANSFORMS_H_

#include "src/dsp/dsp.h"

namespace WP2 {

// Transform pair for horizontal and vertical directions.
enum TransformPair {
  kDctDct = 0,
  kAdstAdst,
  kDctAdst,
  kAdstDct,
  kIdentityDct,
  kDctIdentity,
  kIdentityIdentity,
  kNumTransformPairs,
  kUnknownTf = kNumTransformPairs  // Transform cannot be deduced, signal it.
};

// Transform for vertical and horizontal axes corresponding to each transform
// pair.
extern const WP2TransformType kTfX[kNumTransformPairs];
extern const WP2TransformType kTfY[kNumTransformPairs];

enum class TransformClass { kTwoD, kHorizontal, kVertical };

// Returns the class corresponding to the given transform.
TransformClass GetTransformClass(TransformPair transform);

}  // namespace WP2

#endif
