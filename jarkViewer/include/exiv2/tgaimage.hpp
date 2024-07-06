// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TGAIMAGE_HPP_
#define TGAIMAGE_HPP_

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
  @brief Class to access raw TARGA images. This is just a stub - we only
      read width and height.
 */
class EXIV2API TgaImage : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor to open a Targa image. Since the
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
  explicit TgaImage(BasicIo::UniquePtr io);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  /*!
    @brief Todo: Write metadata back to the image. This method is not
        yet(?) implemented. Calling it will throw an Error(ErrorCode::kerWritingImageFormatUnsupported).
   */
  void writeMetadata() override;
  /*!
    @brief Todo: Not supported yet(?). Calling this function will throw
        an instance of Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setExifData(const ExifData& exifData) override;
  /*!
    @brief Todo: Not supported yet(?). Calling this function will throw
        an instance of Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setIptcData(const IptcData& iptcData) override;
  /*!
    @brief Not supported. Calling this function will throw an instance
        of Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setComment(const std::string&) override;
  //@}

  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override;
  //@}

};  // class TgaImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new TgaImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newTgaInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a Targa v2 image.
EXIV2API bool isTgaType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef TGAIMAGE_HPP_
