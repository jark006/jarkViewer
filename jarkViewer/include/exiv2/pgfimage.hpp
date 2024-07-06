// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PGFIMAGE_HPP_
#define PGFIMAGE_HPP_

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
  @brief Class to access PGF images. Exif and IPTC metadata are supported
      directly.
 */
class EXIV2API PgfImage : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing PGF image or create
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
  PgfImage(BasicIo::UniquePtr io, bool create);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  void writeMetadata() override;
  //@}

  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override {
    return "image/pgf";
  }
  //@}

 private:
  bool bSwap_;  // true for bigEndian hardware, else false
  /*!
    @brief Provides the main implementation of writeMetadata() by
          writing all buffered metadata to the provided BasicIo.
    @throw Error on input-output errors or when the image data is not valid.
    @param outIo BasicIo instance to write to (a temporary location).

   */
  void doWriteMetadata(BasicIo& outIo);
  //! Read Magick number. Only version >= 6 is supported.
  static byte readPgfMagicNumber(BasicIo& iIo);
  //! Read PGF Header size encoded in 32 bits integer.
  size_t readPgfHeaderSize(BasicIo& iIo) const;
  //! Read header structure.
  DataBuf readPgfHeaderStructure(BasicIo& iIo, uint32_t& width, uint32_t& height) const;
  //@}

};  // class PgfImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new PgfImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newPgfInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a PGF image.
EXIV2API bool isPgfType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef PGFIMAGE_HPP_
