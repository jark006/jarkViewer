#ifndef __PSD_SYSTEM_H__
#define __PSD_SYSTEM_H__

#include <assert.h>
#include "psd_types.h"

#ifdef __cplusplus
extern "C" {
#endif


#if 1
#define psd_assert(x)			assert(x)
#else
#define psd_assert(x)			do {} while(0)
// or
 //#define psd_assert(x)		return psd_status_unkown_error
#endif


void * psd_malloc(psd_int size);
void * psd_realloc(void * block, psd_int size);
void psd_free(void * block);
void psd_freeif(void * block);
void * psd_fopen(psd_char * file_name);
psd_int psd_fsize(void * file);
psd_int psd_fread(psd_uchar * buffer, psd_int count, void * file);
psd_int psd_fseek(void * file, psd_int length);
void psd_fclose(void * file);


#ifdef __cplusplus
}
#endif

#endif
