// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PREVIEW_HPP_
#define PREVIEW_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

#include "image.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
// *****************************************************************************
// class definitions

//! Type of preview image.
using PreviewId = int;

/*!
  @brief Preview image properties.
 */
struct EXIV2API PreviewProperties {
  std::string mimeType_;   //!< Preview image mime type.
  std::string extension_;  //!< Preview image extension.
  size_t size_{};          //!< Preview image size in bytes.
  size_t width_{};         //!< Preview image width in pixels or 0 for unknown width.
  size_t height_{};        //!< Preview image height in pixels or 0 for unknown height.
  PreviewId id_{};         //!< Identifies type of preview image.
};

//! Container type to hold all preview images metadata.
using PreviewPropertiesList = std::vector<PreviewProperties>;

/*!
  @brief Class that holds preview image properties and data buffer.
 */
class EXIV2API PreviewImage {
  friend class PreviewManager;

 public:
  //! @name Constructors
  //@{
  //! Copy constructor
  PreviewImage(const PreviewImage& rhs);
  //@}

  ~PreviewImage() = default;

  //! @name Manipulators
  //@{
  //! Assignment operator
  PreviewImage& operator=(const PreviewImage& rhs);
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Return a copy of the preview image data. The caller owns
           this copy and %DataBuf ensures that it will be deleted.
   */
  [[nodiscard]] DataBuf copy() const;
  /*!
    @brief Return a pointer to the image data for read-only access.
   */
  [[nodiscard]] const byte* pData() const;
  /*!
    @brief Return the size of the preview image in bytes.
   */
  [[nodiscard]] uint32_t size() const;
  /*!
    @brief Write the thumbnail image to a file.

    A filename extension is appended to \em path according to the image
    type of the preview image, so \em path should not include an extension.
    The function will overwrite an existing file of the same name.

    @param path File name of the preview image without extension.
    @return The number of bytes written.
  */
  [[nodiscard]] size_t writeFile(const std::string& path) const;
  /*!
    @brief Return the MIME type of the preview image, usually either
           \c "image/tiff" or \c "image/jpeg".
   */
  [[nodiscard]] std::string mimeType() const;
  /*!
    @brief Return the file extension for the format of the preview image
           (".tif" or ".jpg").
   */
  [[nodiscard]] std::string extension() const;
  /*!
    @brief Return the width of the preview image in pixels.
  */
  [[nodiscard]] size_t width() const;
  /*!
    @brief Return the height of the preview image in pixels.
  */
  [[nodiscard]] size_t height() const;
  /*!
    @brief Return the preview image type identifier.
  */
  [[nodiscard]] PreviewId id() const;
  //@}

 private:
  //! Private constructor
  PreviewImage(PreviewProperties properties, DataBuf&& data);

  PreviewProperties properties_;  //!< Preview image properties
  DataBuf preview_;               //!< Preview image data

};  // class PreviewImage

/*!
  @brief Class for extracting preview images from image metadata.
 */
class EXIV2API PreviewManager {
 public:
  //! @name Constructors
  //@{
  //! Constructor.
  explicit PreviewManager(const Image& image);
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Return the properties of all preview images in a list
           sorted by preview width * height, starting with the smallest
           preview image.
   */
  [[nodiscard]] PreviewPropertiesList getPreviewProperties() const;
  /*!
    @brief Return the preview image for the given preview properties.
   */
  [[nodiscard]] PreviewImage getPreviewImage(const PreviewProperties& properties) const;
  //@}

 private:
  const Image& image_;

};  // class PreviewManager
}  // namespace Exiv2

#endif  // #ifndef PREVIEW_HPP_
