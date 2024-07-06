// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef IMAGE_TYPES_H
#define IMAGE_TYPES_H

namespace Exiv2 {
/// Supported Image Formats
enum class ImageType {
  none,
  arw,
  asf,
  bigtiff,
  bmff,
  bmp,  ///< Windows bitmap
  cr2,
  crw,
  dng,
  eps,
  exv,
  gif,  ///< GIF
  jp2,  ///< JPEG-2000
  jpeg,
  mrw,
  nef,
  orf,
  pef,
  png,
  pgf,
  psd,  ///< Photoshop (PSD)
  raf,
  rw2,
  sr2,
  srw,
  tga,
  tiff,
  webp,
  xmp,  ///< XMP sidecar files
  qtime,
  riff,
  mkv,
};
}  // namespace Exiv2

#endif  // IMAGE_TYPES_H
