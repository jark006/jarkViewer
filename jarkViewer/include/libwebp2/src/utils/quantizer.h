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
// Defines utilities to quantize a distribution to better store it and the
// population it represents.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#ifndef WP2_ENC_LOSSLESS_QUANTIZER_H_
#define WP2_ENC_LOSSLESS_QUANTIZER_H_

#include <cstddef>
#include <cstdint>

#include "src/utils/ans_enc.h"
#include "src/utils/ans_utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

class Quantizer {
 public:
  enum ConfigType { Raw, Huffman };
  // Config defining how a histogram is stored.
  // E.g.  [0,1,8,0,4]
  // If 'is_sparse' is true, it is stored as sparse, 'histo' contains [1,8,4]
  // and "runs" the mapping differences minus 1: [2,1,2] (the first element is
  // a difference to -1) minus 1 hence [1,0,1]
  // 'histogram_to_write' contains how 'histo' will be finally written, which
  // in Raw type is the same, but in Huffman is [0,3,2] (the powers of 2 used to
  // represent the histogram).
  // 'histo' represents a distribution of symbol and if we were to write
  // data with it, it would have an overall cost of 'cost'. Overall means the
  // cost of writing the symbols with probabilities defined by 'histo' but also
  // of storing 'histo'.
  struct ConfigParam {
    // The following members differentiate the different configurations.
    ConfigType type;
    bool is_sparse;
    uint8_t max_freq_bits;
    // Prefix code specific parameters.
    uint32_t prefix_code_histo_len;
    uint32_t prefix_code_prefix_size;
  };
  struct HistogramSparse {
    uint32_t* counts;
    const uint16_t* mapping;  // pointer to the original external mapping
    uint32_t nnz;             // number of non-zero counts
  };
  struct Config {
    // Writes the header and the count of a histogram.
    void WriteHistogramHeader(uint32_t symbol_range,
                              OptimizeArrayStorageStat* stats,
                              uint32_t stats_size, ANSEncBase* enc) const;
    void WriteHistogramCounts(uint32_t symbol_range, uint32_t max_count,
                              OptimizeArrayStorageStat* stats,
                              uint32_t stats_size, ANSEncBase* enc) const;

    ConfigParam param;
    // Histogram to write (which might be different from histo.counts: e.g.,
    // for sparse histograms, we know counts >0, hence we can subtract 1 when
    // writing).
    uint32_t* histogram_to_write;
    size_t size_to_write;
    HistogramSparse histo;
    // Cost of storing the symbols only (without the header).
    float cost_symbols_only;
    // Overall cost: header cost + cost_symbols_only.
    float cost;
  };

  // range_max is the maximum range of the used symbols (maximum value + 1).
  WP2Status Allocate(uint32_t range_max);

  // Given a histogram of symbols, their range, as well as the bit size
  // of the number of pixels n_pixels_bits, find the best way to store the
  // symbols represented by 'histogram' with the data in 'histogram' by
  // minimizing their overall size. 'size_sparse' is an upper bound of the
  // number of non-zero counts. 'effort' is between 0 (fastest) and 9 (slowest).
  // 'max_count' is the maximum value a count can take.
  // The function is re-entrant: if ResetConfigs() is not called, the best
  // configuration is kept in memory as a starting configuration. It will be
  // updated if a strictly better score is found.
  // 'cost_offset' is the cost that will be added to the Config cost.
  // 'cost_max' is the cost above which we can give up if
  // cost_offset + Config cost >= cost_max.
  bool Quantize(const uint32_t* histogram, const uint16_t* mapping,
                size_t size_sparse, uint32_t symbol_range, uint32_t max_count,
                float cost_max, float cost_offset, int effort);

  // Returns the current best configuration. The best configuration is updated
  // between Quantize calls.
  Config* GetBest() { return config_best_; }
  // Resets what is considered as the best config.
  void ResetBest() { config_best_ = nullptr; }

 private:
  // Implementation of the quantization that can call itself to compress its own
  // coefficients.
  // 'cost_max' is the value above which we can early exit.
  void QuantizeImpl(const uint32_t* histogram, const uint16_t* mapping,
                    size_t size_sparse, uint32_t symbol_range,
                    uint32_t max_count, int effort);

  static constexpr size_t kConfigNbr = 2;
  Config configs_[kConfigNbr];
  // Pointer to the current best config in configs_.
  Config* config_best_ = nullptr;
  uint32_t histogram_size_max_;

  Vector_u32 buffer_;  // Big memory chunk storing the Config buffers.

  VectorNoCtor<OptimizeArrayStorageStat> stats_buffer_;
};
}  // namespace WP2

#endif  // WP2_ENC_LOSSLESS_QUANTIZER_H_
