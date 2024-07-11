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
 * $Id: drop_shadow.c, created by Patrick in 2006.05.23, libpsd@graphest.com Exp $
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
extern void psd_effects_add_noise(psd_bitmap * bitmap, psd_int noise, psd_int left, psd_int top, psd_context * context);


// set default value
static void psd_set_layer_drop_shadow_default(psd_layer_effects_drop_shadow * drop_shadow)
{
	psd_int i;

	drop_shadow->blend_mode = psd_blend_mode_multiply;
	drop_shadow->color = drop_shadow->native_color = psd_color_black;
	drop_shadow->opacity = 191;
	drop_shadow->angle = 120;
	drop_shadow->use_global_light = psd_true;
	drop_shadow->distance = 5;
	drop_shadow->spread = 0;
	drop_shadow->size = 5;
	
	for(i = 0; i < 256; i ++)
		drop_shadow->contour_lookup_table[i] = i;
	drop_shadow->anti_aliased = psd_false;
	drop_shadow->noise = 0;
	
	drop_shadow->knocks_out = psd_false;
}

psd_status psd_get_layer_drop_shadow(psd_context * context, psd_layer_effects_drop_shadow * drop_shadow)
{
	psd_int size, version;
	psd_uint tag;

	psd_set_layer_drop_shadow_default(drop_shadow);
	
	// Size of the remaining items: 41 or 51 (depending on version)
	size = psd_stream_get_int(context);
	
	// Version: 0 (Photoshop 5.0) or 2 (Photoshop 5.5)
	version = psd_stream_get_int(context);
	if(version != 0 && version != 2)
		return psd_status_shadow_unsupport_version;

	// Blur value in pixels
	drop_shadow->size = psd_stream_get_short(context);		// i don't know, but it is psd_short

	// Intensity as a percent
	drop_shadow->spread = psd_stream_get_int(context);

	// Angle in degrees
	drop_shadow->angle = psd_stream_get_int(context);

	// Distance in pixels
	drop_shadow->distance = psd_stream_get_int(context);

	// maybe photoshop is wrong
	psd_stream_get_short(context);
	
	// Color
	drop_shadow->color = psd_stream_get_space_color(context);

	// Blend mode: 4 bytes for signature and 4 bytes for key
	// Blend mode signature: '8BIM'
	tag = psd_stream_get_int(context);
	if(tag != '8BIM')
		return psd_status_blend_mode_signature_error;
	
	// Blend mode key
	drop_shadow->blend_mode = psd_stream_get_blend_mode(context);

	// Effect enabled
	drop_shadow->effect_enable = psd_stream_get_bool(context);

	// Use this angle in all of the drop_shadow effects
	drop_shadow->use_global_light = psd_stream_get_bool(context);

	// Opacity as a percent
	drop_shadow->opacity = psd_stream_get_char(context);

	// Version 2 only
	if(version == 2)
	{
		// Native color
		drop_shadow->native_color = psd_stream_get_space_color(context);
	}

	return psd_status_done;
}

psd_status psd_get_layer_drop_shadow2(psd_context * context, psd_layer_effects_drop_shadow * drop_shadow)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_drop_shadow_default(drop_shadow);

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
			case 0:
				// layer knocks out drop shadow
				if(strcmp(keychar, "layerConceals") == 0)
				{
					psd_assert(type == 'bool');
					drop_shadow->knocks_out = psd_stream_get_bool(context);
				}
				else
				{
					psd_assert(0);
				}
				break;
				
			// effect enable
			case 'enab':
				psd_assert(type == 'bool');
				drop_shadow->effect_enable = psd_stream_get_bool(context);
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
				drop_shadow->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// color or native color
			case 'Clr ':
				// Descriptor
				psd_assert(type == 'Objc');
				drop_shadow->color = drop_shadow->native_color = psd_stream_get_object_color(context);
				break;

			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				drop_shadow->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// use global light
			case 'uglg':
				psd_assert(type == 'bool');
				drop_shadow->use_global_light = psd_stream_get_bool(context);
				break;

			// angle
			case 'lagl':
				psd_assert(type == 'UntF');
				// angle: base degrees
				key = psd_stream_get_int(context);
				psd_assert(key == '#Ang');
				drop_shadow->angle = (psd_int)psd_stream_get_double(context);
				break;

			// distance
			case 'Dstn':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				drop_shadow->distance = (psd_int)psd_stream_get_double(context);
				break;

			// spread
			case 'Ckmt':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				drop_shadow->spread = (psd_int)psd_stream_get_double(context);
				break;

			// size
			case 'blur':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				drop_shadow->size = (psd_int)psd_stream_get_double(context);
				break;

			// noise
			case 'Nose':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				drop_shadow->noise = (psd_int)psd_stream_get_double(context);
				break;

			// anti-aliased
			case 'AntA':
				psd_assert(type == 'bool');
				drop_shadow->anti_aliased = psd_stream_get_bool(context);
				break;

			// contour
			case 'TrnS':
				psd_assert(type == 'Objc');
				psd_stream_get_object_contour(drop_shadow->contour_lookup_table, context);
				break;

			default:
				psd_assert(0);
				break;
		}
	}

	return psd_status_done;
}

static psd_float * psd_shadow_create_blur_filter(psd_int radius)
{
	psd_float * filter, sum;
	psd_int size, i, j, x, y;

	size = radius * 2 + 1;
	filter = (psd_float *)psd_malloc(size * size * sizeof(psd_float));
	if(filter == NULL)
		return NULL;

	sum = 0.0;
	for(i = 0; i < size; i ++)
	{
		y = i - size;
		for(j = 0; j < size; j ++)
		{
			x = j - size;
			sum += filter[i * size + j] = (psd_float)((1 / (2 * PSD_PI * size)) *
           		exp ((- (x * x + y * y)) / (2 * size * size)));
		}
	}

	for(i = 0; i < size; i ++)
	{
		for(j = 0; j < size; j ++)
		{
			filter[i * size + j] /= sum;
		}
	}

	return filter;
}

#ifdef PSD_SUPPORT_EFFECTS_BLEND
psd_status psd_layer_effects_blend_drop_shadow(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_drop_shadow * drop_shadow = &data->drop_shadow;
	psd_int width, height;
	psd_int angle;
	psd_int distance_x, distance_y;
	psd_bitmap src_bmp, dst_bmp, knock_bmp;
	psd_layer_mask_info layer_mask_info;
	psd_int spread_size, blur_size;
	
	if(drop_shadow->use_global_light == psd_true)
		angle = context->global_angle;
	else
		angle = drop_shadow->angle;
	distance_x = -(psd_int)(drop_shadow->distance * cos(PSD_PI * angle / 180) + 0.5);
	distance_y = (psd_int)(drop_shadow->distance * sin(PSD_PI * angle / 180) + 0.5);
	
	data->left[psd_layer_effects_type_drop_shadow] = -drop_shadow->size + distance_x;
	data->top[psd_layer_effects_type_drop_shadow] = -drop_shadow->size + distance_y;
	width = layer->width + drop_shadow->size * 2;
	height = layer->height + drop_shadow->size * 2;
	data->right[psd_layer_effects_type_drop_shadow] = data->left[psd_layer_effects_type_drop_shadow] + width;
	data->bottom[psd_layer_effects_type_drop_shadow] = data->top[psd_layer_effects_type_drop_shadow] + height;
	data->blend_mode[psd_layer_effects_type_drop_shadow] = drop_shadow->blend_mode;
	data->opacity[psd_layer_effects_type_drop_shadow] = drop_shadow->opacity;

	if(data->image_data[psd_layer_effects_type_drop_shadow] != NULL)
	{
		if(data->width[psd_layer_effects_type_drop_shadow] != width ||
			data->height[psd_layer_effects_type_drop_shadow] != height)
		{
			psd_free(data->image_data[psd_layer_effects_type_drop_shadow]);
			data->image_data[psd_layer_effects_type_drop_shadow] = (psd_argb_color *)psd_malloc(width * height * 4);
			if(data->image_data[psd_layer_effects_type_drop_shadow] == NULL)
				return psd_status_malloc_failed;
		}
	}
	else
	{
		data->image_data[psd_layer_effects_type_drop_shadow] = (psd_argb_color *)psd_malloc(width * height * 4);
		if(data->image_data[psd_layer_effects_type_drop_shadow] == NULL)
			return psd_status_malloc_failed;
	}
	data->width[psd_layer_effects_type_drop_shadow] = width;
	data->height[psd_layer_effects_type_drop_shadow] = height;
	psd_color_memset(data->image_data[psd_layer_effects_type_drop_shadow], drop_shadow->color, width * height);

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	dst_bmp.width = width;
	dst_bmp.height = height;
	dst_bmp.image_data = data->image_data[psd_layer_effects_type_drop_shadow];

	if(drop_shadow->size == 0)
	{
		psd_fill_bitmap(&dst_bmp, drop_shadow->color);
		psd_bitmap_copy_alpha_channel(&dst_bmp, &src_bmp);
	}
	else
	{
		psd_inflate_bitmap(&dst_bmp, &src_bmp, drop_shadow->size, drop_shadow->size);
		psd_fill_bitmap_without_alpha_channel(&dst_bmp, drop_shadow->color);
	}

	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		layer_mask_info.left -= layer->left - drop_shadow->size;
		layer_mask_info.top -= layer->top - drop_shadow->size;
		layer_mask_info.right -= layer->left - drop_shadow->size;
		layer_mask_info.bottom -= layer->top - drop_shadow->size;
		psd_bitmap_blend_mask(&dst_bmp, &layer_mask_info);
	}

	if(drop_shadow->knocks_out == psd_true)
	{
		psd_create_bitmap(&knock_bmp, width, height);
		psd_copy_bitmap(&knock_bmp, &dst_bmp);
		psd_offset_bitmap(&knock_bmp, distance_x, distance_y, psd_color_clear);
	}
	
	spread_size = (drop_shadow->spread * drop_shadow->size + 50) / 100;
	blur_size = drop_shadow->size - spread_size;
	if(spread_size != 0)
	{
		psd_bitmap_gaussian_blur_alpha_channel(&dst_bmp, spread_size);
		psd_bitmap_find_edge(&dst_bmp, psd_true);
	}
	if(blur_size != 0)
	{
		psd_bitmap_gaussian_blur_alpha_channel(&dst_bmp, blur_size);
	}

	psd_bitmap_contour_alpha_channel(&dst_bmp, drop_shadow->contour_lookup_table, 
		drop_shadow->anti_aliased, psd_true);

	if(drop_shadow->noise > 0)
	{
		psd_effects_add_noise(&dst_bmp, drop_shadow->noise, 
			data->left[psd_layer_effects_type_drop_shadow] + layer->left,
			data->top[psd_layer_effects_type_drop_shadow] + layer->top, context);
	}

	if(drop_shadow->knocks_out == psd_true)
	{
		psd_bitmap_knock_out(&dst_bmp, &knock_bmp);
		psd_free_bitmap(&knock_bmp);
	}
	
	data->valid[psd_layer_effects_type_drop_shadow] = psd_false;
	
	return psd_status_done;
}
#endif
