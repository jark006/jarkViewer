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
 * $Id: satin.c, created by Patrick in 2006.06.18, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_descriptor.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_bitmap.h"
#include "psd_math.h"


extern void psd_bitmap_gaussian_blur_alpha_channel(psd_bitmap * bitmap, psd_double radius);


static void psd_set_layer_satin_default(psd_layer_effects_satin * satin)
{
	psd_int i;
	
	satin->blend_mode = psd_blend_mode_multiply;
	satin->color = psd_color_black;
	satin->opacity = 128;
	satin->angle = 19;
	satin->distance = 11;
	satin->size = 14;
	
	for(i = 0; i < 256; i ++)
		satin->contour_lookup_table[i] = i;
	satin->anti_aliased = psd_false;
	satin->invert = psd_true;
}

psd_status psd_get_layer_satin2(psd_context * context, psd_layer_effects_satin * satin)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_satin_default(satin);

	// Unicode string: name from classID
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);

	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID
	length = psd_stream_get_int(context);
	if(length == 0)
		psd_stream_get_int(context);
	else
		psd_stream_get_null(context, length);

	// Number of items in descriptor
	number_items = psd_stream_get_int(context);

	while(number_items --)
	{
		length = psd_stream_get_int(context);
		if(length == 0)
			rootkey = psd_stream_get_int(context);
		else
		{
			rootkey = 0;
			psd_stream_get(context, keychar, length);
			keychar[length] = 0;
		}
		// Type: OSType key
		type = psd_stream_get_int(context);

		switch(rootkey)
		{
			// effect enable
			case 'enab':
				psd_assert(type == 'bool');
				satin->effect_enable = psd_stream_get_bool(context);
				break;

			// blend mode
			case 'Md  ':
				psd_assert(type == 'enum');
				
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// blend mode
				psd_assert(key == 'BlnM');
				satin->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// color
			case 'Clr ':
				// Descriptor
				psd_assert(type == 'Objc');
				satin->color = psd_stream_get_object_color(context);
				break;
				
			// anti-aliased
			case 'AntA':
				psd_assert(type == 'bool');
				satin->anti_aliased = psd_stream_get_bool(context);
				break;

			// invert
			case 'Invr':
				psd_assert(type == 'bool');
				satin->invert = psd_stream_get_bool(context);
				break;
				
			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				satin->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// angle
			case 'lagl':
				psd_assert(type == 'UntF');
				// angle: base degrees
				key = psd_stream_get_int(context);
				psd_assert(key == '#Ang');
				satin->angle = (psd_int)psd_stream_get_double(context);
				break;

			// distance
			case 'Dstn':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				satin->distance = (psd_int)psd_stream_get_double(context);
				break;

			// size
			case 'blur':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				satin->size = (psd_int)psd_stream_get_double(context);
				break;

			// contour
			case 'MpgS':
				psd_assert(type == 'Objc');
				psd_stream_get_object_contour(satin->contour_lookup_table, context);
				break;

			default:
				psd_assert(0);
				psd_stream_get_object_null(type, context);
				break;
		}
	}

	return psd_status_done;
}

static void psd_satin_blend_offset(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, psd_bool invert, 
	psd_int offset_x, psd_int offset_y)
{
	psd_int i, j, left1, left2, top1, top2, right1, right2, bottom1, bottom2,
		x1, x2, y1, y2;
	psd_int alpha1, alpha2, dst_alpha;
	psd_argb_color * dst_data, * src_data1, * src_data2;
	
	if(PSD_ABS(offset_x) >= dst_bmp->width || PSD_ABS(offset_y) >= dst_bmp->height)
		return;

	if(offset_x > 0)
	{
		left1 = offset_x;
		right1 = src_bmp->width;
		x1 = 0;
		left2 = 0;
		right2 = src_bmp->width - offset_x;
		x2 = offset_x;
	}
	else
	{
		left1 = 0;
		right1 = src_bmp->width + offset_x;
		x1 = -offset_x;
		left2 = -offset_x;
		right2 = src_bmp->width;
		x2 = 0;
	}
	if(offset_y > 0)
	{
		top1 = offset_y;
		bottom1 = src_bmp->height;
		y1 = 0;
		top2 = 0;
		bottom2 = src_bmp->height - offset_y;
		y2 = offset_y;
	}
	else
	{
		top1 = 0;
		bottom1 = src_bmp->height + offset_y;
		y1 = -offset_y;
		top2 = -offset_y;
		bottom2 = src_bmp->height;
		y2 = 0;
	}

	dst_data = dst_bmp->image_data;
	for(i = 0; i < dst_bmp->height; i ++)
	{
		if(i >= top1 && i < bottom1)
			src_data1 = src_bmp->image_data + (i - top1 + y1) * src_bmp->width + x1;
		else
			src_data1 = NULL;
		if(i >= top2 && i < bottom2)
			src_data2 = src_bmp->image_data + (i - top2 + y2) * src_bmp->width + x2;
		else
			src_data2 = NULL;
		for(j = 0; j < dst_bmp->width; j ++)
		{
			if(src_data1 != NULL && j >= left1 && j < right1)
			{
				alpha1 = PSD_GET_ALPHA_COMPONENT(*src_data1);
				src_data1 ++;
			}
			else
			{
				alpha1 = 0;
			}
			if(src_data2 != NULL && j >= left2 && j < right2)
			{
				alpha2 = PSD_GET_ALPHA_COMPONENT(*src_data2);
				src_data2 ++;
			}
			else
			{
				alpha2 = 0;
			}

			dst_alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
			if(invert == psd_true)
				dst_alpha = dst_alpha * (255 - PSD_ABS(alpha1 - alpha2)) >> 8;
			else
				dst_alpha = dst_alpha * PSD_ABS(alpha1 - alpha2) >> 8;
			*dst_data = (*dst_data & 0x00FFFFFF) | (dst_alpha << 24);
			dst_data ++;
		}
	}
}

#ifdef PSD_SUPPORT_EFFECTS_BLEND
psd_status psd_layer_effects_blend_satin(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_satin * satin = &data->satin;
	psd_int width, height;
	psd_int distance_x, distance_y;
	psd_bitmap src_bmp, dst_bmp, knock_bmp, satin_bmp;
	psd_layer_mask_info layer_mask_info;

	data->left[psd_layer_effects_type_satin] = -satin->size;
	data->top[psd_layer_effects_type_satin] = -satin->size;
	width = layer->width + satin->size * 2;
	height = layer->height + satin->size * 2;
	data->right[psd_layer_effects_type_satin] = data->left[psd_layer_effects_type_satin] + width;
	data->bottom[psd_layer_effects_type_satin] = data->top[psd_layer_effects_type_satin] + height;
	data->blend_mode[psd_layer_effects_type_satin] = satin->blend_mode;
	data->opacity[psd_layer_effects_type_satin] = satin->opacity;

	if(data->image_data[psd_layer_effects_type_satin] != NULL)
	{
		if(data->width[psd_layer_effects_type_satin] != width ||
			data->height[psd_layer_effects_type_satin] != height)
		{
			psd_free(data->image_data[psd_layer_effects_type_satin]);
			data->image_data[psd_layer_effects_type_satin] = (psd_argb_color *)psd_malloc(width * height * 4);
			if(data->image_data[psd_layer_effects_type_satin] == NULL)
				return psd_status_malloc_failed;
		}
	}
	else
	{
		data->image_data[psd_layer_effects_type_satin] = (psd_argb_color *)psd_malloc(width * height * 4);
		if(data->image_data[psd_layer_effects_type_satin] == NULL)
			return psd_status_malloc_failed;
	}
	data->width[psd_layer_effects_type_satin] = width;
	data->height[psd_layer_effects_type_satin] = height;
	psd_color_memset(data->image_data[psd_layer_effects_type_satin], satin->color, width * height);

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	satin_bmp.width = width;
	satin_bmp.height = height;
	satin_bmp.image_data = data->image_data[psd_layer_effects_type_satin];
	psd_create_bitmap(&dst_bmp, width, height);

	if(satin->size == 0)
	{
		psd_fill_bitmap(&dst_bmp, satin->color);
		psd_bitmap_copy_alpha_channel(&dst_bmp, &src_bmp);
	}
	else
	{
		psd_inflate_bitmap(&dst_bmp, &src_bmp, satin->size, satin->size);
		psd_fill_bitmap_without_alpha_channel(&dst_bmp, satin->color);
	}

	psd_create_bitmap(&knock_bmp, width, height);
	psd_copy_bitmap(&knock_bmp, &dst_bmp);
	psd_bitmap_reverse_alpha_channel(&knock_bmp);

	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		layer_mask_info.left -= layer->left - satin->size;
		layer_mask_info.top -= layer->top - satin->size;
		layer_mask_info.right -= layer->left - satin->size;
		layer_mask_info.bottom -= layer->top - satin->size;
		psd_bitmap_blend_mask(&dst_bmp, &layer_mask_info);
	}

	psd_copy_bitmap(&satin_bmp, &dst_bmp);

	psd_bitmap_gaussian_blur_alpha_channel(&dst_bmp, satin->size);

	psd_bitmap_contour_alpha_channel(&dst_bmp, satin->contour_lookup_table, 
		satin->anti_aliased, psd_false);

	distance_x = -(psd_int)(satin->distance * cos(PSD_PI * satin->angle / 180) + 0.5);
	distance_y = (psd_int)(satin->distance * sin(PSD_PI * satin->angle / 180) + 0.5);
	psd_satin_blend_offset(&satin_bmp, &dst_bmp, satin->invert, distance_x, distance_y);

	psd_bitmap_knock_out(&satin_bmp, &knock_bmp);
	psd_free_bitmap(&knock_bmp);
	psd_free_bitmap(&dst_bmp);
	
	data->valid[psd_layer_effects_type_satin] = psd_false;
	
	return psd_status_done;
}
#endif

