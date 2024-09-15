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
// Tool for finding the best block layout.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_ENC_PARTITIONER_AREA_H_
#define WP2_ENC_PARTITIONER_AREA_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/progress_watcher.h"
#include "src/enc/partitioning/partitioner.h"
#include "src/utils/front_mgr.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Chooses the best sub-partition in each area.
// To be used only with AreaScoreFunc.
class AreaRDOptPartitioner : public Partitioner {
 public:
  AreaRDOptPartitioner(uint32_t area_width, uint32_t area_height)  // In pixels.
      : area_width_(area_width), area_height_(area_height) {}
  WP2Status GetBestPartition(const ProgressRange& progress,
                             VectorNoCtor<Block>* blocks,
                             Vector_u32* splits) override;

 private:
  virtual WP2Status GetAreaBestPartition(const Rectangle& area,
                                         const ProgressRange& progress,
                                         VectorNoCtor<Block>* blocks,
                                         uint32_t* max_num_blocks_left);

  const uint32_t area_width_, area_height_;  // In pixels.
};

//------------------------------------------------------------------------------

// Same as AreaRDOptPartitioner but encodes the whole area for each size of each
// block inside it. To be used only with SubAreaScoreFunc.
class SubAreaRDOptPartitioner : public AreaRDOptPartitioner {
 public:
  SubAreaRDOptPartitioner(uint32_t area_width, uint32_t area_height)
      : AreaRDOptPartitioner(area_width, area_height),
        front_mgr_(area_width, area_height) {}
  WP2Status GetBestPartition(const ProgressRange& progress,
                             VectorNoCtor<Block>* blocks,
                             Vector_u32* splits) override;

 private:
  WP2Status GetAreaBestPartition(const Rectangle& area,
                                 const ProgressRange& progress,
                                 VectorNoCtor<Block>* blocks,
                                 uint32_t* max_num_blocks_left) override;
  FrontMgrArea front_mgr_;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PARTITIONER_AREA_H_
