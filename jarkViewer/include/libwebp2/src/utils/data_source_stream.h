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
// StreamDataSource definition.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_UTILS_DATA_SOURCE_STREAM_H_
#define WP2_UTILS_DATA_SOURCE_STREAM_H_

#include <cstddef>
#include <cstdint>

#include "src/utils/data_source.h"
#include "src/utils/vector.h"
#include "src/wp2/base.h"

namespace WP2 {

// DataSource used as a buffer to store the most recent bytes of a stream, not
// yet used and needed for the next function call.
class StreamDataSource : public DataSource {
 public:
  // Append the stream's current 'bytes' to this DataSource. Data may be copied
  // internally to create a contiguous array with previously stored data.
  WP2Status AppendAsExternal(const uint8_t* bytes, size_t size);

  // Copy all remaining referenced external data to the internal buffer.
  WP2Status AppendExternalToInternal();

  void Reset() override;

 private:
  bool Fetch(size_t num_requested_bytes) override;  // DataSource requirements.
  void OnDiscard(size_t num_bytes) override;

  const uint8_t* external_bytes_ = nullptr;
  size_t num_external_bytes_ = 0;

  Vector_u8 internal_data_;

  size_t num_new_bytes_to_discard_ = 0;
};

}  // namespace WP2

#endif  // WP2_UTILS_DATA_SOURCE_STREAM_H_
