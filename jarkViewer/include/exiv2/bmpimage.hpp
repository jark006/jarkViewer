// SPDX-License-Identifier: GPL-2.0-or-later

/*!
  @author  Marco Piovanelli, Ovolab (marco)
           <a href="mailto:marco.piovanelli@pobox.com">marco.piovanelli@pobox.com</a>
  @date    05-Mar-2007, marco: created
 */
#ifndef BMPIMAGE_HPP_
#define BMPIMAGE_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "image.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
// *****************************************************************************
// class definitions

/*!
  @brief Class to access Windows bitmaps. This is just a stub - we only
      read width and height.
 */
class EXIV2API BmpImage : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor to open a Windows bitmap image. Since the
        constructor can not return a result, callers should check the
        good() method after object construction to determine success
        or failure.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method.  Use the Image::io()
        method to get a temporary reference.
   */
  explicit BmpImage(BasicIo::UniquePtr io);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;

  /// @throws Error(ErrorCode::kerWritingImageFormatUnsupported).
  void writeMetadata() override;

  /// @throws Error(ErrorCode::kerInvalidSettingForImage)
  void setExifData(const ExifData& exifData) override;

  /// @throws Error(ErrorCode::kerInvalidSettingForImage)
  void setIptcData(const IptcData& iptcData) override;

  /// @throws Error(ErrorCode::kerInvalidSettingForImage)
  void setComment(const std::string&) override;
  //@}

  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override;
  //@}
};  // class BmpImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new BmpImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newBmpInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a Windows Bitmap image.
EXIV2API bool isBmpType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef BMPIMAGE_HPP_
