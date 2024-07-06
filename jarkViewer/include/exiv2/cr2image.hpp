// SPDX-License-Identifier: GPL-2.0-or-later

/*!
  @author  Andreas Huggel (ahu)
           <a href="mailto:ahuggel@gmx.net">ahuggel@gmx.net</a>
  @date    22-Apr-06, ahu: created
 */
#ifndef CR2IMAGE_HPP_
#define CR2IMAGE_HPP_

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
  @brief Class to access raw Canon CR2 images.  Exif metadata
      is supported directly, IPTC is read from the Exif data, if present.
 */
class EXIV2API Cr2Image : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing CR2 image or create
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
  Cr2Image(BasicIo::UniquePtr io, bool create);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  void writeMetadata() override;
  /*!
    @brief Print out the structure of image file.
    @throw Error if reading of the file fails or the image data is
          not valid (does not look like data of the specific image type).
    @warning This function is not thread safe and intended for exiv2 -pS for debugging.
   */
  void printStructure(std::ostream& out, PrintStructureOption option, size_t depth) override;
  /*!
    @brief Not supported. CR2 format does not contain a comment.
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
};  // class Cr2Image

/*!
  @brief Stateless parser class for data in CR2 format. Images use this
         class to decode and encode CR2 data.
         See class TiffParser for details.
 */
class EXIV2API Cr2Parser {
 public:
  /*!
    @brief Decode metadata from a buffer \em pData of length \em size
           with data in CR2 format to the provided metadata containers.
           See TiffParser::decode().
  */
  static ByteOrder decode(ExifData& exifData, IptcData& iptcData, XmpData& xmpData, const byte* pData, size_t size);
  /*!
    @brief Encode metadata from the provided metadata to CR2 format.
           See TiffParser::encode().
  */
  static WriteMethod encode(BasicIo& io, const byte* pData, size_t size, ByteOrder byteOrder, ExifData& exifData,
                            IptcData& iptcData, XmpData& xmpData);

};  // class Cr2Parser

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new Cr2Image instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newCr2Instance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a CR2 image.
EXIV2API bool isCr2Type(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef CR2IMAGE_HPP_
