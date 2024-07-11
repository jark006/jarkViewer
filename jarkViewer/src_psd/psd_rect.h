#ifndef __PSD_RECT_H__
#define __PSD_RECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libpsd.h"


typedef struct _psd_rect
{
	psd_int 			left;
	psd_int 			top;
	psd_int 			right;
	psd_int 			bottom;
} psd_rect;


psd_bool psd_incept_rect(psd_rect * r1, psd_rect * r2, psd_rect * dst_rect);
psd_bool psd_equal_rect(psd_rect * r1, psd_rect * r2);
psd_bool psd_subtract_rect(psd_rect * r1, psd_rect * r2, psd_rect * dst_rect);
void psd_make_rect(psd_rect * rc, psd_int left, psd_int top, psd_int right, psd_int bottom);
void psd_dup_rect(psd_rect * dst, psd_rect * src);
psd_int psd_rect_width(psd_rect * rc);
psd_int psd_rect_height(psd_rect * rc);
void psd_offset_rect(psd_rect * rc, psd_int dlt_x, psd_int dlt_y);
void psd_inflate_rect(psd_rect * rc, psd_int dlt_x, psd_int dlt_y);
psd_bool psd_point_in_rect(psd_rect * rc, psd_int x, psd_int y);
psd_bool psd_is_empty_rect(psd_rect * rc);



#ifdef __cplusplus
}
#endif

#endif
