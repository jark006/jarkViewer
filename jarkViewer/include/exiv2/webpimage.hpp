// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WEBPIMAGE_HPP
#define WEBPIMAGE_HPP

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
  @brief Class to access WEBP video files.
 */
class EXIV2API WebPImage : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor for a WebP video. Since the constructor
        can not return a result, callers should check the good() method
        after object construction to determine success or failure.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method. Use the Image::io()
        method to get a temporary reference.
   */
  explicit WebPImage(BasicIo::UniquePtr io);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  void writeMetadata() override;
  void printStructure(std::ostream& out, PrintStructureOption option, size_t depth) override;
  //@}

  /*!
    @brief Not supported. Calling this function will throw an Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setComment(const std::string&) override;
  void setIptcData(const IptcData& /*iptcData*/) override;

  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override;
  //@}

 private:
  void doWriteMetadata(BasicIo& outIo);

  //! Finds the offset of header in data. Returns std::string::npos if the header isn't found.
  static size_t getHeaderOffset(const byte* data, size_t data_size, const byte* header, size_t header_size);

  static bool equalsWebPTag(const Exiv2::DataBuf& buf, const char* str);
  void debugPrintHex(byte* data, size_t size);
  void decodeChunks(uint32_t filesize);
  void inject_VP8X(BasicIo& iIo, bool has_xmp, bool has_exif, bool has_alpha, bool has_icc, uint32_t width,
                   uint32_t height) const;
  /* Misc. */
  static constexpr byte WEBP_PAD_ODD = 0;
  static constexpr int WEBP_TAG_SIZE = 0x4;
  /* VP8X feature flags */
  static constexpr int WEBP_VP8X_ICC_BIT = 0x20;
  static constexpr int WEBP_VP8X_ALPHA_BIT = 0x10;
  static constexpr int WEBP_VP8X_EXIF_BIT = 0x8;
  static constexpr int WEBP_VP8X_XMP_BIT = 0x4;
  /* Chunk header names */
  static constexpr auto WEBP_CHUNK_HEADER_VP8X = "VP8X";
  static constexpr auto WEBP_CHUNK_HEADER_VP8L = "VP8L";
  static constexpr auto WEBP_CHUNK_HEADER_VP8 = "VP8 ";
  static constexpr auto WEBP_CHUNK_HEADER_ANMF = "ANMF";
  static constexpr auto WEBP_CHUNK_HEADER_ANIM = "ANIM";
  static constexpr auto WEBP_CHUNK_HEADER_ICCP = "ICCP";
  static constexpr auto WEBP_CHUNK_HEADER_EXIF = "EXIF";
  static constexpr auto WEBP_CHUNK_HEADER_XMP = "XMP ";
};  // Class WebPImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new WebPImage instance and return an auto-pointer to it.
      Caller owns the returned object and the auto-pointer ensures that
      it will be deleted.
 */
EXIV2API Image::UniquePtr newWebPInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a WebP Video.
EXIV2API bool isWebPType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // WEBPIMAGE_HPP
