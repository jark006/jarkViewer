// Copyright 2022 Google LLC
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
// Defines a hash map for LZW-like algorithm.
//
// Author: Felipe Mourad Pereira (mouradfelipe@google.com)

#ifndef WP2_UTILS_HASH_MAP_H_
#define WP2_UTILS_HASH_MAP_H_

#include <algorithm>
#include <cstdint>

#include "src/enc/lossless/palette.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

// Each ColorSegment is an array of int16_t's.
// Each group of 4 int16_t's describes a pixel.
struct ColorSegment {
  bool operator==(const ColorSegment& a) const {
    return (num_pixels == a.num_pixels &&
            std::equal(data, data + 4 * num_pixels, a.data));
  }
  const int16_t* data;
  uint32_t num_pixels;  // number of pixels (not int16_t's)
};

constexpr uint32_t kClearCode = 0;

namespace WP2 {

// SegmentMap maps segment of pixels (key) to an index in an internal vector
// (value). Used for LZW compression algorithm.
class SegmentMap {
 public:
  explicit SegmentMap(uint32_t num_segments);

  WP2Status Allocate();

  // Initializes the cache with the unique colors of the input image.
  WP2Status InsertColors(const WP2L::Palette& palette);

  // Clears the cache (hash_map_) and segment vector (segments_).
  void Clear();

  // Adds segment only if the segment is new in cache,
  // and returns index of corresponding segment in segment vector.
  WP2Status FindOrAdd(const ColorSegment& segment, uint32_t& index);

  // Searches for segment in the cache and returns its index in segment vector.
  // Returns false if segment is not found in cache.
  bool HasKey(const ColorSegment& segment, uint32_t& index) const;

  uint32_t MaxCode() const { return segments_.size() - 1; }

 private:
  // hash_map_ size is bigger than segments_, because it is desired to
  // reduce hash collision. For now, it will be at least 2 times bigger than
  // segments_capacity_. hash_map_ size is defined by num_bits_ in Allocate().
  const uint32_t segments_capacity_, num_bits_;

  Vector<ColorSegment> segments_;
  Vector<uint32_t> hash_map_;

  uint32_t num_colors_;  // number of unique colors in the image
};

// SegmentCache stores segments similarly to SegmentMap, but overwrites the
// previous value when a hash collision happens.
class SegmentCache {
 public:
  WP2Status Allocate(uint32_t num_bits);

  // Searches for segment in the cache. Returns index in cache if found.
  bool HasKey(const ColorSegment& segment, uint32_t* index) const;

  // Inserts segment in the cache. Returns false if segment is already in
  // the cache. Returns segment index in cache.
  bool Insert(const ColorSegment& segment, uint32_t* index);

  // Returns a segment from a given cache index.
  void Lookup(uint32_t index, ColorSegment* segment) const;

  uint32_t IndexRange() const { return segments_.size(); }

 private:
  uint32_t num_bits_;
  Vector<ColorSegment> segments_;
};
}  // namespace WP2

#endif  // WP2_UTILS_HASH_MAP_H_
