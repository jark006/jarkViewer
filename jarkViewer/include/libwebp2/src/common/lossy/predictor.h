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
// Predictors for blocks.
//
// Authors: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_LOSSY_PREDICTOR_H_
#define WP2_COMMON_LOSSY_PREDICTOR_H_

#include <array>
#include <cstdint>
#include <limits>
#include <string>

#include "src/dsp/dsp.h"
#include "src/utils/ans.h"
#include "src/utils/ans_enc.h"
#include "src/utils/csp.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/utils/wiener.h"
#include "src/wp2/base.h"
#include "src/wp2/format_constants.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Returns a string containing the values of the pixels inside the 'block' of
// 'w' by 'h' pixels and its surrounding 'context'.
std::string GetContextAndBlockPixelsStr(const int16_t context[],
                                        const int16_t context_right[],
                                        const int16_t context_left[],
                                        uint32_t w, uint32_t h,
                                        const int16_t block[], uint32_t step);

//------------------------------------------------------------------------------
// Predictors

typedef WienerOptimizer<kContextSize, kPredWidth * kPredHeight> PredOptimizer;

class CodedBlock;
class CodedBlockBase;
class SymbolManager;
class Counters;
class SymbolReader;

// A predictor predicts a kPredWidth x kPredHeight block's values using
// heuristics and local context information.
class Predictor : public WP2Allocable {
 public:
  virtual ~Predictor() = default;

  // Predicts values for the given block, on channel 'channel', for the
  // whole block if 'split_tf' is false, or for sub block 'tf_i' if 'split_tf'
  // is true. Stores the result to 'output', incremented by 'step' for each row.
  virtual void Predict(const CodedBlockBase& cb, Channel channel, bool split_tf,
                       uint32_t tf_i, int16_t output[],
                       uint32_t step) const = 0;

  // Returns true if it should be processed again every time luma changes.
  virtual bool DependsOnLuma() const { return false; }

  // The three following methods are for predictors that signal some
  // parameters in the bitstream.

  // Computes parameters needed for this predictor and stores them in the block.
  // This should be called on the encoder side before using the predictor.
  // Returns false if the predictor seems too costly or useless, and should be
  // avoided.
  virtual bool ComputeParams(CodedBlock* cb, Channel channel) const {
    (void)cb, (void)channel;
    return true;
  }
  // Writes the side-parameter to the bitstream.
  // Returns the sub-mode used, for info.
  virtual uint32_t WriteParams(const CodedBlockBase& cb, Channel channel,
                               SymbolManager* sm, ANSEncBase* enc) const {
    (void)cb, (void)channel, (void)sm, (void)enc;
    return 0;
  }
  // Reads the side-parameter for the predictor. Return the index offset to the
  // actual predictor to use (for instance, when refining the angular
  // prediction). Returns 0 if the predictor isn't changed.
  virtual uint32_t ReadParams(CodedBlockBase* cb, Channel channel,
                              SymbolReader* sm, ANSDec* dec) const {
    (void)cb, (void)channel, (void)sm, (void)dec;
    return 0;
  }
  // Setter/Getter for the mode of a predictor. For now, each predictor is its
  // own mode, and we set it to the index in the vector containing all
  // predictors.
  void SetMode(uint32_t mode) { mode_ = mode; }
  virtual uint32_t mode() const { return mode_; }
  // Returns true if this predictor is an angle predictor.
  virtual bool IsAngle(float* angle) const { return false; }

  // Debugging methods.
  virtual std::string GetName() const = 0;
  virtual std::string GetInfoStr() const { return ""; }
  virtual std::string GetFakePredStr() const = 0;
  virtual std::string GetPredStr(const CodedBlockBase& cb, Channel channel,
                                 bool split_tf, uint32_t tf_i) const = 0;

  static constexpr uint32_t kInvalidMode = std::numeric_limits<uint32_t>::max();

 private:
  uint32_t mode_ = kInvalidMode;
};

// This vector will *own* the Predictor pointers and destruct them
// when going out of scope.
struct PredictorVector : public Vector<Predictor*> {
  PredictorVector() = default;
  PredictorVector(PredictorVector&&) = default;
  ~PredictorVector() { reset(); }
  void reset();
};

//------------------------------------------------------------------------------
// Class containing the different predictors.

struct EncoderConfig;
class BlockContext;
class Segment;

class Predictors {
 public:
  Predictors() = default;
  Predictors(Predictors&&) = default;

  void reset() {
    preds_.reset();
    preds_no_angle_.reset();
  }
  bool empty() const { return preds_.empty(); }
  uint32_t size() const { return preds_.size(); }
  const Predictor* operator[](uint32_t i) const { return preds_[i]; }
  PredictorVector::const_iterator begin() const { return preds_.begin(); }
  PredictorVector::const_iterator end() const { return preds_.end(); }

  // Get the predictor for a given mode read from the bitstream.
  const Predictor* GetPred(uint32_t mode, uint32_t sub_mode = 0) const;
  // Get the maximum mode (inclusive) contained in a predictor vector.
  uint32_t GetMaxMode() const;

  enum class Pred {
    kDcAll,
    kDcLeft,
    kDcTop,
    kMedianDcAll,
    kMedianDcLeft,
    kMedianDcTop,
    kDoubleDcAll,
    kDoubleDcLeft,
    kDoubleDcTop,
    kSmooth2D,
    kSmoothVertical,
    kSmoothHorizontal,
    kTrueMotion,
    kGradient,
    kAngle23,
    kAngle45,
    kAngle67,
    kAngle90,
    kAngle113,
    kAngle135,
    kAngle157,
    kAngle180,
    kAngle203,
    kAngle225,
    kCfl,
    kSignalingCfl,
    kZero,
    kNum,
  };

 protected:
  Channel channel_;

  // Fills the different angle and non-angle predictors.
  WP2Status FillImpl(const Pred* mapping, uint32_t num_modes, int16_t min_value,
                     int16_t max_value, const CSPTransform* transform);

 private:
  // Takes care of recording a freshly allocated predictor 'pred'.
  // Warning: pred can be wrapped in an alpha-predictor, so the
  // returned predictor is not necessarily the input 'pred'.
  // Returns nullptr in case of problem (and disallocates 'pred').
  Predictor* RecordPredictor(Predictor* pred, uint32_t mode,
                             const CSPTransform* transform);

  PredictorVector preds_;
  Vector<const Predictor*> preds_no_angle_;
  std::array<const Predictor*, kAnglePredNum> preds_main_angle_;
  // Index of the first predictor with mode 'i' in preds_[].
  uint32_t main_modes_[kYBasePredNum + kAnglePredNum];
};

class YAPredictors : public Predictors {
 public:
  YAPredictors() = default;
};

class YPredictors : public YAPredictors {
 public:
  YPredictors() { channel_ = kYChannel; }
  YPredictors(YPredictors&&) = default;
  // Fills the object with the non-angle and angle predictors.
  WP2Status Fill(int16_t min_value, int16_t max_value);
};

class APredictors : public YAPredictors {
 public:
  APredictors() { channel_ = kAChannel; }
  APredictors(APredictors&&) = default;
  // Fills the object with the non-angle and angle predictors.
  WP2Status Fill(int16_t min_value, int16_t max_value,
                 const CSPTransform* transform);
};

class UVPredictors : public Predictors {
 public:
  UVPredictors() { channel_ = kUChannel; }
  UVPredictors(UVPredictors&&) = default;
  // Fills the object with the non-angle and angle predictors.
  WP2Status Fill(int16_t min_value, int16_t max_value);
};

// Returns a pointer to Predictor. The caller has to take an ownership of the
// pointer. Returns null if allocation fails.
Predictor* MakeNonAnglePredictor(Predictors::Pred predictor_type,
                                 int16_t min_value, int16_t max_value);

//------------------------------------------------------------------------------

// Context-based predictor base class.
class ContextPredictor : public Predictor {
 public:
  void Predict(const CodedBlockBase& cb, Channel channel, bool split_tf,
               uint32_t tf_i, int16_t output[], uint32_t step) const override;

  std::string GetFakePredStr() const override;
  std::string GetPredStr(const CodedBlockBase& cb, Channel channel,
                         bool split_tf, uint32_t tf_i) const override;

 protected:
  virtual void Predict(const int16_t context[], uint32_t w, uint32_t h,
                       int16_t output[], uint32_t step) const = 0;
  std::string GetPredStr(const int16_t context[], const int16_t context_right[],
                         const int16_t context_left[], ContextType context_type,
                         uint32_t width, uint32_t height) const;
  ContextType context_type_ = kContextSmall;
};

//------------------------------------------------------------------------------
// CFL predictors.

// Chroma-form-luma predictor. Predicts chroma as being:
// chroma = a * luma + b
// where the a and b parameters are entirely deduced from context pixels.
// Also used for alpha.
class CflPredictor : public Predictor {
 public:
  CflPredictor(int16_t min_value, int16_t max_value);

  void Predict(const CodedBlockBase& cb, Channel channel, bool split_tf,
               uint32_t tf_i, int16_t output[], uint32_t step) const override;

  bool DependsOnLuma() const override { return true; }

  std::string GetName() const override;
  std::string GetFakePredStr() const override;
  std::string GetPredStr(const CodedBlockBase& cb, Channel channel,
                         bool split_tf, uint32_t tf_i) const override;

  // Does a simple linear regression based on the context for chroma and luma,
  // assuming chroma = a * luma + b. Returns values in kCfLBits precision.
  void ContextLinearRegression(Channel channel, const CodedBlockBase& cb,
                               int32_t* a, int32_t* b) const;
  // Does a simple linear regression based on the reconstructed luma and input
  // chroma, assuming chroma = a * luma + b.
  // a and b are returned in kCfLBits precision.
  void BlockLinearRegression(Channel channel, const CodedBlock& cb, int32_t* a,
                             int32_t* b) const;

 protected:
  void LinearRegression(const int16_t* luma, const int16_t* chroma,
                        uint32_t size, int32_t* a, int32_t* b) const;

  virtual void DoPredict(const CodedBlockBase& cb, Channel channel,
                         uint32_t x_start, uint32_t y_start, uint32_t w,
                         uint32_t h, int16_t* output, uint32_t step) const;
  // for debug:
  void DisplaySamples(const CodedBlockBase& cb, Channel channel,
                      std::string* str) const;

 protected:
  // number of bits for represent the 'a' and 'b' parameters, with sign.
  // 'a' is in range [-4,4), 'b' is in range [-1024, 1023]
  static constexpr uint32_t kCflABits = kCflFracBits + 2 + 1;
  static constexpr uint32_t kCflBBits = kCflFracBits + kYuvMaxPrec + 2;
  static constexpr float kCflNorm = 1. / (1 << kCflFracBits);

  const int16_t min_value_;
  const int16_t max_value_;
};

// Like CflPredictor, but signals in the bitstream the difference between
// the predicted slope ('a' in 'chroma = a * chroma + b') and the actual one.
// More exact but obviously more costly.
class SignalingCflPredictor : public CflPredictor {
 public:
  SignalingCflPredictor(int16_t min_value, int16_t max_value);

  std::string GetName() const override;
  std::string GetPredStr(const CodedBlockBase& cb, Channel channel,
                         bool split_tf, uint32_t tf_i) const override;

  bool ComputeParams(CodedBlock* cb, Channel channel) const override;
  uint32_t WriteParams(const CodedBlockBase& cb, Channel channel,
                       SymbolManager* sm, ANSEncBase* enc) const override;
  uint32_t ReadParams(CodedBlockBase* cb, Channel channel, SymbolReader* sr,
                      ANSDec* dec) const override;

  bool DependsOnLuma() const override { return true; }

  // CodedBlockBase::cfl_[] params are in 1:2:6 fixed-point precision.
  constexpr static uint32_t kIOFracBits = 6;
  constexpr static uint32_t kIOBits = 9;  // including sign

 protected:
  void DoPredict(const CodedBlockBase& cb, Channel channel, uint32_t unused_x,
                 uint32_t unused_y, uint32_t w, uint32_t h, int16_t* output,
                 uint32_t step) const override;
  void GetParams(const CodedBlockBase& cb, Channel channel, uint32_t w,
                 uint32_t h, int32_t* a, int32_t* b) const;
};

//------------------------------------------------------------------------------

// Adds predictors for the alpha plane.
WP2Status InitAlphaPredictors(const CSPTransform& transform,
                              APredictors* preds);
// Total number of angle predictors.
static constexpr uint32_t kDirectionalPredNumYA =
    kAnglePredNum * (2 * kDirectionalMaxAngleDeltaYA + 1);
static constexpr uint32_t kDirectionalPredNumUV =
    kAnglePredNum * (2 * kDirectionalMaxAngleDeltaUV + 1);

// Number of prediction modes (8 'base' predictors + directional).
static constexpr uint32_t kYPredModeNum = kYBasePredNum + kAnglePredNum;
static constexpr uint32_t kAPredModeNum = kABasePredNum + kAnglePredNum;
static constexpr uint32_t kUVPredModeNum = kUVBasePredNum + kAnglePredNum;
// Number of predictors (number of modes and their sub-modes).
static constexpr uint32_t kYPredNum = kYBasePredNum + kDirectionalPredNumYA;
static constexpr uint32_t kAPredNum = kABasePredNum + kDirectionalPredNumYA;
static constexpr uint32_t kUVPredNum = kUVBasePredNum + kDirectionalPredNumUV;

}  // namespace WP2

#endif  // WP2_COMMON_LOSSY_PREDICTOR_H_
