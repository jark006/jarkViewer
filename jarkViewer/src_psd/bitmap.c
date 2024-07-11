/**
 * libpsd - Photoshop file formats (*.psd) decode library
 * Copyright (C) 2004-2007 Graphest Software.
 *
 * libpsd is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: bitmap.c, created by Patrick in 2006.06.21, libpsd@graphest.com Exp $
 */

#include <string.h>
#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_color.h"
#include "psd_bitmap.h"
#include "psd_math.h"


#define PSD_MIN_TEMP_IMAGE_LENGTH		12288


psd_status psd_get_bitmap(psd_bitmap * bitmap, psd_int width, psd_int height, psd_context * context)
{
	psd_int length;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;
	if(width <= 0 || height <= 0)
		return psd_status_invalid_bitmap;
	
	length = width * height * 4;
	if(length > context->temp_image_length)
	{
		psd_freeif(context->temp_image_data);
		context->temp_image_length = PSD_MAX(context->temp_image_length * 2, length);
		context->temp_image_length = PSD_MAX(context->temp_image_length, PSD_MIN_TEMP_IMAGE_LENGTH);
		context->temp_image_data = (psd_uchar *)psd_malloc(context->temp_image_length);
		if(context->temp_image_data == NULL)
		{
			context->temp_image_length = 0;
			return psd_status_malloc_failed;
		}
	}

	bitmap->width = width;
	bitmap->height = height;
	bitmap->image_data = (psd_argb_color *)context->temp_image_data;

	return psd_status_done;
}

psd_status psd_create_bitmap(psd_bitmap * bitmap, psd_int width, psd_int height)
{
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;
	if(width <= 0 || height <= 0)
		return psd_status_invalid_bitmap;

	bitmap->width = width;
	bitmap->height = height;
	bitmap->image_data = (psd_argb_color *)psd_malloc(width * height * 4);
	if(bitmap->image_data == NULL)
	{
		bitmap->width = 0;
		bitmap->height = 0;
		return psd_status_malloc_failed;
	}

	return psd_status_done;
}

psd_status psd_free_bitmap(psd_bitmap * bitmap)
{
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	psd_freeif(bitmap->image_data);
	bitmap->image_data = NULL;
	bitmap->width = 0;
	bitmap->height = 0;
	
	return psd_status_done;
}

psd_status psd_copy_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp)
{
	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width != src_bmp->width || dst_bmp->height != src_bmp->height)
		return psd_status_bitmap_dismatch_size;

	memcpy(dst_bmp->image_data, src_bmp->image_data, src_bmp->width * src_bmp->height * 4);

	return psd_status_done;
}

psd_status psd_inflate_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, psd_int horz_size, psd_int vert_size)
{
	psd_int i, length;
	psd_argb_color * dst_data, * src_data;
	
	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width != src_bmp->width + horz_size * 2 || 
		dst_bmp->height != src_bmp->height + vert_size * 2)
		return psd_status_bitmap_dismatch_size;

	psd_fill_bitmap(dst_bmp, psd_color_clear);

	dst_data = dst_bmp->image_data + vert_size * dst_bmp->width + horz_size;
	src_data = src_bmp->image_data;
	length = src_bmp->width * 4;
	for(i = src_bmp->height; i --; )
	{
		memcpy(dst_data, src_data, length);
		dst_data += dst_bmp->width;
		src_data += src_bmp->width;
	}

	return psd_status_done;
}

psd_status psd_offset_bitmap(psd_bitmap * bitmap, psd_int offset_x, psd_int offset_y, psd_argb_color fill_color)
{
	psd_int i, width, height;
	psd_argb_color * dst_data;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	if(offset_x != 0)
	{
		if(PSD_ABS(offset_x) >= bitmap->width)
		{
			psd_fill_bitmap(bitmap, fill_color);
			return psd_status_done;
		}

		width = bitmap->width - PSD_ABS(offset_x);
		for(i = 0; i < bitmap->height; i ++)
		{
			dst_data = bitmap->image_data + i * bitmap->width;
			if(offset_x > 0)
			{
				memmove(dst_data + offset_x, dst_data, width * 4);
				psd_color_memset(dst_data, fill_color, offset_x);
			}
			else
			{
				memmove(dst_data, dst_data - offset_x, width * 4);
				psd_color_memset(dst_data - offset_x, fill_color, -offset_x);
			}
		}
	}

	if(offset_y != 0)
	{
		if(PSD_ABS(offset_y) >= bitmap->height)
		{
			psd_fill_bitmap(bitmap, fill_color);
			return psd_status_done;
		}

		height = bitmap->height - PSD_ABS(offset_y);
		dst_data = bitmap->image_data;
		if(offset_y > 0)
		{
			memmove(dst_data + offset_y * bitmap->width, dst_data, height * bitmap->width * 4);
			psd_color_memset(dst_data, fill_color, offset_y * bitmap->width);
		}
		else
		{
			memmove(dst_data, dst_data - offset_y * bitmap->width, height * bitmap->width * 4);
			psd_color_memset(dst_data - offset_y * bitmap->width, fill_color, -offset_y * bitmap->width);
		}
	}

	return psd_status_done;
}

static psd_bool psd_make_transfer_bound(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, 
	psd_rect * dst_rect, psd_rect * src_rect,
	psd_rect * dst_bound, psd_rect * src_bound)
{
	psd_rect rc;
	
	if(dst_rect == NULL)
		psd_make_rect(dst_bound, 0, 0, dst_bmp->width, dst_bmp->height);
	else
	{
		psd_make_rect(&rc, 0, 0, dst_bmp->width, dst_bmp->height);
		if(psd_incept_rect(dst_rect, &rc, &rc) == psd_false)
			return psd_false;
		psd_dup_rect(dst_bound, dst_rect);
	}

	if(src_rect == NULL)
		psd_make_rect(src_bound, 0, 0, src_bmp->width, src_bmp->height);
	else
	{
		psd_make_rect(&rc, 0, 0, src_bmp->width, src_bmp->height);
		if(psd_incept_rect(src_rect, &rc, &rc) == psd_false)
			return psd_false;
		psd_dup_rect(src_bound, src_rect);
	}

	if(dst_bound->left < 0)
	{
		src_bound->left += 0 - dst_bound->left;
		dst_bound->left = 0;
	}
	if(dst_bound->top < 0)
	{
		src_bound->top += 0 - dst_bound->top;
		dst_bound->top = 0;
	}
	if(dst_bound->right > dst_bmp->width)
	{
		src_bound->right -= dst_bound->right - dst_bmp->width;
		dst_bound->right = dst_bmp->width;
	}
	if(dst_bound->bottom > dst_bmp->height)
	{
		src_bound->bottom -= dst_bound->bottom - dst_bmp->height;
		dst_bound->bottom = dst_bmp->height;
	}
	
	if(src_bound->left < 0)
	{
		dst_bound->left += 0 - src_bound->left;
		src_bound->left = 0;
	}
	if(src_bound->top < 0)
	{
		dst_bound->top += 0 - src_bound->top;
		src_bound->top = 0;
	}
	if(src_bound->right > src_bmp->width)
	{
		dst_bound->right -= src_bound->right - src_bmp->width;
		src_bound->right = src_bmp->width;
	}
	if(src_bound->bottom > src_bmp->height)
	{
		dst_bound->bottom -= src_bound->bottom - src_bmp->height;
		src_bound->bottom = src_bmp->height;
	}
	
	if(psd_is_empty_rect(dst_bound) == psd_true || psd_is_empty_rect(src_bound) == psd_true)
		return psd_false;

	return psd_true;
}

psd_status psd_draw_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, psd_rect * dst_rect, psd_rect * src_rect)
{
	psd_rect dst_bound, src_bound;
	psd_int width, height, i;
	psd_argb_color * dst_data, * src_data;
	
	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;

	if(psd_make_transfer_bound(dst_bmp, src_bmp, dst_rect, src_rect, &dst_bound, &src_bound) == psd_false)
		return psd_status_done;

	width = psd_rect_width(&dst_bound) * 4;
	height = psd_rect_height(&dst_bound);
	dst_data = dst_bmp->image_data + dst_bound.top * dst_bmp->width + dst_bound.left;
	src_data = src_bmp->image_data + src_bound.top * src_bmp->width + src_bound.left;
	for(i = 0; i < height; i ++)
	{
		memcpy(dst_data, src_data, width);
		dst_data += dst_bmp->width;
		src_data += src_bmp->width;
	}

	return psd_status_done;
}

psd_status psd_scale_bitmap(psd_bitmap * dst_bmp, psd_bitmap * src_bmp)
{
	psd_fixed_16_16 step_x, step_y, cur_x, cur_y;
	psd_int i, j;
	psd_argb_color * dst_data;
	
	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width == src_bmp->width && dst_bmp->height == src_bmp->height)
		return psd_copy_bitmap(dst_bmp, src_bmp);

	step_x = psd_fixed_16_16_int(src_bmp->width) / dst_bmp->width;
	step_y = psd_fixed_16_16_int(src_bmp->height) / dst_bmp->height;

	dst_data = dst_bmp->image_data;
	for(i = 0, cur_y = 0; i < dst_bmp->height; i ++)
	{
		for(j = 0, cur_x = 0; j < dst_bmp->width; j ++, dst_data ++)
		{
			*dst_data = psd_bitmap_get_fixed_pixel(src_bmp, cur_x, cur_y);
			cur_x += step_x;
		}
		cur_y += step_y;
	}

	return psd_status_done;
}

psd_status psd_fill_bitmap(psd_bitmap * bitmap, psd_argb_color color)
{
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	psd_color_memset(bitmap->image_data, color, bitmap->width * bitmap->height);
	
	return psd_status_done;
}

psd_status psd_fill_bitmap_without_alpha_channel(psd_bitmap * bitmap, psd_argb_color color)
{
	psd_int i;
	psd_argb_color * dst_data, src_color;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	dst_data = bitmap->image_data;
	src_color = color & 0x00FFFFFF;
	for(i = bitmap->width * bitmap->height; i --; )
	{
		*dst_data = (*dst_data & 0xFF000000) | src_color;
		dst_data ++;
	}

	return psd_status_done;
}

psd_status psd_bitmap_copy_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp)
{
	psd_int i;
	psd_argb_color * dst_data, * src_data;

	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width != src_bmp->width || dst_bmp->height != src_bmp->height)
		return psd_status_bitmap_dismatch_size;

	dst_data = dst_bmp->image_data;
	src_data = src_bmp->image_data;
	for(i = src_bmp->width * src_bmp->height; i --; )
	{
		*dst_data = (*dst_data & 0x00FFFFFF) | (*src_data & 0xFF000000);
		dst_data ++;
		src_data ++;
	}

	return psd_status_done;
}

psd_status psd_bitmap_copy_without_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp)
{
	psd_int i;
	psd_argb_color * dst_data, * src_data;

	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width != src_bmp->width || dst_bmp->height != src_bmp->height)
		return psd_status_bitmap_dismatch_size;

	dst_data = dst_bmp->image_data;
	src_data = src_bmp->image_data;
	for(i = src_bmp->width * src_bmp->height; i --; )
	{
		*dst_data = (*dst_data & 0xFF000000) | (*src_data & 0x00FFFFFF);
		dst_data ++;
		src_data ++;
	}

	return psd_status_done;
}

psd_status psd_bitmap_mix_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp)
{
	psd_int i;
	psd_argb_color * dst_data, * src_data;
	psd_int src_alpha, dst_alpha;

	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width != src_bmp->width || dst_bmp->height != src_bmp->height)
		return psd_status_bitmap_dismatch_size;

	dst_data = dst_bmp->image_data;
	src_data = src_bmp->image_data;
	for(i = src_bmp->width * src_bmp->height; i --; )
	{
		dst_alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
		src_alpha = PSD_GET_ALPHA_COMPONENT(*src_data);
		dst_alpha = dst_alpha * src_alpha >> 8;
		*dst_data = (*dst_data & 0x00FFFFFF) | (dst_alpha << 24);
		dst_data ++;
		src_data ++;
	}

	return psd_status_done;
}

psd_status psd_bitmap_blend_alpha_channel(psd_bitmap * dst_bmp, psd_bitmap * src_bmp)
{
	psd_int i;
	psd_argb_color * dst_data, * src_data, dst_color, src_color;
	psd_int src_alpha, dst_alpha;
	psd_int flr1, flr2;
	psd_int dst_red, dst_green, dst_blue;

	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width != src_bmp->width || dst_bmp->height != src_bmp->height)
		return psd_status_bitmap_dismatch_size;

	dst_data = dst_bmp->image_data;
	src_data = src_bmp->image_data;
	for(i = src_bmp->width * src_bmp->height; i --; )
	{
		dst_color = *dst_data;
		src_color = *src_data;
		src_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
		if(src_alpha > 0)
		{
			dst_alpha = PSD_GET_ALPHA_COMPONENT(dst_color);
			if(src_alpha == 255 || dst_alpha == 0)
				*dst_data = src_color;
			else
			{
				dst_red = PSD_GET_RED_COMPONENT(dst_color);
				dst_green = PSD_GET_GREEN_COMPONENT(dst_color);
				dst_blue = PSD_GET_BLUE_COMPONENT(dst_color);
				if(dst_alpha == 255)
				{
					dst_red = ((dst_red << 8) + 
						(PSD_GET_RED_COMPONENT(src_color) - dst_red) * src_alpha) >> 8;
					dst_green = ((dst_green << 8) + 
						(PSD_GET_GREEN_COMPONENT(src_color) - dst_green) * src_alpha) >> 8;
					dst_blue = ((dst_blue << 8) + 
						(PSD_GET_BLUE_COMPONENT(src_color) - dst_blue) * src_alpha) >> 8;
				}
				else
				{
					flr1 = (src_alpha << 8) /
						(src_alpha + ((256 - src_alpha) * dst_alpha >> 8));
					flr2 = 256 - flr1;
					dst_alpha = dst_alpha +
						((255 - dst_alpha) * src_alpha >> 8);
					dst_red = (dst_red * flr2 + 
						PSD_GET_RED_COMPONENT(src_color) * flr1) >> 8;
					dst_green = (dst_green * flr2 + 
						PSD_GET_GREEN_COMPONENT(src_color) * flr1) >> 8;
					dst_blue = (dst_blue * flr2 + 
						PSD_GET_BLUE_COMPONENT(src_color) * flr1) >> 8;
				}
				*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
			}
		}
		dst_data ++;
		src_data ++;
	}

	return psd_status_done;
}

psd_status psd_bitmap_contour_alpha_channel(psd_bitmap * bitmap, psd_uchar * lookup_table, 
	psd_bool anti_aliased, psd_bool edge_hidden)
{
	psd_uchar contour[256] = {
		0x00, 0x0b, 0x16, 0x21, 0x2c, 0x37, 0x42, 0x4d, 0x58, 0x63, 0x6e, 0x79, 0x84, 0x8f, 0x9a, 0xa5, 
		0xb0, 0xbb, 0xc6, 0xd1, 0xdc, 0xf2, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	psd_int i;
	psd_argb_color * dst_data;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;
	if(lookup_table == NULL)
		return psd_status_done;

	if(edge_hidden == psd_true)
	{
		if(anti_aliased == psd_false)
		{
			for(i = 0; i < 256; i ++)
				contour[i] = contour[i] * lookup_table[(psd_int)((psd_int)(i / 2.55) * 2.55 + 0.5)] >> 8;
		}
		else
		{
			for(i = 0; i < 256; i ++)
				contour[i] = contour[i] * lookup_table[i] >> 8;
		}
	}
	else
	{
		if(anti_aliased == psd_false)
		{
			for(i = 0; i < 256; i ++)
				contour[i] = lookup_table[(psd_int)((psd_int)(i / 2.55) * 2.55 + 0.5)];
		}
		else
		{
			for(i = 0; i < 256; i ++)
				contour[i] = lookup_table[i];
		}
	}

	dst_data = bitmap->image_data;
	for(i = bitmap->width * bitmap->height; i --; )
	{
		*dst_data = (*dst_data & 0x00FFFFFF) | (contour[PSD_GET_ALPHA_COMPONENT(*dst_data)] << 24);
		dst_data ++;
	}

	return psd_status_done;
}

psd_status psd_bitmap_fill_alpha_channel(psd_bitmap * bitmap, psd_color_component alpha)
{
	psd_int i;
	psd_argb_color * data;
	psd_argb_color alpha_color = alpha << 24;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	data = bitmap->image_data;
	for(i = bitmap->width * bitmap->height; i --; )
	{
		*data = (*data & 0x00FFFFFF) | alpha_color;
		data ++;
	}
	
	return psd_status_done;
}

psd_status psd_bitmap_reverse_alpha_channel(psd_bitmap * bitmap)
{
	psd_int i;
	psd_argb_color * data;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	data = bitmap->image_data;
	for(i = bitmap->width * bitmap->height; i --; )
	{
		*data = (*data & 0x00FFFFFF) | (0xFF000000 - (*data & 0xFF000000));
		data ++;
	}
	
	return psd_status_done;
}

psd_status psd_bitmap_reverse_mixed_alpha_channel(psd_bitmap * bitmap)
{
	psd_int i, alpha;
	psd_argb_color * data;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	data = bitmap->image_data;
	for(i = bitmap->width * bitmap->height; i --; )
	{
		if(*data > 0x00FFFFFF)
		{
			alpha = PSD_GET_ALPHA_COMPONENT(*data);
			//if(alpha >= 24)
				alpha = 255 - alpha;
			//else
			//	alpha = alpha * (255 - alpha) >> 8;
			*data = (*data & 0x00FFFFFF) | (alpha << 24);
		}
		data ++;
	}
	
	return psd_status_done;
}

// exist some problems
psd_status psd_bitmap_knock_out(psd_bitmap * dst_bmp, psd_bitmap * src_bmp)
{
	psd_int i;
	psd_argb_color * dst_data, * src_data;
	psd_int src_alpha, dst_alpha;
	
	if(dst_bmp == NULL || src_bmp == NULL)
		return psd_status_invalid_bitmap;
	if(dst_bmp->width != src_bmp->width || dst_bmp->height != src_bmp->height)
		return psd_status_bitmap_dismatch_size;

	src_data = src_bmp->image_data;
	dst_data = dst_bmp->image_data;
	for(i = src_bmp->width * src_bmp->height; i --; )
	{
		src_alpha = PSD_GET_ALPHA_COMPONENT(*src_data);
		dst_alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
		dst_alpha = dst_alpha * (255 - src_alpha) >> 8;
		*dst_data = (*dst_data & 0x00FFFFFF) | (dst_alpha << 24);
		src_data ++;
		dst_data ++;
	}

	return psd_status_done;
}

psd_status psd_bitmap_find_edge(psd_bitmap * bitmap, psd_bool edge_hidden)
{
	psd_int i, alpha;
	psd_argb_color * dst_data;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;

	dst_data = bitmap->image_data;
	for(i = bitmap->width * bitmap->height; i --; )
	{
		alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
		if(edge_hidden == psd_true && alpha < 24)
			*dst_data = (*dst_data & 0x00FFFFFF) | (alpha * 10 << 24);
		else
			*dst_data = (*dst_data & 0x00FFFFFF) | 0xFF000000;
		dst_data ++;
	}
	
	return psd_status_done;
}

psd_status psd_bitmap_ajust_range(psd_bitmap * bitmap, psd_int range)
{
	psd_int i, value;
	psd_uchar range_table[256];
	psd_argb_color * dst_data;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;
	if(range > 100 || range < 1)
		return psd_status_invalid_range;

	if(range == 100)
		return psd_status_done;

	for(i = 0; i < 256; i ++)
	{
		value = i * 100 / range;
		range_table[i] = PSD_MIN(value, 255);
	}

	dst_data = bitmap->image_data;
	for(i = bitmap->width * bitmap->height; i --; )
	{
		*dst_data = (*dst_data & 0x00FFFFFF) | (range_table[PSD_GET_ALPHA_COMPONENT(*dst_data)] << 24);
		dst_data ++;
	}
	
	return psd_status_done;
}

psd_status psd_bitmap_blend_mask(psd_bitmap * bitmap, psd_layer_mask_info * layer_mask_info)
{
	psd_int i, j, left, right, top, bottom;
	psd_argb_color * image_data;
	psd_color_component * mask_data;
	psd_int default_color;
	
	if(bitmap == NULL)
		return psd_status_invalid_bitmap;
	if(layer_mask_info == NULL)
		return psd_status_invalid_layer_mask_info;

	default_color = layer_mask_info->default_color;
	if(default_color != 255)
	{
		if(layer_mask_info->left > 0)
		{
			left = PSD_MIN(layer_mask_info->left, bitmap->width);
			for(i = 0; i < bitmap->height; i ++)
			{
				image_data = bitmap->image_data + i * bitmap->width;
				if(default_color == 0)
				{
					for(j = 0; j < left; j ++, image_data ++)
						*image_data &= 0x00FFFFFF;
				}
				else
				{
					for(j = 0; j < left; j ++, image_data ++)
					{
						*image_data = (*image_data & 0x00FFFFFF) | 
							((PSD_GET_ALPHA_COMPONENT(*image_data) * default_color >> 8) << 24);
					}
				}
			}
		}

		if(layer_mask_info->right < bitmap->width)
		{
			right = PSD_MAX(layer_mask_info->right, 0);
			for(i = 0; i < bitmap->height; i ++)
			{
				image_data = bitmap->image_data + i * bitmap->width + right;
				if(default_color == 0)
				{
					for(j = right; j < bitmap->width; j ++, image_data ++)
						*image_data &= 0x00FFFFFF;
				}
				else
				{
					for(j = right; j < bitmap->width; j ++, image_data ++)
					{
						*image_data = (*image_data & 0x00FFFFFF) | 
							((PSD_GET_ALPHA_COMPONENT(*image_data) * default_color >> 8) << 24);
					}
				}
			}
		}

		if(layer_mask_info->top > 0)
		{
			top = PSD_MIN(layer_mask_info->top, bitmap->height);
			for(i = 0; i < top; i ++)
			{
				image_data = bitmap->image_data + i * bitmap->width;
				if(default_color == 0)
				{
					for(j = 0; j < bitmap->width; j ++, image_data ++)
						*image_data &= 0x00FFFFFF;
				}
				else
				{
					for(j = 0; j < bitmap->width; j ++, image_data ++)
					{
						*image_data = (*image_data & 0x00FFFFFF) | 
							((PSD_GET_ALPHA_COMPONENT(*image_data) * default_color >> 8) << 24);
					}
				}
			}
		}

		if(layer_mask_info->bottom < bitmap->height)
		{
			bottom = PSD_MAX(layer_mask_info->bottom, 0);
			for(i = bottom; i < bitmap->height; i ++)
			{
				image_data = bitmap->image_data + i * bitmap->width;
				if(default_color == 0)
				{
					for(j = 0; j < bitmap->width; j ++, image_data ++)
						*image_data &= 0x00FFFFFF;
				}
				else
				{
					for(j = 0; j < bitmap->width; j ++, image_data ++)
					{
						*image_data = (*image_data & 0x00FFFFFF) | 
							((PSD_GET_ALPHA_COMPONENT(*image_data) * default_color >> 8) << 24);
					}
				}
			}
		}
	}

	left = PSD_MAX(layer_mask_info->left, 0);
	right = PSD_MIN(layer_mask_info->right, bitmap->width);
	top = PSD_MAX(layer_mask_info->top, 0);
	bottom = PSD_MIN(layer_mask_info->bottom, bitmap->height);
	if(left >= right || top >= bottom)
		return psd_status_done;

	for(i = top; i < bottom; i ++)
	{
		image_data = bitmap->image_data + i * bitmap->width + left;
		mask_data = layer_mask_info->mask_data + (i - layer_mask_info->top) * 
			layer_mask_info->width + (left - layer_mask_info->left);
		for(j = left; j < right; j ++, image_data ++, mask_data ++)
		{
			switch(*mask_data)
			{
				case 0:
					*image_data &= 0x00FFFFFF;
					break;
				case 255:
					continue;
				default:
					*image_data = (*image_data & 0x00FFFFFF) | 
						((PSD_GET_ALPHA_COMPONENT(*image_data) * *mask_data >> 8) << 24);
					break;
			}
		}
	}
	
	return psd_status_done;
}

psd_argb_color psd_bitmap_get_pixel(psd_bitmap * bitmap, psd_int x, psd_int y)
{
	if(x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height)
		return psd_color_clear;
	return *(bitmap->image_data + y * bitmap->width + x);
}

psd_argb_color psd_bitmap_get_fixed_pixel(psd_bitmap * bitmap, psd_fixed_16_16 x, psd_fixed_16_16 y)
{
	psd_argb_color * buffer;
	psd_int flrx, flry;
	psd_argb_color c1, c2, c3, c4;
	psd_int flr1, flr2, flr3, flr4;
	psd_int alpha_green, red_blue;

	if(x < 0)
		x = 0;
	if(y < 0)
		y = 0;
	if(x > PSD_FIXED_16_16_INT(bitmap->width - 1))
		x = PSD_FIXED_16_16_INT(bitmap->width - 1);
	if(y > PSD_FIXED_16_16_INT(bitmap->height - 1))
		y = PSD_FIXED_16_16_INT(bitmap->height - 1);

	buffer = bitmap->image_data + PSD_FIXED_16_16_FLOOR(y) * bitmap->width + 
		PSD_FIXED_16_16_FLOOR(x);
	flrx = (x >> 10) & 63;
	flry = (y >> 10) & 63;

	if(flrx == 0)
	{
		if(flry == 0)
		{
			return *buffer;
		}
		else
		{
			c1 = *buffer;
			buffer += bitmap->width;
			c2 = *buffer;
			flr1 = 64 - flry;
			flr2 = flry;
			alpha_green = ((((c1 & 0xFF00FF00) >> 8) * flr1 + ((c2 & 0xFF00FF00) >> 8) * flr2) << 2) & 0xFF00FF00;
			red_blue = (((c1 & 0x00FF00FF) * flr1 + (c2 & 0x00FF00FF) * flr2) >> 6) & 0x00FF00FF;
		}
	}
	else
	{
		if(flry == 0)
		{
			c1 = *buffer;
			buffer ++;
			c2 = *buffer;
			flr1 = 64 - flrx;
			flr2 = flrx;
			alpha_green = ((((c1 & 0xFF00FF00) >> 8) * flr1 + ((c2 & 0xFF00FF00) >> 8) * flr2) << 2) & 0xFF00FF00;
			red_blue = (((c1 & 0x00FF00FF) * flr1 + (c2 & 0x00FF00FF) * flr2) >> 6) & 0x00FF00FF;
		}
		else
		{
			c1 = *buffer;
			buffer ++;
			c2 = *buffer;
			buffer += bitmap->width;
			c3 = *buffer;
			buffer --;
			c4 = *buffer;
			flr1 = (64 - flrx) + (64 - flry);
			flr2 = flrx + (64 - flry);
			flr3 = flrx + flry;
			flr4 = (64 - flrx) + flry;
			alpha_green = (((c1 & 0xFF00FF00) >> 8) * flr1 + ((c2 & 0xFF00FF00) >> 8) * flr2 +
				((c3 & 0xFF00FF00) >> 8) * flr3 + ((c4 & 0xFF00FF00) >> 8) * flr4) & 0xFF00FF00;
			red_blue = (((c1 & 0x00FF00FF) * flr1 + (c2 & 0x00FF00FF) * flr2 +
				(c3 & 0x00FF00FF) * flr3 + (c4 & 0x00FF00FF) * flr4) >> 8) & 0x00FF00FF;
		}
	}

	return (alpha_green | red_blue);
}

