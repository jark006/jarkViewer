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
//   Random matrices
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_LOSSY_RND_MTX_H_
#define WP2_COMMON_LOSSY_RND_MTX_H_

#include <cstdint>

#include "src/common/lossy/block_size.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

// #define WP2_COLLECT_MTX_SCORES   // to collect matrix stats
#if defined(WP2_COLLECT_MTX_SCORES)
#include "src/utils/stats.h"

extern WP2::Stats<uint32_t> scores;
#endif

namespace WP2 {

class CodedBlockBase;
struct QuantMtx;

static constexpr uint32_t kMaxNumRndMtx = 32;

//------------------------------------------------------------------------------
// Random Matrix
//
//  They are used to code the -1/1 trailing coefficients.
// 1rst pass: coeff[] = large coeffs
// 2nd pass: if rnd[i] != 0:
//    if coeffs[i] is large: coeffs[i] += 1 in abs-value
//    otherwise coeffs[i] = rnd[i]

uint32_t FillRandomMatrix(uint32_t id, uint32_t sparsity, uint32_t freq_cut_off,
                          uint32_t sx, uint32_t sy, int16_t m[]);
// returns the number of ones:
uint32_t DownscaleMatrix(const int16_t* src, int16_t* dst, uint32_t sx,
                         uint32_t sy, bool vertically);

// add/subtract operations (TODO(skal): move to dsp/)
void AddRandomMatrix(uint32_t size, const int16_t rnd[], int16_t coeffs[]);
void AddRandomMatrix(uint32_t size, const int16_t rnd[], int32_t coeffs[]);
void SubtractRandomMatrix(uint32_t size, const int16_t rnd[], int16_t coeffs[],
                          uint32_t* num_coeffs);

// match scoring
uint32_t RandomMatrixScore(const int16_t rnd[], const int16_t coeffs[],
                           uint32_t sx, uint32_t sy);

// debug
void PrintRandomMatrix(uint32_t sx, uint32_t sy, const int16_t m[],
                       bool full_coeffs = false);

template <int SX, int SY>
struct RndMtx {
 public:
  int16_t rnd[SX * SY];  // +1/-1 or 0 coeffs
  uint32_t num_ones;     // for hashing

 public:
  void SubtractFrom(int16_t coeffs[], uint32_t* const num_coeffs) const {
    if (num_ones) SubtractRandomMatrix(SX * SY, rnd, coeffs, num_coeffs);
  }
  void AddTo(int16_t coeffs[]) const {
    if (num_ones) AddRandomMatrix(SX * SY, rnd, coeffs);
  }
  void AddTo(int32_t coeffs[]) const {
    if (num_ones) AddRandomMatrix(SX * SY, rnd, coeffs);
  }

  uint32_t MatchScore(const int16_t coeffs[]) const {
    return (num_ones ? RandomMatrixScore(rnd, coeffs, SX, SY) : 0u);
  }
  // * id is used for random seed
  // * Sparsity is fractional 8b (0=empty, 255 = full)
  // * freq_cut_off is fractional 8b (larger value = more compact distribution)
  void FillRandom(uint32_t id, uint32_t sparsity, uint32_t freq_cut_off) {
    num_ones = FillRandomMatrix(id, sparsity, freq_cut_off, SX, SY, rnd);
  }
  void DownscaleFrom(const int16_t* src, bool vertically) {
    num_ones = DownscaleMatrix(src, rnd, SX, SY, vertically);
  }

  // debug
  void Print() const { PrintRandomMatrix(SX, SY, rnd); }
};

static constexpr uint8_t kRndMtxBadIdx = 0xff;
static_assert(kRndMtxBadIdx > kMaxNumRndMtx, "BadIdx should be larger.");

// For the encoder:
template <int SX, int SY>
uint32_t FindAndSubtractBestMatch(const Vector<RndMtx<SX, SY>>& mtx,
                                  int16_t coeffs[],
                                  uint32_t* const num_coeffs) {
  uint32_t best_id = kRndMtxBadIdx;
  uint32_t best_score = 0;
  for (uint32_t id = 0; id < mtx.size(); ++id) {
    if (mtx[id].num_ones == 0) continue;
    const uint32_t score = mtx[id].MatchScore(coeffs);
    if (score > best_score) {
      best_score = score;
      best_id = id;
    }
  }
  if (best_id != kRndMtxBadIdx) {
    mtx[best_id].SubtractFrom(coeffs, num_coeffs);
#if defined(WP2_COLLECT_MTX_SCORES)
    const uint32_t idx =
        GetBlockSize(SX / kMinBlockSizePix, SY / kMinBlockSizePix);
    scores.Add(idx, best_score);
#endif
  }
  return best_id;
}

class RndMtxSet {
 public:
  WP2Status Init(uint32_t num_mtx, const QuantMtx& quant);
  void DecideUseRndMtx(CodedBlockBase* cb) const;

  // Returns true if dimension is compatible.
  bool IsOk(uint32_t id, TrfSize tdim) const;
  // Add matrix contribution to coeffs.
  void AddMatrix(uint32_t id, TrfSize tdim, int16_t dst[]) const;
  void AddMatrix(uint32_t id, TrfSize tdim, int32_t dst[]) const;

  void Reset();  // frees memory
  WP2Status CopyFrom(const RndMtxSet& src);

  // downscale from 'src', halving horizontally or vertically
  template <int SX, int SY>
  WP2Status Downscale(const Vector<RndMtx<2 * SX, SY>>& src,
                      Vector<RndMtx<SX, SY>>* const dst) {
    WP2_CHECK_ALLOC_OK(dst->resize(num_mtx_));
    for (uint32_t i = 0; i < num_mtx_; ++i) {
      (*dst)[i].DownscaleFrom(src[i].rnd, true);
    }
    return WP2_STATUS_OK;
  }
  template <int SX, int SY>
  WP2Status Downscale(const Vector<RndMtx<SX, 2 * SY>>& src,
                      Vector<RndMtx<SX, SY>>* const dst) {
    WP2_CHECK_ALLOC_OK(dst->resize(num_mtx_));
    for (uint32_t i = 0; i < num_mtx_; ++i) {
      (*dst)[i].DownscaleFrom(src[i].rnd, false);
    }
    return WP2_STATUS_OK;
  }

 protected:
  uint32_t num_mtx_ = 0;

  Vector<RndMtx<4, 4>> mtx4x4_;
  Vector<RndMtx<4, 8>> mtx4x8_;
  Vector<RndMtx<8, 4>> mtx8x4_;
  Vector<RndMtx<8, 8>> mtx8x8_;
  Vector<RndMtx<16, 8>> mtx16x8_;
  Vector<RndMtx<8, 16>> mtx8x16_;
  Vector<RndMtx<16, 16>> mtx16x16_;
  Vector<RndMtx<32, 16>> mtx32x16_;
  Vector<RndMtx<16, 32>> mtx16x32_;
  Vector<RndMtx<32, 32>> mtx32x32_;
};

}  // namespace WP2

#endif /* WP2_COMMON_LOSSY_RND_MTX_H_ */
