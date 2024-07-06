// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PSDIMAGE_HPP_
#define PSDIMAGE_HPP_

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
  @brief Class to access raw Photoshop images.
 */
class EXIV2API PsdImage : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor to open a Photoshop image. Since the
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
  explicit PsdImage(BasicIo::UniquePtr io);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  void writeMetadata() override;
  /*!
    @brief Not supported. Calling this function will throw an Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setComment(const std::string&) override;
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Return the MIME type of the image.

    The MIME type returned for Photoshop images is "image/x-photoshop".

    @note This should really be "image/vnd.adobe.photoshop"
        (officially registered with IANA in December 2005 -- see
        http://www.iana.org/assignments/media-types/image/vnd.adobe.photoshop)
        but Apple, as of Tiger (10.4.8), maps this official MIME type to a
        dynamic UTI, rather than "com.adobe.photoshop-image" as it should.
   */
  [[nodiscard]] std::string mimeType() const override;
  //@}

 private:
  //! @name Manipulators
  //@{
  void readResourceBlock(uint16_t resourceId, uint32_t resourceSize);
  /*!
    @brief Provides the main implementation of writeMetadata() by
          writing all buffered metadata to the provided BasicIo.
    @throw Error on input-output errors or when the image data is not valid.
    @param outIo BasicIo instance to write to (a temporary location).

   */
  void doWriteMetadata(BasicIo& outIo);
  uint32_t writeExifData(ExifData& exifData, BasicIo& out);
  //@}

  //! @name Accessors
  //@{
  static uint32_t writeIptcData(const IptcData& iptcData, BasicIo& out);
  uint32_t writeXmpData(const XmpData& xmpData, BasicIo& out) const;
  //@}

};  // class PsdImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new PsdImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newPsdInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a Photoshop image.
EXIV2API bool isPsdType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef PSDIMAGE_HPP_
