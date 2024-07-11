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
 * $Id: inner_glow.c, created by Patrick in 2006.06.18, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_descriptor.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_bitmap.h"


extern void psd_bitmap_gaussian_blur_alpha_channel(psd_bitmap * bitmap, psd_double radius);
extern void psd_effects_add_noise(psd_bitmap * bitmap, psd_int noise, psd_int left, psd_int top, psd_context * context);
extern void psd_gradient_color_get_table(psd_gradient_color * gradient_color, psd_argb_color * color_table, psd_int table_count, psd_bool reverse);
extern void psd_effects_apply_gradient(psd_bitmap * bitmap, psd_argb_color * gradient_table, psd_bool edge_hidden, 
	psd_int jitter, psd_int left, psd_int top, psd_context * context);


// set default value
static void psd_set_layer_inner_glow_default(psd_layer_effects_inner_glow * inner_glow)
{
	psd_int i;

	inner_glow->blend_mode = psd_blend_mode_screen;
	inner_glow->opacity = 191;
	inner_glow->noise = 0;
	inner_glow->fill_type = psd_fill_solid_color;
	inner_glow->color = inner_glow->native_color = 0XFFFFFFBE;
	
	inner_glow->technique = psd_technique_softer;
	inner_glow->source = psd_glow_edge;
	inner_glow->choke = 0;
	inner_glow->size = 5;
	
	for(i = 0; i < 256; i ++)
		inner_glow->contour_lookup_table[i] = i;
	inner_glow->anti_aliased = psd_false;
	inner_glow->range = 50;
	inner_glow->jitter = 0;
}

psd_status psd_get_layer_inner_glow(psd_context * context, psd_layer_effects_inner_glow * inner_glow)
{
	psd_int size, version;
	psd_uint tag;
	psd_bool invert;

	psd_set_layer_inner_glow_default(inner_glow);

	// Size of the remaining items: 32 for Photoshop 5.0; 43 for 5.5
	size = psd_stream_get_int(context);
	
	// Version: 0 for Photoshop 5.0; 2 for 5.5
	version = psd_stream_get_int(context);
	if(version != 0 && version != 2)
		return psd_status_inner_glow_unsupport_version;

	// Blur value in pixels.
	inner_glow->size = psd_stream_get_int(context);

	// Intensity as a percent
	inner_glow->choke = psd_stream_get_int(context);

	// Color
	inner_glow->color = psd_stream_get_space_color(context);
	
	// Blend mode: 4 bytes for signature and 4 bytes for the key
	// Blend mode signature: '8BIM'
	tag = psd_stream_get_int(context);
	if(tag != '8BIM')
		return psd_status_blend_mode_signature_error;
	
	// Blend mode key
	inner_glow->blend_mode = psd_stream_get_blend_mode(context);

	// Effect enabled
	inner_glow->effect_enable = psd_stream_get_bool(context);

	// Opacity as a percent
	inner_glow->opacity = psd_stream_get_char(context);

	// Version 2 only
	if(version == 2)
	{
		// Invert
		invert = psd_stream_get_bool(context);
		if(invert == psd_true)
			inner_glow->source = psd_glow_center;
		
		// Native color
		inner_glow->native_color = psd_stream_get_space_color(context);
	}

	return psd_status_done;
}

psd_status psd_get_layer_inner_glow2(psd_context * context, psd_layer_effects_inner_glow * inner_glow)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_inner_glow_default(inner_glow);

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
				inner_glow->effect_enable = psd_stream_get_bool(context);
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
				inner_glow->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// color or native color
			case 'Clr ':
				// Descriptor
				psd_assert(type == 'Objc');
				inner_glow->color = inner_glow->native_color = psd_stream_get_object_color(context);
				inner_glow->fill_type = psd_fill_solid_color;
				break;

			// gradient color
			case 'Grad':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_stream_get_object_gradient_color(&inner_glow->gradient_color, context);
				inner_glow->fill_type = psd_fill_gradient;
				break;

			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				inner_glow->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// technique
			case 'GlwT':
				psd_assert(type == 'enum');
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// Matte Technique
				psd_assert(key == 'BETE');
				inner_glow->technique = psd_stream_get_object_technique(context);
				break;

			// choke
			case 'Ckmt':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				inner_glow->choke = (psd_int)psd_stream_get_double(context);
				break;

			// size
			case 'blur':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				inner_glow->size = (psd_int)psd_stream_get_double(context);
				break;

			// jitter
			case 'ShdN':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				inner_glow->jitter = (psd_int)psd_stream_get_double(context);
				break;

			// noise
			case 'Nose':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				inner_glow->noise = (psd_int)psd_stream_get_double(context);
				break;

			// anti-aliased
			case 'AntA':
				psd_assert(type == 'bool');
				inner_glow->anti_aliased = psd_stream_get_bool(context);
				break;

			// source
			case 'glwS':
				psd_assert(type == 'enum');
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// inner glow source
				psd_assert(key == 'IGSr');

				length = psd_stream_get_int(context);
				if(length == 0)
					key = psd_stream_get_int(context);
				else
				{
					key = 0;
					psd_stream_get(context, keychar, length);
					keychar[length] = 0;
				}
				switch(key)
				{
					case 'SrcC':
						inner_glow->source = psd_glow_center;
						break;
					case 'SrcE':
						inner_glow->source = psd_glow_edge;
						break;
					default:
						psd_assert(0);
						break;
				}
				break;

			// contour
			case 'TrnS':
				psd_assert(type == 'Objc');
				psd_stream_get_object_contour(inner_glow->contour_lookup_table, context);
				break;

			// range
			case 'Inpr':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				inner_glow->range = (psd_int)psd_stream_get_double(context);
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
psd_status psd_layer_effects_blend_inner_glow(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_inner_glow * inner_glow = &data->inner_glow;
	psd_int width, height;
	psd_bitmap src_bmp, dst_bmp, knock_bmp;
	psd_layer_mask_info layer_mask_info;
	psd_int choke_size, blur_size;
	psd_argb_color gradient_table[256];
	
	data->left[psd_layer_effects_type_inner_glow] = -inner_glow->size;
	data->top[psd_layer_effects_type_inner_glow] = -inner_glow->size;
	width = layer->width + inner_glow->size * 2;
	height = layer->height + inner_glow->size * 2;
	data->right[psd_layer_effects_type_inner_glow] = data->left[psd_layer_effects_type_inner_glow] + width;
	data->bottom[psd_layer_effects_type_inner_glow] = data->top[psd_layer_effects_type_inner_glow] + height;
	data->blend_mode[psd_layer_effects_type_inner_glow] = inner_glow->blend_mode;
	data->opacity[psd_layer_effects_type_inner_glow] = inner_glow->opacity;

	if(data->image_data[psd_layer_effects_type_inner_glow] != NULL)
	{
		if(data->width[psd_layer_effects_type_inner_glow] != width ||
			data->height[psd_layer_effects_type_inner_glow] != height)
		{
			psd_free(data->image_data[psd_layer_effects_type_inner_glow]);
			data->image_data[psd_layer_effects_type_inner_glow] = (psd_argb_color *)psd_malloc(width * height * 4);
			if(data->image_data[psd_layer_effects_type_inner_glow] == NULL)
				return psd_status_malloc_failed;
		}
	}
	else
	{
		data->image_data[psd_layer_effects_type_inner_glow] = (psd_argb_color *)psd_malloc(width * height * 4);
		if(data->image_data[psd_layer_effects_type_inner_glow] == NULL)
			return psd_status_malloc_failed;
	}
	data->width[psd_layer_effects_type_inner_glow] = width;
	data->height[psd_layer_effects_type_inner_glow] = height;
	psd_color_memset(data->image_data[psd_layer_effects_type_inner_glow], inner_glow->color, width * height);

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	dst_bmp.width = width;
	dst_bmp.height = height;
	dst_bmp.image_data = data->image_data[psd_layer_effects_type_inner_glow];

	if(inner_glow->size == 0)
	{
		psd_fill_bitmap(&dst_bmp, inner_glow->color);
		psd_bitmap_copy_alpha_channel(&dst_bmp, &src_bmp);
	}
	else
	{
		psd_inflate_bitmap(&dst_bmp, &src_bmp, inner_glow->size, inner_glow->size);
		psd_fill_bitmap_without_alpha_channel(&dst_bmp, inner_glow->color);
	}

	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		layer_mask_info.left -= layer->left - inner_glow->size;
		layer_mask_info.top -= layer->top - inner_glow->size;
		layer_mask_info.right -= layer->left - inner_glow->size;
		layer_mask_info.bottom -= layer->top - inner_glow->size;
		psd_bitmap_blend_mask(&dst_bmp, &layer_mask_info);
	}

	psd_bitmap_reverse_alpha_channel(&dst_bmp);

	psd_create_bitmap(&knock_bmp, width, height);
	psd_copy_bitmap(&knock_bmp, &dst_bmp);

	if(inner_glow->technique == psd_technique_precise)
		psd_bitmap_find_edge(&dst_bmp, psd_false);
	
	choke_size = (inner_glow->choke * inner_glow->size + 50) / 100;
	blur_size = inner_glow->size - choke_size;
	if(choke_size != 0)
	{
		psd_bitmap_gaussian_blur_alpha_channel(&dst_bmp, choke_size);
		psd_bitmap_find_edge(&dst_bmp, psd_false);
	}
	if(blur_size != 0)
	{
		psd_bitmap_gaussian_blur_alpha_channel(&dst_bmp, blur_size);
	}

	psd_bitmap_ajust_range(&dst_bmp, inner_glow->range);

	if(inner_glow->source == psd_glow_center)
		psd_bitmap_reverse_alpha_channel(&dst_bmp);

	psd_bitmap_contour_alpha_channel(&dst_bmp, inner_glow->contour_lookup_table, 
		inner_glow->anti_aliased, psd_false);

	if(inner_glow->noise > 0)
	{
		psd_effects_add_noise(&dst_bmp, inner_glow->noise, 
			data->left[psd_layer_effects_type_inner_glow] + layer->left,
			data->top[psd_layer_effects_type_inner_glow] + layer->top, context);
	}

	if(inner_glow->fill_type == psd_fill_gradient)
	{
		psd_gradient_color_get_table(&inner_glow->gradient_color, gradient_table, 256, psd_false);
		psd_effects_apply_gradient(&dst_bmp, gradient_table, psd_false, inner_glow->jitter, 
			data->left[psd_layer_effects_type_inner_glow] + layer->left,
			data->top[psd_layer_effects_type_inner_glow] + layer->top, context);
	}

	psd_bitmap_knock_out(&dst_bmp, &knock_bmp);
	psd_free_bitmap(&knock_bmp);
	
	data->valid[psd_layer_effects_type_inner_glow] = psd_false;
	
	return psd_status_done;
}
#endif

