#ifndef __PSD_GRADIENT_H__
#define __PSD_GRADIENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libpsd.h"


#define PSD_TAN(angle)			((angle) <= 45 ? (angle) / 45.0 : 45.0 / (90 - (angle)))


psd_status psd_gradient_fill_linear(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse, 
	psd_int start_x, psd_int start_y, psd_int end_x, psd_int end_y);
psd_status psd_gradient_fill_radial(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse, 
	psd_int center_x, psd_int center_y, psd_int radius);
psd_status psd_gradient_fill_angle(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse, 
	psd_int center_x, psd_int center_y, psd_int angle);
psd_status psd_gradient_fill_reflected(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse,
	psd_int start_x, psd_int start_y, psd_int end_x, psd_int end_y);
psd_status psd_gradient_fill_diamond(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse,
	psd_int center_x, psd_int center_y, psd_int radius, psd_int angle);
psd_float psd_carm_sqrt(psd_float x);


#ifdef __cplusplus
}
#endif

#endif
