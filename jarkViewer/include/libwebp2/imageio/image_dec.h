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
//  All-in-one library to decode PNG/JPEG/WEBP/WP2/TIFF/WIC input images.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_IMAGEIO_IMAGE_DEC_H_
#define WP2_IMAGEIO_IMAGE_DEC_H_

#include <cstddef>
#include <cstdint>

#include "imageio/file_format.h"
#include "imageio/imageio_util.h"
#include "src/wp2/base.h"

#ifdef HAVE_CONFIG_H
#include "src/wp2/config.h"
#endif

namespace WP2 {

// Tries to infer the image format. 'data_size' should be at least 12.
// Returns UNSUPPORTED if format cannot be guessed safely.
FileFormat GuessImageFormat(const uint8_t* data, size_t data_size);

// Decodes 'data' into 'buffer'. Image type (png, jpg etc.) is guessed from the
// bitstream if 'format' is AUTO.
// Returns WP2_STATUS_OK upon success.
WP2Status ReadImage(const uint8_t* data, size_t data_size, ArgbBuffer* buffer,
                    FileFormat format = FileFormat::AUTO,
                    LogLevel log_level = LogLevel::DEFAULT);

// Reads the file at 'file_path' and decodes it into 'buffer'.
// If 'file_size' is not null, the file's original size is returned.
// Image type (png, jpg etc.) is guessed from the bitstream if 'format' is AUTO.
// Returns WP2_STATUS_OK upon success.
WP2Status ReadImage(const char* file_path, ArgbBuffer* buffer,
                    size_t* file_size = nullptr,
                    FileFormat format = FileFormat::AUTO,
                    LogLevel log_level = LogLevel::DEFAULT);

}  // namespace WP2

#endif  // WP2_IMAGEIO_IMAGE_DEC_H_
