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
 * $Id: inner_shadow.c, created by Patrick in 2006.06.16, libpsd@graphest.com Exp $
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
static void psd_set_layer_inner_shadow_default(psd_layer_effects_inner_shadow * inner_shadow)
{
	psd_int i;

	inner_shadow->blend_mode = psd_blend_mode_multiply;
	inner_shadow->color = inner_shadow->native_color = psd_color_black;
	inner_shadow->opacity = 191;
	inner_shadow->angle = 120;
	inner_shadow->use_global_light = psd_true;
	inner_shadow->distance = 5;
	inner_shadow->choke = 0;
	inner_shadow->size = 5;
	
	for(i = 0; i < 256; i ++)
		inner_shadow->contour_lookup_table[i] = i;
	inner_shadow->anti_aliased = psd_false;
	inner_shadow->noise = 0;
}

psd_status psd_get_layer_inner_shadow(psd_context * context, psd_layer_effects_inner_shadow * inner_shadow)
{
	psd_int size, version;
	psd_uint tag;

	psd_set_layer_inner_shadow_default(inner_shadow);

	// Size of the remaining items: 41 or 51 (depending on version)
	size = psd_stream_get_int(context);
	
	// Version: 0 (Photoshop 5.0) or 2 (Photoshop 5.5)
	version = psd_stream_get_int(context);
	if(version != 0 && version != 2)
		return psd_status_shadow_unsupport_version;

	// Blur value in pixels
	inner_shadow->size = psd_stream_get_short(context);		// i don't know, but it is psd_short

	// Intensity as a percent
	inner_shadow->choke = psd_stream_get_int(context);

	// Angle in degrees
	inner_shadow->angle = psd_stream_get_int(context);

	// Distance in pixels
	inner_shadow->distance = psd_stream_get_int(context);

	// maybe photoshop is wrong
	psd_stream_get_short(context);
	
	// Color
	inner_shadow->color = psd_stream_get_space_color(context);

	// Blend mode: 4 bytes for signature and 4 bytes for key
	// Blend mode signature: '8BIM'
	tag = psd_stream_get_int(context);
	if(tag != '8BIM')
		return psd_status_blend_mode_signature_error;
	
	// Blend mode key
	inner_shadow->blend_mode = psd_stream_get_blend_mode(context);

	// Effect enabled
	inner_shadow->effect_enable = psd_stream_get_bool(context);

	// Use this angle in all of the inner_shadow effects
	inner_shadow->use_global_light = psd_stream_get_bool(context);

	// Opacity as a percent
	inner_shadow->opacity = psd_stream_get_char(context);

	// Version 2 only
	if(version == 2)
	{
		// Native color
		inner_shadow->native_color = psd_stream_get_space_color(context);
	}

	return psd_status_done;
}

psd_status psd_get_layer_inner_shadow2(psd_context * context, psd_layer_effects_inner_shadow * inner_shadow)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_inner_shadow_default(inner_shadow);

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
		psd_assert(length == 0);
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
				inner_shadow->effect_enable = psd_stream_get_bool(context);
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
				inner_shadow->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// color or native color
			case 'Clr ':
				// Descriptor
				psd_assert(type == 'Objc');
				inner_shadow->color = inner_shadow->native_color = psd_stream_get_object_color(context);
				break;

			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				inner_shadow->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// use global light
			case 'uglg':
				psd_assert(type == 'bool');
				inner_shadow->use_global_light = psd_stream_get_bool(context);
				break;

			// angle
			case 'lagl':
				psd_assert(type == 'UntF');
				// angle: base degrees
				key = psd_stream_get_int(context);
				psd_assert(key == '#Ang');
				inner_shadow->angle = (psd_int)psd_stream_get_double(context);
				break;

			// distance
			case 'Dstn':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				inner_shadow->distance = (psd_int)psd_stream_get_double(context);
				break;

			// choke
			case 'Ckmt':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				inner_shadow->choke = (psd_int)psd_stream_get_double(context);
				break;

			// size
			case 'blur':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				inner_shadow->size = (psd_int)psd_stream_get_double(context);
				break;

			// noise
			case 'Nose':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				inner_shadow->noise = (psd_int)psd_stream_get_double(context);
				break;

			// anti-aliased
			case 'AntA':
				psd_assert(type == 'bool');
				inner_shadow->anti_aliased = psd_stream_get_bool(context);
				break;

			// contour
			case 'TrnS':
				psd_assert(type == 'Objc');
				psd_stream_get_object_contour(inner_shadow->contour_lookup_table, context);
				break;

			default:
				psd_assert(0);
				psd_stream_get_object_null(type, context);
				break;
		}
	}

	return psd_status_done;
}

#ifdef PSD_SUPPORT_EFFECTS_BLEND
psd_status psd_layer_effects_blend_inner_shadow(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_inner_shadow * inner_shadow = &data->inner_shadow;
	psd_int width, height;
	psd_int angle;
	psd_int distance_x, distance_y;
	psd_bitmap src_bmp, dst_bmp, knock_bmp;
	psd_layer_mask_info layer_mask_info;
	psd_int choke_size, blur_size;
	
	if(inner_shadow->use_global_light == psd_true)
		angle = context->global_angle;
	else
		angle = inner_shadow->angle;
	distance_x = -(psd_int)(inner_shadow->distance * cos(PSD_PI * angle / 180) + 0.5);
	distance_y = (psd_int)(inner_shadow->distance * sin(PSD_PI * angle / 180) + 0.5);
	
	data->left[psd_layer_effects_type_inner_shadow] = -inner_shadow->size + distance_x - PSD_ABS(distance_x);
	data->top[psd_layer_effects_type_inner_shadow] = -inner_shadow->size + distance_y - PSD_ABS(distance_y);
	width = layer->width + inner_shadow->size * 2 + PSD_ABS(distance_x) * 2;
	height = layer->height + inner_shadow->size * 2 + PSD_ABS(distance_y) * 2;
	data->right[psd_layer_effects_type_inner_shadow] = data->left[psd_layer_effects_type_inner_shadow] + width;
	data->bottom[psd_layer_effects_type_inner_shadow] = data->top[psd_layer_effects_type_inner_shadow] + height;
	data->blend_mode[psd_layer_effects_type_inner_shadow] = inner_shadow->blend_mode;
	data->opacity[psd_layer_effects_type_inner_shadow] = inner_shadow->opacity;

	if(data->image_data[psd_layer_effects_type_inner_shadow] != NULL)
	{
		if(data->width[psd_layer_effects_type_inner_shadow] != width ||
			data->height[psd_layer_effects_type_inner_shadow] != height)
		{
			psd_free(data->image_data[psd_layer_effects_type_inner_shadow]);
			data->image_data[psd_layer_effects_type_inner_shadow] = (psd_argb_color *)psd_malloc(width * height * 4);
			if(data->image_data[psd_layer_effects_type_inner_shadow] == NULL)
				return psd_status_malloc_failed;
		}
	}
	else
	{
		data->image_data[psd_layer_effects_type_inner_shadow] = (psd_argb_color *)psd_malloc(width * height * 4);
		if(data->image_data[psd_layer_effects_type_inner_shadow] == NULL)
			return psd_status_malloc_failed;
	}
	data->width[psd_layer_effects_type_inner_shadow] = width;
	data->height[psd_layer_effects_type_inner_shadow] = height;
	psd_color_memset(data->image_data[psd_layer_effects_type_inner_shadow], inner_shadow->color, width * height);

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	dst_bmp.width = width;
	dst_bmp.height = height;
	dst_bmp.image_data = data->image_data[psd_layer_effects_type_inner_shadow];

	if(inner_shadow->size == 0 && inner_shadow->distance == 0)
	{
		psd_fill_bitmap(&dst_bmp, inner_shadow->color);
		psd_bitmap_copy_alpha_channel(&dst_bmp, &src_bmp);
	}
	else
	{
		psd_inflate_bitmap(&dst_bmp, &src_bmp, inner_shadow->size + PSD_ABS(distance_x), 
			inner_shadow->size + PSD_ABS(distance_y));
		psd_fill_bitmap_without_alpha_channel(&dst_bmp, inner_shadow->color);
	}
	
	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		layer_mask_info.left -= layer->left - inner_shadow->size - PSD_ABS(distance_x);
		layer_mask_info.top -= layer->top - inner_shadow->size - PSD_ABS(distance_y);
		layer_mask_info.right -= layer->left - inner_shadow->size - PSD_ABS(distance_x);
		layer_mask_info.bottom -= layer->top - inner_shadow->size - PSD_ABS(distance_y);
		psd_bitmap_blend_mask(&dst_bmp, &layer_mask_info);
	}

	psd_bitmap_reverse_alpha_channel(&dst_bmp);

	psd_create_bitmap(&knock_bmp, width, height);
	psd_copy_bitmap(&knock_bmp, &dst_bmp);
	psd_offset_bitmap(&knock_bmp, distance_x, distance_y, inner_shadow->color);
	
	choke_size = (inner_shadow->choke * inner_shadow->size + 50) / 100;
	blur_size = inner_shadow->size - choke_size;
	if(choke_size != 0)
	{
		psd_bitmap_gaussian_blur_alpha_channel(&dst_bmp, choke_size);
		psd_bitmap_find_edge(&dst_bmp, psd_false);
	}
	if(blur_size != 0)
	{
		psd_bitmap_gaussian_blur_alpha_channel(&dst_bmp, blur_size);
	}

	psd_bitmap_contour_alpha_channel(&dst_bmp, inner_shadow->contour_lookup_table, 
		inner_shadow->anti_aliased, psd_false);

	if(inner_shadow->noise > 0)
	{
		psd_effects_add_noise(&dst_bmp, inner_shadow->noise, 
			data->left[psd_layer_effects_type_inner_shadow] + layer->left,
			data->top[psd_layer_effects_type_inner_shadow] + layer->top, context);
	}

	psd_bitmap_knock_out(&dst_bmp, &knock_bmp);
	psd_free_bitmap(&knock_bmp);
	
	data->valid[psd_layer_effects_type_inner_shadow] = psd_false;
	
	return psd_status_done;
}
#endif

