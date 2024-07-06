// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef RW2IMAGE_HPP_
#define RW2IMAGE_HPP_

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
  @brief Class to access raw Panasonic RW2 images.  Exif metadata is
      supported directly, IPTC and XMP are read from the Exif data, if
      present.
 */
class EXIV2API Rw2Image : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor to open an existing RW2 image. Since the
        constructor can not return a result, callers should check the
        good() method after object construction to determine success or
        failure.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method.  Use the Image::io()
        method to get a temporary reference.
   */
  explicit Rw2Image(BasicIo::UniquePtr io);
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
    @brief Not supported. RW2 format does not contain a comment.
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
};  // class Rw2Image

/*!
  @brief Stateless parser class for data in RW2 format. Images use this
         class to decode and encode RW2 data. Only decoding is currently
         implemented. See class TiffParser for details.
 */
class EXIV2API Rw2Parser {
 public:
  /*!
    @brief Decode metadata from a buffer \em pData of length \em size
           with data in RW2 format to the provided metadata containers.
           See TiffParser::decode().
  */
  static ByteOrder decode(ExifData& exifData, IptcData& iptcData, XmpData& xmpData, const byte* pData, size_t size);

};  // class Rw2Parser

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new Rw2Image instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newRw2Instance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a RW2 image.
EXIV2API bool isRw2Type(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef RW2IMAGE_HPP_
