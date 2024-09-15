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
//   AOM residual decoding, branched from libgav1, with as little changes as
//   possible.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_DEC_RESIDUALS_DEC_AOM_H_
#define WP2_DEC_RESIDUALS_DEC_AOM_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/common/lossy/residuals.h"
#include "src/common/lossy/residuals_aom.h"
#include "src/common/lossy/transforms.h"
#include "src/dec/symbols_dec.h"
#include "src/wp2/format_constants.h"

namespace WP2 {
namespace libgav1 {

//------------------------------------------------------------------------------
// From libgav1/src/tile/tile.h

// Class specialized in reading transform residuals.
// This is a pure interface (only static methods to call directly).
class AOMResidualReader : public AOMResidualIO {
 public:
  // Reads residual coeffs, returns the number of non-zero coefficients read.
  static uint32_t ReadCoeffs(Channel channel, const int16_t dequant[],
                             TrfSize tdim, TransformPair tx_type,
                             bool first_is_dc, SymbolReader* sr,
                             int16_t* coeffs);

 private:
  // Reads the position of the end of block.
  static int ReadEOB(TrfSize tdim, PlaneType plane_type, uint32_t context,
                     SymbolReader* sr);

  // Reads a coefficient and stores it at position 'coeffs[pos]'. Updates
  // 'levels' for reading following coeffs.
  // Returns true if the coeff read is not 0.
  // This method should be called in reverse zigzag order.
  static bool ReadCoeff(uint32_t pos, bool is_eob, uint32_t eob,
                        const int16_t dequant[], PlaneType plane_type,
                        TrfSize tdim, TransformClass tx_class, bool first_is_dc,
                        uint8_t* levels, SymbolReader* sr, int16_t* coeffs);

  // Part of 5.11.39.
  // Writes the coefficient's sign, and the remaining part that is above
  // kQuantizerCoefficientBaseRangeContextClamp if needed.
  static int16_t ReadSignAndRest(int16_t coeff, bool is_dc, SymbolReader* sr);
  static int ReadCoeffBaseRange(uint32_t cdf_context, PlaneType plane_type,
                                SymbolReader* sr);
};

}  // namespace libgav1
}  // namespace WP2

#endif /* WP2_DEC_RESIDUALS_DEC_AOM_H_ */
