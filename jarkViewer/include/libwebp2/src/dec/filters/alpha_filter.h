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
// Alpha filter (bilateral filter).
//
// Author: Maryla (maryla@google.com)

#ifndef DEC_FILTERS_ALPHA_FILTER_H_
#define DEC_FILTERS_ALPHA_FILTER_H_

#include <cstdint>

#include "src/dec/filters/block_map_filter.h"
#include "src/utils/plane.h"
#include "src/wp2/base.h"

namespace WP2 {

bool IsAlphaFilterEnabled(const DecoderConfig& config,
                          const GlobalParams& gparams);

// Filter for the alpha plane: applies a bilateral filter which smoothes the
// image while preserving edges.
class AlphaFilter {
 public:
  AlphaFilter(const DecoderConfig& config, const GlobalParams& gparams,
              const FilterBlockMap& blocks);

  WP2Status Allocate();

  // Applies the filter up to 'num_rows'. Returns the total number of rows
  // filtered.
  uint32_t Smooth(uint32_t num_rows);

 private:
  static constexpr uint32_t kMaxKernelRadius = 2;
  static constexpr uint32_t kTapHeight = 2 * kMaxKernelRadius + 1;
  static constexpr uint32_t kMaxKernelSize2 = kTapHeight * kTapHeight;

  bool enabled_ = true;
  const DecoderConfig& config_;
  const FilterBlockMap& blocks_;
  uint32_t num_filtered_rows_ = 0;

  // Contains unfiltered pixels from the previous rows and the current row.
  Plane16 top_taps_;

  uint32_t kernel_radius_;         // Radius of gaussian kernel.
  const uint32_t* gaussian_;       // Weights for gaussian blur.
  const uint32_t* value_weights_;  // Weights for pixel value differences.

  // Visual debugging.
  YUVPlane unfiltered_pixels_;
  uint32_t num_debug_rows_ = 0;
  void SavePixelsForVDebug(uint32_t num_rows);
  void ApplyVDebug(uint32_t num_rows);
};

}  // namespace WP2

#endif  // DEC_FILTERS_ALPHA_FILTER_H_
