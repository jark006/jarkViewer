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
//  Segment
//
// Author: Skal (pascal.massimino@gmail.com)
//
#ifndef WP2_COMMON_LOSSY_SEGMENT_H_
#define WP2_COMMON_LOSSY_SEGMENT_H_

#include <cassert>
#include <cstdint>

#include "src/common/lossy/quant_mtx.h"
#include "src/utils/ans.h"
#include "src/utils/ans_enc.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

struct EncoderConfig;
class CSPTransform;
class CodedBlock;
class CodedBlockBase;
class SymbolManager;
class SymbolReader;
class GlobalParams;

//------------------------------------------------------------------------------
// Grain

class GrainParams {
 public:
  void Write(ANSEnc* enc) const;
  void Read(ANSDec* dec);

  bool operator==(const GrainParams& other) const;
  bool IsUsed() const { return (y_ > 0 || uv_ > 0); }
  void Reset();

  void Print() const;  // debug

 public:
  uint8_t y_ = 0, uv_ = 0;            // grain level [0=off, 100=full]
  uint8_t freq_y_ = 0, freq_uv_ = 0;  // grain frequency [0..6]
};

//------------------------------------------------------------------------------
// Segment

class Segment {
 public:
  // I/O for header
  WP2Status WriteHeader(ANSEnc* enc, bool write_alpha, bool write_grain) const;
  WP2Status ReadHeader(uint32_t u_quant_offset, uint32_t v_quant_offset,
                       bool read_alpha, bool read_grain, ANSDec* dec);

  // I/O for Encoding methods.
  WP2Status ReadEncodingMethods(SymbolReader* sr, CodedBlockBase* cb) const;

  void StoreEncodingMethods(const CodedBlockBase& cb, SymbolManager* sm,
                            ANSEncBase* enc) const;

 public:
  // quantization
  QuantMtx quant_y_;
  QuantMtx quant_u_;
  QuantMtx quant_v_;
  QuantMtx quant_a_;

  const QuantMtx& GetQuant(Channel channel) const {
    switch (channel) {
      case kYChannel:
        return quant_y_;
      case kUChannel:
        return quant_u_;
      case kVChannel:
        return quant_v_;
      default:
        assert(channel == kAChannel);
        return quant_a_;
    }
  }

  // Returns the maximum absolute value of the DC coefficient over all possible
  // block dimensions.
  uint32_t GetMaxAbsDC(Channel channel) const;

  uint16_t quant_steps_[4][kNumQuantZones] = {{0}};  // Y/U/V/A
  bool use_quality_factor_ = true;
  uint16_t quality_factor_ = kQualityToQFactor(75.f);
  uint16_t a_quality_factor_ = kQualityToQFactor(75.f);

  // Sets the range of YUV values max_residual_ to [yuv_min, yuv_max] inclusive.
  // The quantization scale q_scale_ is updated based on it too.
  // This must be called before FinalizeQuant().
  void SetYUVBounds(int16_t yuv_min, int16_t yuv_max);

  // Fills quant_steps_[][] with flat values based on 'quality_factor' in
  // [0,kQFactorMax]. Note 'quality_factor' is backwards: the higher it is, the
  // more the image is compressed.
  void SetQuality(uint32_t quality_factor, uint32_t u_quant_offset,
                  uint32_t v_quant_offset);
  // Same, for kAChannel. Must be called *after* SetQuality().
  void SetAlphaQuality(uint32_t quality_factor);

  // Fills the corresponding quant_steps_[]
  void SetQuantSteps(uint16_t quants[kNumQuantZones], Channel channel);
  void GetQuantSteps(uint16_t quants[kNumQuantZones], Channel channel) const;

  // Initializes the quant_mtx based on quant_steps_[].
  // q_scale must be set before calling this.
  void FinalizeQuant();
  // Same, for kAChannel. Must be called *after* FinalizeQuant().
  void FinalizeAlphaQuant();

  // must be called before FinalizeQuant()
  WP2Status AllocateForEncoder(bool for_alpha = false);

  // Sets the YUV bounds, the quality and the quant mtx based on 'qfactor'.
  // 'qfactor' should be in [0, kFactorMax] range.
  // The alpha quantization factor will be set to default value.
  void SetQuantizationFactor(const CSPTransform& transform,
                             uint32_t u_quant_offset, uint32_t v_quant_offset,
                             uint32_t qfactor);
  // Same, but for alpha channel. Must be called *after* SetQuantizationFactor()
  void SetAlphaQuantizationFactor(uint32_t qfactor);

  // Returns true if this segment is "close enough" to 'other' that they can be
  // merged (some fields might still be different).
  bool IsMergeableWith(const Segment& other) const;

  // Filtering and noise/grain generation.
  bool use_grain_ = false;
  GrainParams grain_;

 public:
  // Basic characteristics of a segment, which can be use to determine
  // quantization and other compression parameters. Used by encoder only.
  // 'risk_' is heuristic a value between 0 (high risk of visible artifacts)
  // and 1 (low risk of artifacts). The value impacts the choice of quantizers.
  // 'risk_class_' is a heuristic index mapping to risk_ values.
  float risk_ = 0.f;
  int risk_class_ = 0;  // class 0 = most difficult

  static constexpr float kDefaultAvgLambda = 1.f;
  float avg_lambda_ = kDefaultAvgLambda;  // complexity multiplier for lambda

 private:
  float q_scale_ = 0.f;  // y/u/v quantization precision (depending on range)
  uint32_t max_residual_;
};

//------------------------------------------------------------------------------

// Colors of segments for visual debugging.
extern const Argb32b kSegmentColors[kMaxNumSegments];

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_COMMON_LOSSY_SEGMENT_H_
