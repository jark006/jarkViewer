#ifndef __PSD_DESCRIPTOR_H__
#define __PSD_DESCRIPTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libpsd.h"


void psd_stream_get_object_null(psd_uint type, psd_context * context);
psd_argb_color psd_stream_get_object_color(psd_context * context);
void psd_stream_get_object_contour(psd_uchar * lookup_table, psd_context * context);
psd_blend_mode psd_stream_get_object_blend_mode(psd_context * context);
psd_technique_type psd_stream_get_object_technique(psd_context * context);
psd_gradient_style psd_stream_get_object_gradient_style(psd_context * context);
void psd_stream_get_object_gradient_color(psd_gradient_color * gradient_color, psd_context * context);
void psd_stream_get_object_pattern_info(psd_pattern_info * pattern_info, psd_context * context);
void psd_stream_get_object_point(psd_int * horz, psd_int * vert, psd_context * context);


#ifdef __cplusplus
}
#endif

#endif
