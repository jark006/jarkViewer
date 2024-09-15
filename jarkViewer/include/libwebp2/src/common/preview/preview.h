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
//  Preview triangulation and rasterizer
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_COMMON_PREVIEW_PREVIEW_H_
#define WP2_COMMON_PREVIEW_PREVIEW_H_

#include <cstdint>

#include "src/common/color_precision.h"
#include "src/common/progress_watcher.h"
#include "src/utils/ans.h"
#include "src/utils/ans_enc.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

// Not counting the extra four corner vertices.
static constexpr uint32_t kPreviewMinNumVertices = 0;
static constexpr uint32_t kPreviewMaxNumVertices = 1024;

static constexpr uint32_t kPreviewMinNumColors = 2;
static constexpr uint32_t kPreviewMaxNumColors = 32;

static constexpr uint32_t kPreviewMinGridSize = 2;
static constexpr uint32_t kPreviewMaxGridSize = 256;

// Probas out of kPreviewTotalProba.
static constexpr uint32_t kPreviewOpaqueProba = 3;
static constexpr uint32_t kPreviewNoiseProba = 2;
static constexpr uint32_t kPreviewTotalProba = 4;

// 'cg' must be 0 because the palette is sorted by 'cg' before encoding.
static constexpr AYCoCg19b kPreviewStartingColorPrediction = {
    true, AYCoCg19b::kYCoCgMax >> 1, AYCoCg19b::kYCoCgMax >> 1, 0};

// Fwd dcl.
class UniformIntDistribution;
struct PreviewConfig;
class PreviewData;

//------------------------------------------------------------------------------

// Used for computation and storage.
struct VertexIndexedColor {
  uint16_t x, y;
  uint16_t color_index;

  bool operator<(const VertexIndexedColor& other) const {
    return (y < other.y) || (y == other.y && x < other.x);
  }
};

//------------------------------------------------------------------------------
// Triangulation

struct Circle {
  int64_t center_x, center_y;
  int64_t i_s;            // signed inner surface * 4
  int64_t square_radius;  // radius * radius

  bool Contains(int64_t x, int64_t y) const;
};

// Type for vertex index. If negative, refers to fixed corners.
typedef int16_t vtx_t;
struct Triangle {
  vtx_t v0, v1, v2;  // Vertex indices.
  Circle circumcircle;
};

// Structure storing Delaunay triangulation deduced from input vertices.
class Triangulation {
 public:
  // Creates the Delaunay triangulation.
  WP2Status Create(const PreviewData& data);

  // Adapts the 'triangles_' to include the new vertex
  // (expected to be at 'vertices[new_vertex_index]').
  WP2Status Insert(const Vector<VertexIndexedColor>& vertices,
                   vtx_t new_vertex_index);

  // The triangulation.
  const Vector<Triangle>& GetTriangles() const { return triangles_; }

  // Drawing calls.
  WP2Status FillTriangles(const PreviewData& preview, ArgbBuffer* canvas,
                          uint32_t* num_pixels) const;

  WP2Status GetLoss(const PreviewData& preview, const ArgbBuffer& reference,
                    float* loss, uint32_t* num_pixels) const;
  WP2Status GetPartialLoss(const PreviewData& preview,
                           const ArgbBuffer& reference,
                           const Vector<Triangle>& triangles, float* loss,
                           uint32_t* num_pixels) const;
  // For a given triangle, returns its vertices' colors that would best
  // approximate the content.
  WP2Status GetBestColors(const PreviewData& preview,
                          const ArgbBuffer& reference, const Triangle& triangle,
                          Argb32b colors[3]) const;

 protected:
  // sets up the two starting 'triangles_'.
  WP2Status InitCorners(const PreviewData& data);

  // returns vertex data for index, including corner index < 0
  VertexIndexedColor GetVertex(const Vector<VertexIndexedColor>& vertices,
                               vtx_t index) const;
  Vector<Triangle> triangles_;
  VertexIndexedColor corners_[4];  // precalc'd by InitCorners()
};

//------------------------------------------------------------------------------

// ANS stats used during encoding and decoding.
struct ValueStats {
  ANSBinSymbol zero;
  ANSBinSymbol sign;
  ANSBinSymbol bits[AYCoCg19b::kYCoCgBitDepth];
};

//------------------------------------------------------------------------------
// Main structure for storing parameters

class PreviewData {
 public:
  void Reset();

  WP2Status Decode(ANSDec* dec);
  WP2Status Decode(const uint8_t data[], uint32_t size);

  WP2Status Encode(ANSEnc* enc) const;

 public:  // encoder-size generation / optimization of the preview
  // returns false in case of memory allocation failure
  bool CopyFrom(const PreviewData& other);

  // public entry for encoding / optimizing
  WP2Status Generate(const ArgbBuffer& canvas, const PreviewConfig& config,
                     const ProgressRange& progress);

  // Tries to increase the quality and the size of the preview by randomly
  // mutating the data.
  WP2Status Optimize(const ArgbBuffer& canvas, const PreviewConfig& config,
                     const ProgressRange& progress, bool log = false);

 protected:  // decoding I/O
  WP2Status DecodePalette(ANSDec* dec);
  WP2Status DecodeVertices(ANSDec* dec);
  uint16_t DecodeColorIndex(ANSDec* dec, ANSBinSymbol* stats,
                            uint16_t* prediction);

 protected:  // encoding I/O
  WP2Status EncodePalette(ANSEnc* enc) const;
  WP2Status EncodeVertices(ANSEnc* enc) const;
  void EncodeColorIndex(uint16_t idx, ANSEnc* enc,
                        ANSBinSymbol* stats,
                        uint16_t* prediction) const;

 protected:  // encoding
  // Removes least used colors until palette_ contains at most 'max_num_colors'.
  WP2Status ReduceColors(const ArgbBuffer& canvas, uint32_t max_num_colors);

  // Computes grid width and height based on canvas dimensions and expected
  // grid size. Preserves aspect ratio as possible.
  WP2Status SetGridSize(const ArgbBuffer& canvas, uint32_t grid_size);

  // removes dups from vertices_[]
  WP2Status RemoveDuplicateVertices();

  // Fills palette_ with at most 'max_colors' taken from the pixels of
  // 'canvas' located at vertices_, which get assigned color indices.
  WP2Status CollectColors(const ArgbBuffer& canvas, uint32_t max_colors);
  // Alternative algorithm
  WP2Status CollectColors2(const ArgbBuffer& canvas, uint32_t max_colors);

  // Sort the palette_ by cg, remove duplicates and unused colors.
  WP2Status SortPalette();
  // Sort the vertices_ and verify them.
  WP2Status SortVertices();

  // Set color index, include corners if -4 <= vtx_idx < 0
  void SetColorIndex(int vtx_idx, uint32_t index);

  // Returns best index matching the canvas' colors around x,y (in grid units)
  uint16_t GetBestIndex(const ArgbBuffer& canvas, uint32_t x, uint32_t y) const;
  // Find closest matching color in palette_ for all vertices_.
  void AssignBestVertices(const ArgbBuffer& canvas);

 public:
  bool IsCorner(uint32_t x, uint32_t y) const {
    return (x == 0 || x == grid_width_ - 1) &&
           (y == 0 || y == grid_height_ - 1);
  }
  bool IsOutside(uint32_t x, uint32_t y) const {
    return (x >= grid_width_ || y >= grid_height_);
  }
  // Sort vertices, remove duplicates, outside, corner vertices.
  WP2Status CleanupVertices();

 public:
  uint32_t grid_width_ = 0, grid_height_ = 0;
  Vector<VertexIndexedColor> vertices_;
  // color index of corners (order: top-left,top-right,bottom-left,bottom-right)
  uint16_t corners_[4] = {0};
  Vector<AYCoCg19b> palette_;
  bool use_noise_ = false;
};

//------------------------------------------------------------------------------
// Derived class for encoding optimization

class PreviewDataEnc : public PreviewData {
 public:
  explicit PreviewDataEnc(const ArgbBuffer& canvas) : canvas_(canvas) {}

  WP2Status ImportFrom(const PreviewData& data);
  WP2Status ExportTo(PreviewData* data) const;
  void Print() const;

 public:
  // Encodes the data and computes a score based on the byte count and
  // the similarity with the canvas_.
  WP2Status GetScore(float bigger_size_penalty);
  // Assigns the color producing the smallest local loss to each vertex.
  WP2Status OptimizeColorIndices();

  // Returns true if a vertex is possibly duplicated.
  bool MoveVertex(UniformIntDistribution* rnd);

  // Vertex mutations.
  WP2Status AddVertex(UniformIntDistribution* rnd);
  void RemoveVertex(UniformIntDistribution* rnd);
  void MoveColorIndex(UniformIntDistribution* rnd);

  // Color mutations.
  WP2Status AddColor(UniformIntDistribution* rnd);
  WP2Status RemoveColor(UniformIntDistribution* rnd);
  void MoveColor(UniformIntDistribution* rnd);

  float Score() const { return score_; }
  uint32_t Size() const { return num_bytes_; }

 private:
  Triangulation triangulation_;
  const ArgbBuffer& canvas_;
  float score_;
  float triangulation_loss_;  // ~distortion
  uint32_t num_bytes_;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_COMMON_PREVIEW_PREVIEW_H_
