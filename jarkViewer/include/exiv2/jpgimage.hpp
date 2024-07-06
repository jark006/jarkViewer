// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JPGIMAGE_HPP_
#define JPGIMAGE_HPP_

#include "exiv2lib_export.h"

// included header files
#include "error.hpp"
#include "image.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
// *****************************************************************************
// class definitions

/*!
  @brief Abstract helper base class to access JPEG images.
 */
class EXIV2API JpegBase : public Image {
 public:
  //! @name Manipulators
  //@{
  void readMetadata() override;
  void writeMetadata() override;
  void printStructure(std::ostream& out, PrintStructureOption option, size_t depth) override;
  //@}

 protected:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing image or create
        a new image from scratch. If a new image is to be created, any
        existing data is overwritten.
    @param type Image type.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method.  Use the Image::io()
        method to get a temporary reference.
    @param create Specifies if an existing image should be read (false)
        or if a new image should be created (true).
    @param initData Data to initialize newly created images. Only used
        when \em create is true. Should contain data for the smallest
        valid image of the calling subclass.
    @param dataSize Size of initData in bytes.
   */
  JpegBase(ImageType type, BasicIo::UniquePtr io, bool create, const byte initData[], size_t dataSize);
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Determine if the content of the BasicIo instance is of the
        type supported by this class.

    The advance flag determines if the read position in the stream is
    moved (see below). This applies only if the type matches and the
    function returns true. If the type does not match, the stream
    position is not changed. However, if reading from the stream fails,
    the stream position is undefined. Consult the stream state to obtain
    more information in this case.

    @param iIo BasicIo instance to read from.
    @param advance Flag indicating whether the position of the io
        should be advanced by the number of characters read to
        analyse the data (true) or left at its original
        position (false). This applies only if the type matches.
    @return  true  if the data matches the type of this class;<BR>
             false if the data does not match
   */
  virtual bool isThisType(BasicIo& iIo, bool advance) const = 0;
  //@}

  //! @name Manipulators
  //@{
  /*!
    @brief Writes the image header (aka signature) to the BasicIo instance.
    @param oIo BasicIo instance that the header is written to.
    @return 0 if successful;<BR>
            4 if the output file can not be written to
   */
  virtual int writeHeader(BasicIo& oIo) const = 0;
  //@}

 private:
  //! @name Manipulators
  //@{
  /*!
    @brief Initialize the image with the provided data.
    @param initData Data to be written to the associated BasicIo
    @param dataSize Size in bytes of data to be written
    @return 0 if successful;<BR>
            4 if the image can not be written to.
   */
  int initImage(const byte initData[], size_t dataSize);
  /*!
    @brief Provides the main implementation of writeMetadata() by
          writing all buffered metadata to the provided BasicIo.
    @throw Error on input-output errors or when the image data is not valid.
    @param outIo BasicIo instance to write to (a temporary location).

   */
  void doWriteMetadata(BasicIo& outIo);
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Advances associated io instance to one byte past the next
        Jpeg marker and returns the marker. This method should be called
        when the BasicIo instance is positioned one byte past the end of a
        Jpeg segment.
    @param err the error code to throw if no marker is found
    @return the next Jpeg segment marker if successful;<BR>
            throws an Error if not successful
   */
  [[nodiscard]] byte advanceToMarker(ErrorCode err) const;
  //@}

  DataBuf readNextSegment(byte marker);
};

/*!
  @brief Class to access JPEG images
 */
class EXIV2API JpegImage : public JpegBase {
  friend EXIV2API bool isJpegType(BasicIo& iIo, bool advance);

 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing Jpeg image or create
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
  JpegImage(BasicIo::UniquePtr io, bool create);
  //@}
  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override;
  //@}

 protected:
  //! @name Accessors
  //@{
  bool isThisType(BasicIo& iIo, bool advance) const override;
  //@}
  //! @name Manipulators
  //@{
  /*!
    @brief Writes a Jpeg header (aka signature) to the BasicIo instance.
    @param outIo BasicIo instance that the header is written to.
    @return 0 if successful;<BR>
           2 if the input image is invalid or can not be read;<BR>
           4 if the temporary image can not be written to;<BR>
          -3 other temporary errors
   */
  int writeHeader(BasicIo& outIo) const override;
  //@}

 private:
  // Constant data
  static const byte blank_[];  ///< Minimal Jpeg image
};

//! Helper class to access %Exiv2 files
class EXIV2API ExvImage : public JpegBase {
  friend EXIV2API bool isExvType(BasicIo& iIo, bool advance);

 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing EXV image or create
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
  ExvImage(BasicIo::UniquePtr io, bool create);
  //@}
  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override;
  //@}

 protected:
  //! @name Accessors
  //@{
  bool isThisType(BasicIo& iIo, bool advance) const override;
  //@}
  //! @name Manipulators
  //@{
  int writeHeader(BasicIo& outIo) const override;
  //@}

 private:
  // Constant data
  static constexpr char exiv2Id_[] = "Exiv2";                                          // EXV identifier
  static constexpr byte blank_[] = {0xff, 0x01, 'E', 'x', 'i', 'v', '2', 0xff, 0xd9};  // Minimal exiv2 file

};  // class ExvImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new JpegImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newJpegInstance(BasicIo::UniquePtr io, bool create);
//! Check if the file iIo is a JPEG image.
EXIV2API bool isJpegType(BasicIo& iIo, bool advance);
/*!
  @brief Create a new ExvImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newExvInstance(BasicIo::UniquePtr io, bool create);
//! Check if the file iIo is an EXV file
EXIV2API bool isExvType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef JPGIMAGE_HPP_
