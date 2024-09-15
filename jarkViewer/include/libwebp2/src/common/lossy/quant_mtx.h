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
//   Quantization matrices
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_LOSSY_QUANT_MTX_H_
#define WP2_COMMON_LOSSY_QUANT_MTX_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/dsp/dsp.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

class RndMtxSet;

//------------------------------------------------------------------------------
// Quantization

static constexpr uint32_t kNumQuantZones = 5;

struct QuantMtx {
 public:
  // allocate the *encoding* data space
  WP2Status AllocateForEncoder();

  // Initialize quant_steps[] for steps[].
  // max_residual is the maximum absolute input values to be quantized.
  // q_scale is the expected precision scale of the input Y/U/V coeffs
  // (typically 1<<yuv_bits with yuv_bits ~= 8-10bits, depending on the CSP).
  void Init(uint32_t max_residual, float q_scale,
            const uint16_t steps[kNumQuantZones]);
  // Set the lambda magic constants (encoder), using quant_steps[].
  // Must be called after Init().
  void SetLambda();

  // Quantizes or dequantizes.
  // If 'first_is_dc', 'coeffs[0]' will be treated differently.
  // If 'use_bias' is true, rounding will be biased towards 0.
  // The dequantized result is returned in 'dequantized_coeffs'.
  void Quantize(const int32_t residuals[], TrfSize dim, bool first_is_dc,
                int16_t coeffs[], uint32_t* num_coeffs,
                int16_t dequantized_coeffs[], bool use_bias = true) const;
  // Returns the dequantization matrix (used by decoder only).
  const int16_t* Dequant() const { return dequants; }
  // Returns the dequantization value for the coeff at position 'pos' for a
  // transform of width 'tx_width'.
  int16_t Dequant(uint32_t pos, uint32_t tx_width) const {
    return dequants[QuantIdx(pos, tx_width)];
  }

  // Computes the quantization error on DC.
  // Note that DC, and hence the DC error, is scaled by sqrt(dim/2) by
  // WP2Transform2D. To compare DC error values for blocks of different sizes,
  // the value should be normalized.
  int16_t DCError(int32_t DC, TrfSize dim) const;

  uint32_t GetMaxAbsResDC(TrfSize tdim) const { return max_abs_res_dc[tdim]; }

  // Returns the index of the quant/dequant value corresponding to the coeff
  // at position 'pos' for a transform of width 'tx_width'.
  static uint32_t QuantIdx(uint32_t pos, uint32_t tx_width) {
    return (pos % tx_width) + WP2QStride * (pos / tx_width);
  }

  void Print() const;  // debug

 public:
  float lambda;  // used for rate-distortion optimization
  // used for rate-distortion in trellis-like coeff optimization
  float lambda_trellis;

 protected:
  // Rounding bias. A value of (1<<WP2QBits)/2 will result in normal
  // rounding. A smaller value will bias towards 0.
  static constexpr uint32_t kBias = (1u << WP2QBits) / 3;
  static constexpr uint32_t kNoBias = (1u << WP2QBits) / 2;

  uint16_t quant_steps[kNumQuantZones] = {0};  // quant id for each zones

  // quantization and dequantization factors for converting between transform
  // coefficients and coded levels:
  //   level[] = (coeff[] * iquant[] + bias) >> WP2QBits
  //   coeffs[] = level[] * dequant[]
  // quant[] incorporates the transform scaling constant sqrt(W * H / 4).
  static constexpr uint32_t kDataSize = WP2QStride * WP2QStride;
  int16_t dequants[kDataSize];  // 15 bits (always > 0)
  // for encoder:
  Vector_u32 iquants;
  // ranges of quantized DC residuals, for each transform
  uint16_t max_abs_res_dc[TRF_LAST];
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_COMMON_LOSSY_QUANT_MTX_H_ */
