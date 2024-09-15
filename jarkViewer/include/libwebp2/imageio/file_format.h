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
//  File format definitions.

#ifndef WP2_IMAGEIO_FILE_FORMAT_H_
#define WP2_IMAGEIO_FILE_FORMAT_H_

#ifdef HAVE_CONFIG_H
#include "src/wp2/config.h"
#endif

namespace WP2 {

enum class FileFormat {
  PNG = 0,
  JPEG,
  WEBP,
  WP2,
  TIFF,
  GIF,
  BMP,
  PAM,
  PGM,  // Y/U/V(/A) planes side-by-side
  PPM,
  Y4M_420,  // Subsampled chroma (Cb and Cr planes are half width, half height).
  Y4M_444,  // Full resolution for all Y, Cb and Cr planes. No alpha.
  YUV_420,  // Raw: y4m 420 with no header.
  JPEG_XL,
  AUTO,  // Detect format automatically (by parsing file extension or header).
  UNSUPPORTED
};

const char* GetFileFormatStr(FileFormat file_format);

// Returns UNSUPPORTED if no format matching 'file_path' extension is found.
FileFormat GetFormatFromExtension(const char* file_path);

// Returns a lowercase extension or nullptr if 'format' is unhandled.
const char* GetExtensionFromFormat(FileFormat format);

// Returns true if the 'file_format' is a custom color space (not RGB).
bool IsCustomColorSpace(FileFormat file_format);
// Returns true if the 'file_format' has downscaled chroma planes (4:2:0).
bool IsChromaSubsampled(FileFormat file_format);

}  // namespace WP2

#endif  // WP2_IMAGEIO_FILE_FORMAT_H_
