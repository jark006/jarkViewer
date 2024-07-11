#ifndef __PSD_CONFIG_H__
#define __PSD_CONFIG_H__


// we need libjpeg to get the thumbnail
// You can download libjpeg from http://sourceforge.net/projects/libjpeg
#if 0
#define PSD_INCLUDE_LIBJPEG
#endif


// Photoshop CS and CS2 use the zip arithmetic for data uncompression (compression = 3)
// You can download zlib from http://www.zlib.net
#if 0
#define PSD_INCLUDE_ZLIB
#endif


// get all of the image resource
// it takes more time and memory
#if 0
#define PSD_GET_ALL_IMAGE_RESOURCE
#endif


// we need libxml to get the XMP metadata, it's a file info as XML description
// but we seldom use the XMP metadata to get the information of PSD file,
// so you don't have to define this macro
// You can download libxml from http://sourceforge.net/projects/libxml
#if 1
#if defined(PSD_GET_ALL_IMAGE_RESOURCE)
#define PSD_INCLUDE_LIBXML
#endif
#endif


// we need libexif to get the exif info
// Exif: Exchangeable image file format for Digital Still Cameras
// http://www.pima.net/standards/it10/PIMA15740/exif.htm
// You can download EXIF Tag Parsing Library from http://sourceforge.net/projects/libexif
#if 1
#if defined(PSD_GET_ALL_IMAGE_RESOURCE)
#define PSD_INCLUDDE_LIBEXIF
#endif
#endif


// get path resource
#if 1
#if defined(PSD_GET_ALL_IMAGE_RESOURCE)
#define PSD_GET_PATH_RESOURCE
#endif
#endif


// support for blending psd file
#if 1
#define PSD_SUPPORT_LAYER_BLEND
#endif


// support for blending layer effects, such as shadow and glow
#if 1
#if defined(PSD_SUPPORT_LAYER_BLEND)
#define PSD_SUPPORT_EFFECTS_BLEND
#endif
#endif


// support for cmyk color space
#if 1
#define PSD_SUPPORT_CMYK
#endif


// support for lab color space
#if 1
#define PSD_SUPPORT_LAB
#endif


// support for multi-channel color space
#if 1
#define PSD_SUPPORT_MULTICHANNEL
#endif


#endif
