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
 * $Id: pattern_overlay.c, created by Patrick in 2006.06.18, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_bitmap.h"
#include "psd_descriptor.h"


extern psd_status psd_pattern_fill(psd_bitmap * dst_bmp, psd_pattern * pattern, psd_int scale, psd_int offset_x, psd_int offset_y);


static void psd_set_layer_pattern_overlay_default(psd_layer_effects_pattern_overlay * pattern_overlay)
{
	pattern_overlay->blend_mode = psd_blend_mode_normal;
	pattern_overlay->opacity = 255;
	pattern_overlay->scale = 100;
	pattern_overlay->link_with_layer = psd_true;
}

psd_status psd_get_layer_pattern_overlay2(psd_context * context, psd_layer_effects_pattern_overlay * pattern_overlay)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_pattern_overlay_default(pattern_overlay);

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
				if(strcmp(keychar, "phase") == 0)
				{
					psd_assert(type == 'Objc');
					psd_stream_get_object_point(&pattern_overlay->horz_phase, 
						&pattern_overlay->vert_phase, context);
					// problem, why ???
					pattern_overlay->horz_phase = 0;
					pattern_overlay->vert_phase = 0;
				}
				else
				{
					psd_assert(0);
					psd_stream_get_object_null(type, context);
				}
				break;
				
			// effect enable
			case 'enab':
				psd_assert(type == 'bool');
				pattern_overlay->effect_enable = psd_stream_get_bool(context);
				break;

			// blend mode
			case 'Md  ':
				psd_assert(type == 'enum');
				
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// Gradient Type
				psd_assert(key == 'BlnM');
				pattern_overlay->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				pattern_overlay->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// pattern
			case 'Ptrn':
				psd_assert(type == 'Objc');
				psd_stream_get_object_pattern_info(&pattern_overlay->pattern_info, context);
				break;

			// scale
			case 'Scl ':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				pattern_overlay->scale = (psd_int)psd_stream_get_double(context);
				break;

			// link with layer
			case 'Algn':
				psd_assert(type == 'bool');
				pattern_overlay->link_with_layer = psd_stream_get_bool(context);
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
psd_status psd_layer_effects_blend_pattern_overlay(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_pattern_overlay * pattern_overlay = &data->pattern_overlay;
	psd_bitmap src_bmp, dst_bmp;
	psd_layer_mask_info layer_mask_info;
	psd_int i;

	data->left[psd_layer_effects_type_pattern_overlay] = 0;
	data->top[psd_layer_effects_type_pattern_overlay] = 0;
	data->right[psd_layer_effects_type_pattern_overlay] = layer->width;
	data->bottom[psd_layer_effects_type_pattern_overlay] = layer->height;
	data->blend_mode[psd_layer_effects_type_pattern_overlay] = pattern_overlay->blend_mode;
	data->opacity[psd_layer_effects_type_pattern_overlay] = pattern_overlay->opacity;
	data->width[psd_layer_effects_type_pattern_overlay] = layer->width;
	data->height[psd_layer_effects_type_pattern_overlay] = layer->height;

	if(data->image_data[psd_layer_effects_type_pattern_overlay] == NULL)
	{
		data->image_data[psd_layer_effects_type_pattern_overlay] = (psd_argb_color *)psd_malloc(layer->width * layer->height * 4);
		if(data->image_data[psd_layer_effects_type_pattern_overlay] == NULL)
			return psd_status_malloc_failed;
	}

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	dst_bmp.width = layer->width;
	dst_bmp.height = layer->height;
	dst_bmp.image_data = data->image_data[psd_layer_effects_type_pattern_overlay];

	for(i = 0; i < context->pattern_count; i ++)
	{
		if(strcmp(context->patterns[i].unique_id, pattern_overlay->pattern_info.identifier) == 0)
		{
			psd_pattern_fill(&dst_bmp, &context->patterns[i], pattern_overlay->scale, 
				pattern_overlay->horz_phase, pattern_overlay->vert_phase);
			break;
		}
	}
	
	psd_bitmap_mix_alpha_channel(&dst_bmp, &src_bmp);
	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		layer_mask_info.left -= layer->left;
		layer_mask_info.top -= layer->top;
		layer_mask_info.right -= layer->left;
		layer_mask_info.bottom -= layer->top;
		psd_bitmap_blend_mask(&dst_bmp, &layer_mask_info);
	}

	data->valid[psd_layer_effects_type_pattern_overlay] = psd_false;
	
	return psd_status_done;
}
#endif

