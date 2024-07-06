// config.h

#ifndef _CONFIG_H_
#define _CONFIG_H_

///// Start of Visual Studio Support /////
#ifdef _MSC_VER

#pragma warning(disable : 4996)  // Disable warnings about 'deprecated' standard functions
#pragma warning(disable : 4251)  // Disable warnings from std templates about exporting interfaces

#endif  // _MSC_VER
///// End of Visual Studio Support /////

#include "exv_conf.h"
////////////////////////////////////////

///// Start of platform macros /////////
#if defined(__MINGW32__) || defined(__MINGW64__)
#ifndef __MING__
#define __MING__ 1
#endif
#ifndef __MINGW__
#define __MINGW__ 1
#endif
#endif

#ifndef __CYGWIN__
#if defined(__CYGWIN32__) || defined(__CYGWIN64__)
#define __CYGWIN__ 1
#endif
#endif

#ifndef __LITTLE_ENDIAN__
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#endif
#endif
#endif

#ifndef __LITTLE_ENDIAN__
#if defined(_WIN32) || defined(__CYGWIN__)
#define __LITTLE_ENDIAN__ 1
#endif
#endif

/*
  If you're using Solaris and the Solaris Studio compiler
  you must -library=stdcxx4 along with these inclusions below
*/
#if defined(OS_SOLARIS)
#include <math.h>
#include <string.h>
#include <strings.h>
#if defined(__cplusplus)
#include <fstream>
#include <ios>
#endif
#endif
///// End of platform macros /////////

///// Path separator macros      /////
#ifndef EXV_SEPARATOR_STR
#if defined(_WIN32)
#define EXV_SEPARATOR_STR "\\"
#define EXV_SEPARATOR_CHR '\\'
#else
#define EXV_SEPARATOR_STR "/"
#define EXV_SEPARATOR_CHR '/'
#endif
#endif
//////////////////////////////////////

#endif  // _CONFIG_H_
