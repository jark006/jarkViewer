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
//  Preview encoder
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_ENC_PREVIEW_PREVIEW_ENC_H_
#define WP2_ENC_PREVIEW_PREVIEW_ENC_H_

#include <cassert>
#include <cstdint>

#include "src/common/color_precision.h"
#include "src/common/preview/preview.h"
#include "src/common/progress_watcher.h"
#include "src/utils/csp.h"
#include "src/utils/plane.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

// 'Density^2' is the ratio of the number of source pixels versus number of
// grid points to use during optimization.
static constexpr float kPreviewMinGridDensity = 1.0f;
static constexpr float kPreviewMaxGridDensity = 4.0f;

static constexpr uint32_t kPreviewMinBlurRadius = 0;
static constexpr uint32_t kPreviewMaxBlurRadius = 4;
static constexpr uint32_t kPreviewMinBlurThreshold = 1;
static constexpr uint32_t kPreviewMaxBlurThreshold = 256;
static constexpr uint32_t kPreviewMinNumIterations = 0;
static constexpr uint32_t kPreviewMaxNumIterations = 1u << 16;

//------------------------------------------------------------------------------

struct PreviewConfig {
 public:
  // Available algorithms for the initial vertices generation step.
  enum class AnalysisMethod {
    kEdgeSelectionAndRepulsion,
    kColorDiffMaximization,
    kRandomEdgeSelection,
    kVoronoi,
  };

  PreviewConfig() { assert(IsValid()); }  // Check default values.
  // Get settings matching 'quality' and 'effort' (see EncoderConfig).
  PreviewConfig(float quality, int effort);

  // Returns true if all parameters are within their valid ranges.
  bool IsValid() const;

 public:
  // See preview.h for valid ranges.
  // The following settings give better results for higher values but longer
  // encoding, decoding time and bigger bitstream size.
  uint32_t num_vertices = 200;  // starting number of vertices
  uint32_t num_colors = 6;      // starting number of colors
  uint32_t grid_size = 64;      // grid size in the longest dimension

  // The following settings give better results for higher values but longer
  // encoding time.
  float grid_density = 1.2f;     // ratio canvas / grid
  uint32_t blur_radius = 4;      // Preprocessing blur kernel size (0=disabled).
  uint32_t blur_threshold = 30;  // Preprocessing blur threshold.
  uint32_t num_iterations = 10;  // Postprocessing optimization (0=disabled).
  AnalysisMethod analysis_method = AnalysisMethod::kColorDiffMaximization;

  // Postprocessing optimization parameters.
  uint32_t num_mutations_per_iteration = 1;
  float score_tolerance = 0.0002f;
  uint32_t target_num_bytes = 200;
  float importance_of_size_over_quality = 1.f;
  uint32_t optimize_color_indices_every_n_iterations = 10;
  uint32_t GetMaxNumConsecutiveSolutionsWithoutImprovement() const {
    return (num_iterations + 5) / 6;
  }

  // Probabilities for local moves (in %) during postprocessing optimization.
  uint8_t proba_vertex_move = 50;
  uint8_t proba_vertex_add = 20;
  uint8_t proba_vertex_sub = 25;
  uint8_t proba_color_index_move = 25;
  uint8_t proba_color_move = 20;
  uint8_t proba_color_add = 1;
  uint8_t proba_color_sub = 3;
};

// Compresses 'buffer' into a preview and writes it to 'output'.
// Returns WP2_STATUS_OK or an error.
WP2Status EncodePreview(const ArgbBuffer& buffer, const PreviewConfig& config,
                        const ProgressRange& progress, Writer* output);

//------------------------------------------------------------------------------
// Internal vertex collection

// Places a vertex on the hardest edge, repulses following ones from it,
// repeats 'max_num_vertices' times.
WP2Status CollectVerticesFromEdgeSelectionAndRepulsion(
    const ArgbBuffer& canvas, uint32_t max_num_vertices, PreviewData* preview);

// Places a point where it maximizes color difference with current canvas,
// updates canvas, repeats 'max_num_vertices' times.
WP2Status CollectVerticesFromColorDiffMaximization(const ArgbBuffer& canvas,
                                                   uint32_t max_num_vertices,
                                                   PreviewData* preview);

// Takes a list of at most twice 'max_num_vertices' of the most edgy,
// non-isolated pixels, and draws 'max_num_vertices' at random from it.
WP2Status CollectVerticesFromRandomEdgeSelection(const ArgbBuffer& canvas,
                                                 uint32_t max_num_vertices,
                                                 PreviewData* preview);

// Use Voronoi cells to spread vertices with equal-density.
WP2Status CollectVerticesFromVoronoiCells(const ArgbBuffer& canvas,
                                          uint32_t max_num_vertices,
                                          PreviewData* preview);

//------------------------------------------------------------------------------
// Internal color collection

// Averages pixel color with its neighbors in a '2*radius+1'-sided square.
Argb32b GetAverageColor(const ArgbBuffer& canvas, uint32_t radius,
                        uint32_t pixel_x, uint32_t pixel_y);

// Returns 'counts' with the number of 'vertices' using each 'palette' color.
WP2Status GetPaletteHistogram(const Vector<VertexIndexedColor>& vertices,
                              const Vector<AYCoCg19b>& palette,
                              Vector_u16* counts);

// Returns the index of the 'palette' element matching the 'color' the most.
uint16_t FindClosestColorIndex(const Vector<AYCoCg19b>& palette,
                               AYCoCg19b color);

// Make sure the palette has enough color, and insert a default colormap if
// needed.
WP2Status MakePaletteValid(Vector<AYCoCg19b>* palette);

//------------------------------------------------------------------------------

// Averages all pixels.
RGB12b GetPreviewColor(const ArgbBuffer& canvas);
RGB12b GetPreviewColor(const YUVPlane& canvas,
                       const CSPTransform& csp_transform);
RGB12b GetPreviewColor(const YUVPlane& canvas, const CSPMtx& ccsp_to_rgb);

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_ENC_PREVIEW_PREVIEW_ENC_H_
