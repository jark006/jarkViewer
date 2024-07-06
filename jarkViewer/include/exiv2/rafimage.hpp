// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef RAFIMAGE_HPP_
#define RAFIMAGE_HPP_

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
  @brief Class to access raw Fujifilm RAF images. Exif metadata is
      supported directly, IPTC is read from the Exif data, if present.
 */
class EXIV2API RafImage : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing RAF image or create
        a new image from scratch. If a new image is to be created, any
        existing data is overwritten. Since the constructor can not return
        a result, callers should check the good() method after object
        construction to determine success or failure.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method.  Use the Image::io()
        method to get a temporary reference.
    @param create Specifies if an existing image should be read (false)
        or if a new file should be created (true).
   */
  RafImage(BasicIo::UniquePtr io, bool create);
  //@}

  //! @name Manipulators
  //@{
  void printStructure(std::ostream& out, PrintStructureOption option, size_t depth) override;
  void readMetadata() override;
  /*!
    @brief Todo: Write metadata back to the image. This method is not
        yet implemented. Calling it will throw an Error(ErrorCode::kerWritingImageFormatUnsupported).
   */
  void writeMetadata() override;
  /*!
    @brief Todo: Not supported yet, requires writeMetadata(). Calling
        this function will throw an Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setExifData(const ExifData& exifData) override;
  /*!
    @brief Todo: Not supported yet, requires writeMetadata(). Calling
        this function will throw an Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setIptcData(const IptcData& iptcData) override;
  /*!
    @brief Not supported. RAF format does not contain a comment.
        Calling this function will throw an Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setComment(const std::string&) override;
  //@}

  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override;
  [[nodiscard]] uint32_t pixelWidth() const override;
  [[nodiscard]] uint32_t pixelHeight() const override;
  //@}
};  // class RafImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new RafImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newRafInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a RAF image.
EXIV2API bool isRafType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef RAFIMAGE_HPP_
