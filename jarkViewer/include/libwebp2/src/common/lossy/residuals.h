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
//   Common code for residuals
//
// Author: Vincent Rabaud (vrabaud@google.com)

#include <cassert>
#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

#ifndef WP2_COMMON_LOSSY_RESIDUALS_H_
#define WP2_COMMON_LOSSY_RESIDUALS_H_

namespace WP2 {

static const char* const kCoeffsStr[]{"Y_coeffs", "U_coeffs", "V_coeffs",
                                      "A_coeffs"};

struct ResidualBoxAnalyzer {
 public:
  // Finds the smallest bounding box of a set of coefficients so that all
  // non-zero values are within.
  static void FindBoundingBox(const int16_t* coeffs, uint32_t size, TrfSize dim,
                              uint32_t* max_x, uint32_t* max_y);
  // Computes whether we should use bounds on x/y.
  static void ShouldUseBounds(TrfSize tdim, uint32_t last_zigzag_ind,
                              uint32_t max_x, uint32_t max_y,
                              bool* use_bounds_x, bool* use_bounds_y);
  // Returns whether bounds could be useful for that dimension.
  static void CanUseBounds(TrfSize tdim, bool* can_use_bounds_x,
                           bool* can_use_bounds_y);
  static void FindBounds(TrfSize tdim, uint32_t max_x, uint32_t max_y,
                         uint32_t* min_zig_zag_ind_x,
                         uint32_t* min_zig_zag_ind_y);
  // Returns the range of X/Y when using bounds,
  static void GetRangeX(TrfSize tdim, uint8_t* min, uint8_t* max);
  static void GetRangeY(TrfSize tdim, uint8_t* min, uint8_t* max);
  // Returns the range of Y/X for a given X/Y when using bounds,
  static void GetRangePerX(TrfSize tdim, uint8_t max_x, uint8_t* min,
                           uint8_t* max);
  static void GetRangePerY(TrfSize tdim, uint8_t max_y, uint8_t* min,
                           uint8_t* max);

 private:
  // Per transform size, the min/max of the x bound.
  // For y, the same values (but for a block of symmetric dimensions) are used.
  static uint8_t bound_range_x[TRF_LAST][2];
  static constexpr uint32_t kIgnoreNum = 9u;
  // Omit the first dimensions for memory optimization.
  // Per transform size, given an x (/y) bound, what are the min/max of
  // the y(/x).
  static uint8_t bound_range_per_x[TRF_LAST - kIgnoreNum][kMaxBlockSizePix][2];
  static uint8_t bound_range_per_y[TRF_LAST - kIgnoreNum][kMaxBlockSizePix][2];
};

// Iterator for going over a block of residual in zigzag order.
class ResidualIterator {
 public:
  explicit ResidualIterator(TrfSize dim);
  virtual ~ResidualIterator() = default;
  // Resets the iterator to the first coefficient.
  virtual void Reset();
  // Moves the iterator forward.
  virtual void operator++();
  // Returns the index of the current coefficient, in the original non-zigzag
  // order.
  inline uint32_t Index() const {
    assert(ind_ < max_i_);
    return zigzag_[ind_];
  }
  inline uint32_t ZigZagIndex() const { return ind_; }
  // Returns the x and y position of the current coefficient.
  inline uint32_t x() const { return x_; }
  inline uint32_t y() const { return y_; }
  // Returns the zigzag mapping for a given size.
  static const uint16_t* GetZigzag(TrfSize dim);

 protected:
  const uint32_t max_i_;          // max possible index
  uint32_t ind_;                  // index in the block in zigzag order
  const uint16_t* const zigzag_;  // zigzag order we follow
  uint32_t x_, y_;                // current coefficient position
  const TrfSize dim_;             // block size
  const uint32_t bw_;             // block width in pixels
  const uint32_t bw_shift_;       // log2(bw_)
};
// This iterator takes into account the bounding box on the coefficients, if
// any, by skipping the values outside of it.
class BoundedResidualIterator : public ResidualIterator {
 public:
  BoundedResidualIterator(TrfSize dim, bool use_bounds_x, bool use_bounds_y,
                          uint32_t max_x, uint32_t max_y);
  void operator++() override;
  void Reset() override;
  // Uses the fact that the current coefficient is non-zero to update some
  // internal logic.
  void SetAsNonZero();
  // Returns whether the coefficient at the current position can be an End Of
  // Block.
  bool CanEOB() const;
  // Returns whether the iterator cannot move further.
  bool IsDone() const { return (max_num_coeffs_left_ == 0); }
  // Returns how many more coefficients are left, at most.
  uint32_t MaxNumCoeffsLeft() const { return max_num_coeffs_left_; }

 private:
  uint32_t max_num_coeffs_left_;        // maximum number of coefficients left
  const uint32_t max_x_, max_y_;        // maximum possible coordinates
  bool use_bounds_x_, use_bounds_y_;    // whether we use bounds
  bool can_eob_max_x_, can_eob_max_y_;  // whether we have touched the box
};

enum class EncodingMethod {
  kMethod0,
  kMethod1,
  kDCOnly,
  kNumMethod,
  kAllZero = kNumMethod  // No method because no coeff to signal.
};

class SymbolsInfo;

class ResidualIO {
 public:
  // TODO(vrabaud) Have this parameter depend on the block size.
  // Number of sectors in a block.
  static constexpr uint32_t kNumSectors = 4;

  // Initializes values and probas.
  void Init(bool use_aom_coeffs);

  // whether to use AOM residual coding or not
  bool use_aom() const { return use_aom_coeffs_; }

  // Returns the index of a method internally, in [0, kNumMethods[.
  static uint32_t GetMethodIndex(EncodingMethod method);

  // Returns the cluster for a given channel/method/dimension/sector.
  static uint32_t GetCluster(Channel channel, uint32_t num_channels,
                             EncodingMethod method = EncodingMethod::kMethod0,
                             TrfSize dim = TRF_2x2, uint32_t sector = 0);

  // Same as above but U and V channels share the same cluster.
  static uint32_t GetClusterMergedUV(Channel channel, uint32_t num_channels,
                                     EncodingMethod method,
                                     TrfSize dim = TRF_2x2,
                                     uint32_t sector = 0);

  // Initialize the probabilities for the symbol info.
  static WP2Status InitializeInfo(bool has_alpha, bool use_aom_coeffs,
                                  SymbolsInfo* info);

  // Looks at the coeffs and figure out the geometry of the residuals.
  // When it is not all zero or DC only, it returns kMethod0.
  static void SetGeometry(uint32_t num_coeffs, EncodingMethod* encoding_method);

 protected:
  // Returns the sector in which we are in a block. The sectors divide the block
  // in 4 as follows:   0  1
  //                    2  3
  // Each dimension is divided in half.
  static uint32_t GetSector(uint32_t x, uint32_t y, TrfSize dim);

  bool use_aom_coeffs_;
};

}  // namespace WP2

#endif /* WP2_COMMON_LOSSY_RESIDUALS_H_ */
