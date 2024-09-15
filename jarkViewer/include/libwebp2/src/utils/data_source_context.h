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
// SuspendableDataSource definition.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_UTILS_DATA_SOURCE_CONTEXT_H_
#define WP2_UTILS_DATA_SOURCE_CONTEXT_H_

#include <cstdio>

#include "src/utils/context_switch.h"
#include "src/utils/data_source.h"

#if defined(WP2_USE_CONTEXT_SWITCH) && (WP2_USE_CONTEXT_SWITCH > 0)

namespace WP2 {

// Data source that will suspend and wait for resuming when Fetch() is called
// (occurs when TryGetNext() is missing data), if a LocalContext is set.
class SuspendableDataSource : public ExternalDataSource {
 public:
  void SetContext(LocalContext* context);
  bool HasEnoughDataToResume() const;

  void Reset() override;

 private:
  bool Fetch(size_t num_requested_bytes) override;

  LocalContext* context_ = nullptr;
  size_t num_requested_bytes_ = 0;
};

}  // namespace WP2

#endif  // WP2_USE_CONTEXT_SWITCH

#endif /* WP2_UTILS_DATA_SOURCE_CONTEXT_H_ */
