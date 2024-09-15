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
// Intratile filter. Encapsulates the deblocking, directional filters etc.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_DEC_FILTERS_INTRATILE_FILTER_H_
#define WP2_DEC_FILTERS_INTRATILE_FILTER_H_

#include <cstdint>

#include "src/common/filters/rstr_flt_params.h"
#include "src/dec/filters/alpha_filter.h"
#include "src/dec/filters/block_map_filter.h"
#include "src/dec/filters/deblocking_filter.h"
#include "src/dec/filters/directional_filter.h"
#include "src/dec/filters/grain_filter.h"
#include "src/dec/filters/restoration_filter.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

bool IsIntratileFilterEnabled(const DecoderConfig& config,
                              const GlobalParams& gparams);

//------------------------------------------------------------------------------

class IntratileFilter {
 public:
  IntratileFilter(const DecoderConfig& config, const GlobalParams& gparams,
                  const FilterBlockMap& blocks, const RstrFltParams& params);

  // If this filter is enabled, allocates storage.
  WP2Status Allocate();

  // Performs the filtering up to 'num_decoded_yuv_rows', which is the total
  // number of already decoded rows, as YUV. Returns the total number of
  // filtered pixel rows (still in YUV space).
  uint32_t Apply(uint32_t num_decoded_yuv_rows);

 private:
  const DecoderConfig& config_;
  const FilterBlockMap& blocks_;
  const bool enabled_;

  DeblockingFilter deblocking_filter_;
  DirectionalFilter directional_filter_;
  RestorationFilter restoration_filter_;
  AlphaFilter alpha_filter_;

  GrainFilter grain_filter_;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_DEC_FILTERS_INTRATILE_FILTER_H_
