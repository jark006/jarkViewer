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
// ANS symbol decoding.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_DEC_SYMBOLS_DEC_H_
#define WP2_DEC_SYMBOLS_DEC_H_

#include <cstdint>

#include "src/common/symbols.h"
#include "src/utils/ans.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

// Class simplifying the reading of symbols.
struct StatExtra {
  uint32_t num_symbols;
  // normalized cumulative distribution of counts:
  //   cumul[0] = 0,  cumul[num_symbols] = tab_size
  const uint32_t* cumul;
  uint32_t log2_tab_size;
  const ANSSymbolInfo* codes;

  uint32_t freq(uint32_t sym) const { return (cumul[sym + 1] - cumul[sym]); }
};

class SymbolReader : public SymbolIO<StatExtra> {
 public:
  WP2Status Init(const SymbolsInfo& symbols_info, ANSDec* dec);

  const SymbolsInfo& symbols_info() const { return symbols_info_; }
  ANSDec* dec() const { return dec_; }

  // Sets the range for a particular symbol.
  inline void SetRange(uint32_t sym, int32_t min, int32_t max) {
    symbols_info_.SetMinMax(sym, min, max);
  }

  // For the 'cluster'th cluster of symbol 'sym' (given a maximum value of
  // non-zero values of 'max_nnz', which can be the number of pixels for
  // examples) reads its header to later decide how to read it.
  WP2Status ReadHeader(uint32_t sym, uint32_t cluster, uint32_t max_nnz,
                       WP2_OPT_LABEL);
  // Same as above but reads headers for all clusters.
  WP2Status ReadHeader(uint32_t sym, uint32_t max_nnz, WP2_OPT_LABEL);
  // can be called once ReadHeader() calls are finished
  void Clear() { infos_.reset(); }

  // Reads the symbol of type 'sym' in the 'cluster'th cluster.
  // If 'cost' is not nullptr, the cost in bits of storing 'sym' is ADDED to
  // 'cost'.
  int32_t Read(uint32_t sym, uint32_t cluster, WP2_OPT_LABEL,
               float* cost = nullptr);
  int32_t Read(uint32_t sym, WP2_OPT_LABEL, float* const cost = nullptr) {
    return Read(sym, /*cluster=*/0, label, cost);
  }
  // Same as above except we know that the read value cannot be strictly
  // superior to 'max_value'.
  WP2Status ReadWithMax(uint32_t sym, uint32_t cluster, uint32_t max_value,
                        WP2_OPT_LABEL, int32_t* value, float* cost = nullptr);
  // Fills the array 'is_maybe_used' with true when the symbol might be used,
  // false if we are sure it is not used.
  void GetPotentialUsage(uint32_t sym, uint32_t cluster, bool is_maybe_used[],
                         uint32_t size) const override;

 protected:
  // For the 'cluster'th cluster of symbol 'sym', sets the mode to trivial value
  // with value 'value'.
  void AddTrivial(uint32_t sym, uint32_t cluster, bool is_symmetric,
                  int32_t value);
  // For the 'cluster'th cluster of symbol 'sym', sets the mode to range value,
  // in [0:'range'), without mapping by default. Returns the corresponding Stat.
  Stat* AddRange(uint32_t sym, uint32_t cluster, bool is_symmetric,
                 uint16_t range);
  // For the 'cluster'th cluster of symbol 'sym', sets the mode to dictionary
  // value, using info from 'infos'. 'infos' is modified in this call.
  WP2Status AddDict(uint32_t sym, uint32_t cluster, bool is_symmetric,
                    ANSCodes* infos);
  // For the 'cluster'th cluster of symbol 'sym', sets the mode to kPrefixCode
  // value, using info from 'infos'. 'infos' is modified in this call.
  // 'prefix_size' is the number of bits in the prefix code.
  WP2Status AddPrefixCode(uint32_t sym, uint32_t cluster, bool is_symmetric,
                          ANSCodes* infos, uint32_t prefix_size);

 private:
  // Generic implementation called by the different Read specializations above.
  WP2Status ReadInternal(uint32_t sym, uint32_t cluster, bool use_max_value,
                         uint32_t max_value, WP2_OPT_LABEL, int32_t* value,
                         float* cost = nullptr);
  // return the largest index 'idx' such that 'stat.mappings[idx] <= max_value'.
  static uint32_t FindSymbol(const Stat& stat, uint32_t max_value);

  ANSDec* dec_;
  // Code tables used by dictionaries (StatExtra.codes point to them).
  Vector<ANSCodes> codes_;
  // Cumulative frequencies for dictionaries (StatExtra.cumul point to them).
  Vector<Vector_u32> counts_;
  // Tmp variable used during the updates to avoid re-allocations.
  ANSCodes infos_;
};

}  // namespace WP2

#endif /* WP2_DEC_SYMBOLS_DEC_H_ */
