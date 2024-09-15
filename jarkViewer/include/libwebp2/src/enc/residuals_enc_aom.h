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
//   AOM residual encoding, reverse-engineered from libgav1 decoder.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_ENC_RESIDUALS_ENC_AOM_H_
#define WP2_ENC_RESIDUALS_ENC_AOM_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/residuals_aom.h"
#include "src/common/lossy/transforms.h"
#include "src/enc/symbols_enc.h"
#include "src/utils/ans_enc.h"
#include "src/wp2/format_constants.h"

namespace WP2 {
namespace libgav1 {

// Class writing AOM residuals.
// Each member function matches one in AOMResidualReader.
// This is a pure interface (only static methods to call directly).
class AOMResidualWriter : public AOMResidualIO {
 public:
  static void WriteCoeffs(const int16_t res[], Channel channel, TrfSize tdim,
                          TransformPair tx_type, bool first_is_dc,
                          SymbolManager* sm, ANSEncBase* enc);

  // Writes the position of the end of block.
  static void WriteEOB(int eob, TrfSize tdim, PlaneType plane_type,
                       TransformClass tx_class, SymbolManager* sm,
                       ANSEncBase* enc);

  // Writes the coefficient 'res'. Updates 'levels' for writing following
  // coeffs. This method should be called in reverse zigzag order.
  static void WriteCoeff(int16_t res, uint32_t pos, bool is_eob, uint32_t eob,
                         bool first_is_dc, PlaneType plane_type, TrfSize tdim,
                         TransformClass tx_class, const uint8_t* levels,
                         SymbolManager* sm, ANSEncBase* enc);

 private:
  // Writes the coefficient's sign, and the remaining part that is above
  // kQuantizerCoefficientBaseRangeContextClamp if needed.
  static void WriteSignAndRest(int16_t res, bool is_dc, SymbolManager* sm,
                               ANSEncBase* enc);

  static void WriteCoeffBaseRange(uint32_t level, uint32_t cdf_context,
                                  PlaneType plane_type, SymbolManager* sm,
                                  ANSEncBase* enc);
};

}  // namespace libgav1
}  // namespace WP2

#endif /* WP2_ENC_RESIDUALS_ENC_AOM_H_ */
