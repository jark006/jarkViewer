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
//   utilities for Wiener-optimal filtering
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_UTILS_WIENER_H_
#define WP2_UTILS_WIENER_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace WP2 {

// Helper function:
// solves A.x = y for x. Returns false in case of degenerate case.
// Note that A[n x n] and y[n] inputs are modified (once).
bool LinearSolve(size_t n, double A[], double y[], float x[]);

// All-in-one class for minimizing the L2 error when N context samples are
// used to predict M output samples.
// One should train the optimizer by calling AddSample() several times.
// Then, calling Optimize() will return the set of matrix coefficients
// that optimally predict each output N samples from M samples through linear
// combination. This can only be called once. New cycle should start with a
// call to Reset().
template <size_t N, size_t M>
class WienerOptimizer {
 public:
  WienerOptimizer() noexcept { Reset(); }

  // should be called after calls to Optimize(), to reset the accumulators
  void Reset() {
    count_ = 0;
    memset(acc_, 0, sizeof(acc_));
    memset(cov_, 0, sizeof(cov_));
  }

  // Add a new measurement: values[] are the value we are trying to predict
  // from the context[] samples.
  void AddSample(const int16_t context[N], const int16_t values[M]) {
    for (size_t m = 0; m < M; ++m) {
      const double ref_value = values[m];
      for (size_t j = 0; j < N; ++j) {
        acc_[m][j] += ref_value * context[j];
        for (size_t i = 0; i < N; ++i) {
          cov_[m][j][i] += context[j] * context[i];
        }
        cov_[m][j][N] += context[j];
        cov_[m][N][j] += context[j];
      }
      // context[N] = 1 fake entry, for average component.
      acc_[m][N] += ref_value;
    }
    ++count_;
  }

  // warning! will trash cov_[][][] and acc_[][].
  // output[][N] is the average component.
  bool Optimize(float output[M][N + 1]) {
    if (count_ <= 2 * N) return false;  // not enough stats
    for (size_t m = 0; m < M; ++m) {
      cov_[m][N][N] = count_;  // finish setting up cov matrix
      if (!Solve(cov_[m], acc_[m], output[m])) return false;
    }
    return true;
  }

 private:
  bool Solve(double m[N + 1][N + 1], double b[N + 1], float output[N + 1]) {
    if (!LinearSolve(N + 1, &m[0][0], b, output)) {
      return false;
    }
    return true;
  }

  double acc_[M][N + 1];         // cross-correlation vectors
  double cov_[M][N + 1][N + 1];  // auto-correlation matrices
  size_t count_;
};

}  // namespace WP2

#endif  // WP2_UTILS_WIENER_H_
