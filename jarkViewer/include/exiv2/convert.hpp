// SPDX-License-Identifier: GPL-2.0-or-later

/*!
  @file    convert.hpp
  @brief   Exif and IPTC conversions to and from XMP
  @author  Andreas Huggel (ahu)
           <a href="mailto:ahuggel@gmx.net">ahuggel@gmx.net</a><BR>
           Vladimir Nadvornik (vn)
           <a href="mailto:nadvornik@suse.cz">nadvornik@suse.cz</a>
  @date    17-Mar-08, ahu: created basic converter framework<BR>
           20-May-08, vn:  added actual conversion logic
 */
#ifndef CONVERT_HPP_
#define CONVERT_HPP_

#include "exiv2lib_export.h"

// included header files
#include "config.h"

// + standard includes
#include <string>

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
// *****************************************************************************
// class declarations
class ExifData;
class IptcData;
class XmpData;

// *****************************************************************************
// free functions, template and inline definitions

//! Convert (copy) Exif tags to XMP properties.
EXIV2API void copyExifToXmp(const ExifData& exifData, XmpData& xmpData);
//! Convert (move) Exif tags to XMP properties, remove converted Exif tags.
EXIV2API void moveExifToXmp(ExifData& exifData, XmpData& xmpData);

//! Convert (copy) XMP properties to Exif tags.
EXIV2API void copyXmpToExif(const XmpData& xmpData, ExifData& exifData);
//! Convert (move) XMP properties to Exif tags, remove converted XMP properties.
EXIV2API void moveXmpToExif(XmpData& xmpData, ExifData& exifData);

//! Detect which metadata are newer and perform a copy in appropriate direction.
EXIV2API void syncExifWithXmp(ExifData& exifData, XmpData& xmpData);

//! Convert (copy) IPTC datasets to XMP properties.
EXIV2API void copyIptcToXmp(const IptcData& iptcData, XmpData& xmpData, const char* iptcCharset = nullptr);
//! Convert (move) IPTC datasets to XMP properties, remove converted IPTC datasets.
EXIV2API void moveIptcToXmp(IptcData& iptcData, XmpData& xmpData, const char* iptcCharset = nullptr);

//! Convert (copy) XMP properties to IPTC datasets.
EXIV2API void copyXmpToIptc(const XmpData& xmpData, IptcData& iptcData);
//! Convert (move) XMP properties to IPTC tags, remove converted XMP properties.
EXIV2API void moveXmpToIptc(XmpData& xmpData, IptcData& iptcData);

/*!
  @brief Convert character encoding of \em str from \em from to \em to.
         If the function succeeds, \em str contains the result string.

  This function uses the iconv library, if the %Exiv2 library was compiled
  with iconv support.  Otherwise, on Windows, it uses Windows functions to
  support a limited number of conversions and fails with a warning if an
  unsupported conversion is attempted.  If the function is called but %Exiv2
  was not compiled with iconv support and can't use Windows functions, it
  fails with a warning.

  The conversions supported on Windows without iconv are:

  <TABLE>
  <TR><TD><B>from</B></TD><TD><B>to</B></TD></TR>
  <TR><TD>UTF-8</TD>     <TD>UCS-2BE</TD></TR>
  <TR><TD>UTF-8</TD>     <TD>UCS-2LE</TD></TR>
  <TR><TD>UCS-2BE</TD>   <TD>UTF-8</TD></TR>
  <TR><TD>UCS-2BE</TD>   <TD>UCS-2LE</TD></TR>
  <TR><TD>UCS-2LE</TD>   <TD>UTF-8</TD></TR>
  <TR><TD>UCS-2LE</TD>   <TD>UCS-2BE</TD></TR>
  <TR><TD>ISO-8859-1</TD><TD>UTF-8</TD></TR>
  <TR><TD>ASCII</TD>     <TD>UTF-8</TD></TR>
  </TABLE>

  @param str  The string to convert. It is updated to the converted string,
              which may have a different size. If the function call fails,
              the string is not modified.
  @param from Charset in which the input string is encoded as a name
              understood by \c iconv_open(3).
  @param to   Charset to convert the string to as a name
              understood by \c iconv_open(3).
  @return Return \c true if the conversion was successful, else \c false.
 */
EXIV2API bool convertStringCharset(std::string& str, const char* from, const char* to);

}  // namespace Exiv2

#endif  // #ifndef CONVERT_HPP_
