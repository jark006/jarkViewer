// Copyright 2021 Google LLC
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
// Tools shared between AV1 encoder and decoder.

#ifndef WP2_COMMON_AV1_COMMON_H_
#define WP2_COMMON_AV1_COMMON_H_

#if defined(WP2_HAVE_AOM)
#include "aom/aom_codec.h"
#include "src/wp2/base.h"
#endif

namespace WP2 {

//------------------------------------------------------------------------------

// Small class to wrap a plain-C destruction call. Pointer is not freed.
template <typename TypeToDestruct, typename ReturnedType = void>
class Cleaner {
 public:
  Cleaner(TypeToDestruct* obj, ReturnedType (*dtor)(TypeToDestruct*))
      : obj_(obj), dtor_(dtor) {}
  ~Cleaner() { Destruct(); }
  ReturnedType Destruct() {
    TypeToDestruct* const obj = obj_;
    obj_ = nullptr;
    return (obj != nullptr) ? dtor_(obj) : ReturnedType();
  }

 private:
  TypeToDestruct* obj_;
  ReturnedType (*dtor_)(TypeToDestruct*);
};

#if defined(WP2_HAVE_AOM)
// Maps AV1 error codes to WP2 enums.
static WP2Status MapAomStatus(aom_codec_err_t aom_status) {
  switch (aom_status) {
    case AOM_CODEC_OK:
      return WP2_STATUS_OK;
    case AOM_CODEC_ERROR:
      return WP2_STATUS_INVALID_CONFIGURATION;  // No good equivalent.
    case AOM_CODEC_MEM_ERROR:
      return WP2_STATUS_OUT_OF_MEMORY;
    case AOM_CODEC_ABI_MISMATCH:
      return WP2_STATUS_VERSION_MISMATCH;
    case AOM_CODEC_INCAPABLE:
      return WP2_STATUS_UNSUPPORTED_FEATURE;
    case AOM_CODEC_UNSUP_BITSTREAM:
      return WP2_STATUS_BITSTREAM_ERROR;
    case AOM_CODEC_UNSUP_FEATURE:
      return WP2_STATUS_UNSUPPORTED_FEATURE;
    case AOM_CODEC_CORRUPT_FRAME:
      return WP2_STATUS_BITSTREAM_ERROR;
    case AOM_CODEC_INVALID_PARAM:
      return WP2_STATUS_INVALID_PARAMETER;
    default:
      return WP2_STATUS_LAST;
  }
}
#endif  // defined(WP2_HAVE_AOM)

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_COMMON_AV1_COMMON_H_
