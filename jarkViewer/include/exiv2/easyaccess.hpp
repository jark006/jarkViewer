// SPDX-License-Identifier: GPL-2.0-or-later

/*!
  @brief   Provides easy (high-level) access to some Exif meta data.<BR>
           Please keep in mind that these accessors are provided for convenience only and will return expected/desired
  metadata most of the time. The accessors check several Exif tags and/or MakerNote fields, but in rare cases this list
  of fields checked may not be complete (e.g. if a maker writes a new tag) and then nothing is returned. So there might
  be specific cases where you will want to use lower level Exiv2 API functions to access targeted Exif tags and/or
  MakerNote fields for more specific processing.<BR>
           Please note also that lens name function uses some heuristics to reconstruct the lens name, which might
  involve combinig information from multiple tags and interpreting numeric codes. While the heuristics try to cover most
  cases, they can't be perfect and might fail in some circumstances, leading to imprecise results.
  @author  Carsten Pfeiffer <pfeiffer@kde.org>
  @date    28-Feb-09, gis: created
 */
#ifndef EASYACCESS_HPP_
#define EASYACCESS_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "exif.hpp"

namespace Exiv2 {
// *****************************************************************************
// class declarations
class ExifData;

//! Return the orientation of the image. Please keep in mind that this accessor is provided for convenience only and
//! will return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator orientation(const ExifData& ed);
//! Return the ISO speed used to shoot the image. Please keep in mind that this accessor is provided for convenience
//! only and will return expected/desired metadata most of the time. Please note also that ISO speed function uses some
//! computations, which might involve combining information from multiple tags and interpreting numeric codes. While the
//! computations try to cover most cases, they can't be perfect and might fail in some circumstances, leading to
//! imprecise results.
EXIV2API ExifData::const_iterator isoSpeed(const ExifData& ed);
//! Return the date and time when the original image data was generated. Please keep in mind that this accessor is
//! provided for convenience only and will return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator dateTimeOriginal(const ExifData& ed);
//! Return the flash bias value. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator flashBias(const ExifData& ed);
//! Return the exposure mode setting. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator exposureMode(const ExifData& ed);
//! Return the scene mode setting. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator sceneMode(const ExifData& ed);
//! Return the macro mode setting. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator macroMode(const ExifData& ed);
//! Return the image quality setting. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator imageQuality(const ExifData& ed);
//! Return the white balance setting. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator whiteBalance(const ExifData& ed);
//! Return the name of the lens used. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time. Please note also that lens name function uses some heuristics to
//! reconstruct the lens name, which might involve combining information from multiple tags and interpreting numeric
//! codes. While the heuristics try to cover most cases, they can't be perfect and might fail in some circumstances,
//! leading to imprecise results.
EXIV2API ExifData::const_iterator lensName(const ExifData& ed);
//! Return the saturation level. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator saturation(const ExifData& ed);
//! Return the sharpness level. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator sharpness(const ExifData& ed);
//! Return the contrast level. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator contrast(const ExifData& ed);
//! Return the scene capture type. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator sceneCaptureType(const ExifData& ed);
//! Return the metering mode setting. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator meteringMode(const ExifData& ed);
//! Return the camera make. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator make(const ExifData& ed);
//! Return the camera model. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator model(const ExifData& ed);
//! Return the exposure time. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator exposureTime(const ExifData& ed);
//! Return the F number. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator fNumber(const ExifData& ed);
//! Return the shutter speed value. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator shutterSpeedValue(const ExifData& ed);
//! Return the aperture value. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator apertureValue(const ExifData& ed);
//! Return the brightness value. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator brightnessValue(const ExifData& ed);
//! Return the exposure bias value. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator exposureBiasValue(const ExifData& ed);
//! Return the max aperture value. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator maxApertureValue(const ExifData& ed);
//! Return the subject distance. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator subjectDistance(const ExifData& ed);
//! Return the kind of light source. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator lightSource(const ExifData& ed);
//! Return the status of flash. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator flash(const ExifData& ed);
//! Return the camera serial number. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator serialNumber(const ExifData& ed);
//! Return the focal length setting. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator focalLength(const ExifData& ed);
//! Return the subject location and area. Please keep in mind that this accessor is provided for convenience only and
//! will return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator subjectArea(const ExifData& ed);
//! Return the flash energy. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator flashEnergy(const ExifData& ed);
//! Return the exposure index. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator exposureIndex(const ExifData& ed);
//! Return the image sensor type. Please keep in mind that this accessor is provided for convenience only and will
//! return expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator sensingMethod(const ExifData& ed);
//! Return the AF point. Please keep in mind that this accessor is provided for convenience only and will return
//! expected/desired metadata most of the time.
EXIV2API ExifData::const_iterator afPoint(const ExifData& ed);

}  // namespace Exiv2

#endif  // EASYACCESS_HPP_
