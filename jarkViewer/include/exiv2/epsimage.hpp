// SPDX-License-Identifier: GPL-2.0-or-later

/*!
  @file    epsimage.hpp
  @brief   EPS image.
           <br>References:
           <br>[1] <a href="http://partners.adobe.com/public/developer/en/ps/5001.DSC_Spec.pdf">Adobe PostScript
  Language Document Structuring Conventions Specification, Version 3.0</a>, September 1992 <br>[2] <a
  href="http://partners.adobe.com/public/developer/en/ps/5002.EPSF_Spec.pdf">Adobe Encapsulated PostScript File Format
  Specification, Version 3.0</a>, May 1992 <br>[3] <a
  href="http://www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf">Adobe XMP Specification
  Part 3: Storage in Files</a>, July 2010 <br>[4] <a
  href="http://groups.google.com/group/adobe.illustrator.windows/msg/0a9d7b1244b59062">Re: Thumbnail data format in ai
  file</a>, Dec 2003
  @author  Michael Ulbrich (mul)
           <a href="mailto:mul@rentapacs.de">mul@rentapacs.de</a>
  @author  Volker Grabsch (vog)
           <a href="mailto:vog@notjusthosting.com">vog@notjusthosting.com</a>
  @date    7-Mar-2011, vog: created
 */
#ifndef EPSIMAGE_HPP_
#define EPSIMAGE_HPP_

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
  @brief Class to access EPS images.
 */
class EXIV2API EpsImage : public Image {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor to open a EPS image. Since the
        constructor can't return a result, callers should check the
        good() method after object construction to determine success
        or failure.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method.  Use the Image::io()
        method to get a temporary reference.
    @param create Specifies if an existing image should be read (false)
        or if a new file should be created (true).
   */
  EpsImage(BasicIo::UniquePtr io, bool create);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  void writeMetadata() override;
  /*!
    @brief Not supported.
        Calling this function will throw an instance of Error(ErrorCode::kerInvalidSettingForImage).
   */
  void setComment(const std::string&) override;
  //@}

  //! @name Accessors
  //@{
  [[nodiscard]] std::string mimeType() const override;
  //@}
};  // class EpsImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new EpsImage instance and return an auto-pointer to it.
         Caller owns the returned object and the auto-pointer ensures that
         it will be deleted.
 */
EXIV2API Image::UniquePtr newEpsInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a EPS image.
EXIV2API bool isEpsType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef EPSIMAGE_HPP_
