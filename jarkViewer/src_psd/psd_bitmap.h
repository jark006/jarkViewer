#ifndef __PSD_BITMAP_H__
#define __PSD_BITMAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libpsd.h"
#include "psd_fixed.h"
#include "psd_rect.h"


typedef struct _psd_bitamp
{
	psd_argb_color *			image_data;
	psd_int							width;
	psd_int							height;
} psd_bitmap;

psd_status psd_get_bitmap(psd_bitmap * bitmap, psd_int width, psd_int height, psd_context * context);
psd_status psd_create_bitmap(psd_bitmap * bitmap, psd_int width, psd_int height);
psd_status psd_free_bitmap(psd_bitmap * bitmap);
psd_status psd_copy_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp);
psd_status psd_inflate_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, psd_int horz_size, psd_int vert_size);
psd_status psd_offset_bitmap(psd_bitmap * bitmap, psd_int offset_x, psd_int offset_y, psd_argb_color fill_color);
psd_status psd_draw_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, psd_rect * dst_rect, psd_rect * src_rect);
psd_status psd_scale_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp);
psd_status psd_fill_bitmap(psd_bitmap * bitmap, psd_argb_color color);
psd_status psd_fill_bitmap_without_alpha_channel(psd_bitmap * bitmap, psd_argb_color color);
psd_status psd_bitmap_copy_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp);
psd_status psd_bitmap_copy_without_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp);
psd_status psd_bitmap_mix_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp);
psd_status psd_bitmap_blend_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp);
psd_status psd_bitmap_contour_alpha_channel(psd_bitmap * bitmap, psd_uchar * lookup_table, 
	psd_bool anti_aliased, psd_bool edge_hidden);
psd_status psd_bitmap_fill_alpha_channel(psd_bitmap * bitmap, psd_color_component alpha);
psd_status psd_bitmap_reverse_alpha_channel(psd_bitmap * bitmap);
psd_status psd_bitmap_reverse_mixed_alpha_channel(psd_bitmap * bitmap);
psd_status psd_bitmap_knock_out(psd_bitmap * dst_bmp, psd_bitmap * src_bmp);
psd_status psd_bitmap_find_edge(psd_bitmap * bitmap, psd_bool edge_hidden);
psd_status psd_bitmap_ajust_range(psd_bitmap * bitmap, psd_int range);
psd_argb_color psd_bitmap_get_pixel(psd_bitmap * bitmap, psd_int x, psd_int y);
psd_argb_color psd_bitmap_get_fixed_pixel(psd_bitmap * bitmap, psd_fixed_16_16 x, psd_fixed_16_16 y);
psd_status psd_bitmap_blend_mask(psd_bitmap * bitmap, psd_layer_mask_info * layer_mask_info);


#ifdef __cplusplus
}
#endif

#endif
