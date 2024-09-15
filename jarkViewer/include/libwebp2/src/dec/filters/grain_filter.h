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
// Grain 'filter'.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_DEC_FILTERS_GRAIN_FILTER_H_
#define WP2_DEC_FILTERS_GRAIN_FILTER_H_

#include <cstdint>

#include "src/common/lossy/block.h"
#include "src/dec/filters/block_map_filter.h"
#include "src/dsp/math.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"
#include "src/wp2/decode.h"

namespace WP2 {

//------------------------------------------------------------------------------

bool IsGrainFilterEnabled(const DecoderConfig& config,
                          const Vector<Segment>& segments);

//------------------------------------------------------------------------------

class GrainFilter {
 public:
  explicit GrainFilter(const DecoderConfig& config,
                       const Vector<Segment>& segments,
                       const FilterBlockMap& blocks);

  // Allocates storage.
  WP2Status Allocate();

  // Apply the grain up to 'num_rows' in YUV.
  // Returns the total number of processed rows.
  uint32_t Apply(uint32_t num_rows);

 private:
  const DecoderConfig& config_;
  const Vector<Segment>& segments_;
  const FilterBlockMap& blocks_;
  const bool enabled_;
  uint32_t last_row_;
  PseudoRNG rng_;

 private:
  // Visual debugging.
  YUVPlane unfiltered_pixels_;
  uint32_t num_debug_rows_ = 0;

  void SavePixelsForVDebug(uint32_t num_rows);
  void ApplyVDebug(uint32_t num_rows);
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_FILTERS_GRAIN_FILTER_H_
