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
//  Utility functions used by the image decoders.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_IMAGEIO_IMAGEIO_UTIL_H_
#define WP2_IMAGEIO_IMAGEIO_UTIL_H_

#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

#include "src/wp2/base.h"

#if defined(WP2_HAVE_WEBP)
#include "webp/encode.h"
#endif

namespace WP2 {

// Standard output logging level.
//   QUIET should not print anything.
//   DEFAULT will print errors and requested information.
//   VERBOSE will also print warnings and all available information.
enum class LogLevel { QUIET = 0, DEFAULT = 1, VERBOSE = 2 };

//------------------------------------------------------------------------------
// File I/O

// Reopen file in binary (O_BINARY) mode.
// Returns 'file' on success, NULL otherwise.
FILE* IoUtilSetBinaryMode(FILE* file);

// Allocates storage for entire file 'file_path' and returns contents and size
// in 'data'. Returns WP2_STATUS_OK upon success.
// If 'file_path' is NULL or equal to "-", input is read from stdin by calling
// the function IoUtilReadFromStdin(). 'max_num_bytes' is the maximum number of
// bytes allocated and read unless it is 0.
WP2Status IoUtilReadFile(const char* file_path, Data* data,
                         size_t max_num_bytes = 0);

// Same as IoUtilReadFile(), but reads until EOF from stdin instead.
WP2Status IoUtilReadFromStdin(Data* data, size_t max_num_bytes = 0);

// Write a data segment into a file named 'file_path'.
// Returns WP2_STATUS_OK upon success. Returns written 'file_size' if not null.
// If 'file_path' is NULL or equal to "-", output is written to stdout.
WP2Status IoUtilWriteFile(const uint8_t* data, size_t data_size,
                          const char* file_path, bool overwrite = false,
                          size_t* file_size = nullptr);

// returns the size of file 'file_path', or an error (directory,
// non-existing file, non readable, etc.)
WP2Status IoUtilFileSize(const char* file_path, size_t* size);

// Returns true if the file can be opened in "append/binary" mode.
bool FileExists(const char* file_path);

//------------------------------------------------------------------------------
// string variants

WP2Status IoUtilReadFile(const char* file_path, std::string* data);
WP2Status IoUtilReadFromStdin(std::string* data);
WP2Status IoUtilWriteFile(const std::string& data, const char* file_path,
                          bool overwrite = false);

//------------------------------------------------------------------------------

// Checks that 'mult' times 'value' fit in 'ResultType', then in that case sets
// 'result' if not null. Any combination of signed and unsigned integer types is
// allowed but all values must be positive or zero.
template <typename ResultType, typename ValueType, typename MultType>
WP2Status MultFitsIn(ValueType value, MultType mult,
                     ResultType* result = nullptr) {
  // Forbid any negative value now to make the check below easier.
  if (value < 0 || mult < 0) return WP2_STATUS_BAD_DIMENSION;

  // Verify that both inputs fit in ResultType without overflow.
  const ResultType cast_value = (ResultType)value;
  if (cast_value < 0 || (ValueType)cast_value != value) {
    return WP2_STATUS_BAD_DIMENSION;
  }
  const ResultType cast_mult = (ResultType)mult;
  if (cast_mult < 0 || (MultType)cast_mult != mult) {
    return WP2_STATUS_BAD_DIMENSION;
  }

  // Check that the multiplication will fit in ResultType by verifying that
  // 'cast_value' is not bigger than 'cast_max' / 'cast_mult'.
  // This prevents signed integer overflow undefined behavior.
  if (cast_mult > 0 &&
      cast_value > std::numeric_limits<ResultType>::max() / cast_mult) {
    return WP2_STATUS_BAD_DIMENSION;
  }

  // Set only on success.
  if (result != nullptr) *result = cast_value * cast_mult;
  return WP2_STATUS_OK;
}

//------------------------------------------------------------------------------
// Conversion to and from WebP picture struct. Metadata is ignored.

#if defined(WP2_HAVE_WEBP)
WP2Status ConvertToWebPPicture(const WP2::ArgbBuffer& in, WebPPicture* out,
                               bool use_yuv = false);
WP2Status ConvertFromWebPPicture(const WebPPicture& in, WP2::ArgbBuffer* out);
#endif

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_IMAGEIO_IMAGEIO_UTIL_H_
