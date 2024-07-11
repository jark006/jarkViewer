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
 * $Id: color_overlay.c, created by Patrick in 2006.05.23, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_descriptor.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_bitmap.h"


static void psd_set_layer_color_overlay_default(psd_layer_effects_color_overlay * color_overlay)
{
	color_overlay->blend_mode = psd_blend_mode_normal;
	color_overlay->color = psd_color_red;
	color_overlay->opacity = 255;
}

psd_status psd_get_layer_color_overlay(psd_context * context, psd_layer_effects_color_overlay * color_overlay)
{
	psd_int size, version;
	psd_uint tag;

	psd_set_layer_color_overlay_default(color_overlay);

	// Size: 34
	size = psd_stream_get_int(context);
	
	// Version: 2
	version = psd_stream_get_int(context);
	if(version != 2)
		return psd_status_solid_fill_unsupport_version;

	tag = psd_stream_get_int(context);
	if(tag != '8BIM')
		return psd_status_blend_mode_signature_error;
	
	// Key for blend mode
	color_overlay->blend_mode = psd_stream_get_blend_mode(context);

	// Color space
	color_overlay->color = psd_stream_get_space_color(context);

	// Opacity
	color_overlay->opacity = psd_stream_get_char(context);

	// Enabled
	color_overlay->effect_enable = psd_stream_get_bool(context);

	// Native color space
	color_overlay->native_color = psd_stream_get_space_color(context);

	return psd_status_done;
}

psd_status psd_get_layer_color_overlay2(psd_context * context, psd_layer_effects_color_overlay * color_overlay)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_color_overlay_default(color_overlay);

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
				color_overlay->effect_enable = psd_stream_get_bool(context);
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
				color_overlay->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				color_overlay->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;
				
			// color or native color
			case 'Clr ':
				// Descriptor
				psd_assert(type == 'Objc');
				color_overlay->color = color_overlay->native_color = psd_stream_get_object_color(context);
				break;

			default:
				psd_assert(0);
				break;
		}
	}

	return psd_status_done;
}

#ifdef PSD_SUPPORT_EFFECTS_BLEND
psd_status psd_layer_effects_blend_color_overlay(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_color_overlay * color_overlay = &data->color_overlay;
	psd_bitmap src_bmp, dst_bmp;
	psd_layer_mask_info layer_mask_info;

	data->left[psd_layer_effects_type_color_overlay] = 0;
	data->top[psd_layer_effects_type_color_overlay] = 0;
	data->right[psd_layer_effects_type_color_overlay] = layer->width;
	data->bottom[psd_layer_effects_type_color_overlay] = layer->height;
	data->blend_mode[psd_layer_effects_type_color_overlay] = color_overlay->blend_mode;
	data->opacity[psd_layer_effects_type_color_overlay] = color_overlay->opacity;
	data->width[psd_layer_effects_type_color_overlay] = layer->width;
	data->height[psd_layer_effects_type_color_overlay] = layer->height;

	if(data->image_data[psd_layer_effects_type_color_overlay] == NULL)
	{
		data->image_data[psd_layer_effects_type_color_overlay] = (psd_argb_color *)psd_malloc(layer->width * layer->height * 4);
		if(data->image_data[psd_layer_effects_type_color_overlay] == NULL)
			return psd_status_malloc_failed;
	}

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	dst_bmp.width = layer->width;
	dst_bmp.height = layer->height;
	dst_bmp.image_data = data->image_data[psd_layer_effects_type_color_overlay];

	psd_fill_bitmap(&dst_bmp, color_overlay->color);
	psd_bitmap_copy_alpha_channel(&dst_bmp, &src_bmp);
	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		layer_mask_info.left -= layer->left;
		layer_mask_info.top -= layer->top;
		layer_mask_info.right -= layer->left;
		layer_mask_info.bottom -= layer->top;
		psd_bitmap_blend_mask(&dst_bmp, &layer_mask_info);
	}

	data->valid[psd_layer_effects_type_color_overlay] = psd_false;
	
	return psd_status_done;
}
#endif

