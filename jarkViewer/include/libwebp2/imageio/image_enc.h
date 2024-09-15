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
//  All-in-one library to save PNG/JPEG/WP2/TIFF/WIC images.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_IMAGEIO_IMAGE_ENC_H_
#define WP2_IMAGEIO_IMAGE_ENC_H_

#include <cstdio>

#ifdef HAVE_CONFIG_H
#include "src/wp2/config.h"
#endif

#include "imageio/file_format.h"
#include "src/utils/csp.h"
#include "src/wp2/base.h"
#ifdef WP2_HAVE_WEBP
#include "webp/encode.h"
#endif

namespace WP2 {

// Losslessly saves 'buffer' to 'file_path'. The 'format' can be specified or it
// will be deduced from 'file_path' extension. If 'file_path' is "-", data is
// saved to stdout. Returns WP2_STATUS_OK upon success.
WP2Status SaveImage(const ArgbBuffer& buffer, const char* file_path,
                    bool overwrite = false,
                    FileFormat format = FileFormat::AUTO,
                    size_t* file_size = nullptr,
                    const CSPTransform& transform = CSPTransform() /*=kYCoCg*/);

// Encode 'buffer' to 'fout'.
// WritePNG() might need other arguments than 'fout' if HAVE_WINCODEC_H.
WP2Status WritePNG(const ArgbBuffer& buffer, FILE* fout, const char* file_path,
                   bool use_stdout, bool overwrite, size_t* file_size);
WP2Status WriteTIFF(const ArgbBuffer& buffer, FILE* fout);
WP2Status WriteBMP(const ArgbBuffer& buffer, FILE* fout);
WP2Status WritePPM(const ArgbBuffer& buffer, FILE* fout);
WP2Status WritePAM(const ArgbBuffer& buffer, FILE* fout);
WP2Status WritePGM(const ArgbBuffer& buffer, FILE* fout,
                   const CSPTransform& transform = CSPTransform() /*=kYCoCg*/);

#ifdef WP2_HAVE_WEBP
// Encodes 'buffer' as a WebP image with a given 'config' to 'output'.
WP2Status CompressWebP(const ArgbBuffer& buffer, const WebPConfig& config,
                       Writer* output);
#endif
// Same as above but maps wp2 'quality' and 'effort' to a WebPConfig.
// Returns WP2_STATUS_UNSUPPORTED_FEATURE unless compiled with WP2_HAVE_WEBP.
WP2Status CompressWebP(const ArgbBuffer& buffer, float quality, int effort,
                       Writer* output);
WP2Status CompressJXL(const ArgbBuffer& buffer, float quality, int effort,
                      Writer* output);

}  // namespace WP2

#endif  // WP2_IMAGEIO_IMAGE_ENC_H_
