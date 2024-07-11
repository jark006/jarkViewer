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
 * $Id: effects.c, created by Patrick in 2006.05.23, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_bitmap.h"
#include "psd_math.h"
#include "psd_descriptor.h"


extern psd_status psd_get_layer_drop_shadow(psd_context * context, psd_layer_effects_drop_shadow * drop_shadow);
extern psd_status psd_get_layer_drop_shadow2(psd_context * context, psd_layer_effects_drop_shadow * drop_shadow);
extern psd_status psd_get_layer_inner_shadow(psd_context * context, psd_layer_effects_inner_shadow * inner_shadow);
extern psd_status psd_get_layer_inner_shadow2(psd_context * context, psd_layer_effects_inner_shadow * inner_shadow);
extern psd_status psd_get_layer_outer_glow(psd_context * context, psd_layer_effects_outer_glow * outer_glow);
extern psd_status psd_get_layer_outer_glow2(psd_context * context, psd_layer_effects_outer_glow * outer_glow);
extern psd_status psd_get_layer_inner_glow(psd_context * context, psd_layer_effects_inner_glow * inner_glow);
extern psd_status psd_get_layer_inner_glow2(psd_context * context, psd_layer_effects_inner_glow * inner_glow);
extern psd_status psd_get_layer_bevel_emboss(psd_context * context, psd_layer_effects_bevel_emboss * bevel_emboss);
extern psd_status psd_get_layer_bevel_emboss2(psd_context * context, psd_layer_effects_bevel_emboss * bevel_emboss);
extern psd_status psd_get_layer_color_overlay(psd_context * context, psd_layer_effects_color_overlay * color_overlay);
extern psd_status psd_get_layer_color_overlay2(psd_context * context, psd_layer_effects_color_overlay * color_overlay);
extern psd_status psd_get_layer_gradient_overlay2(psd_context * context, psd_layer_effects_gradient_overlay * gradient_overlay);
extern psd_status psd_get_layer_pattern_overlay2(psd_context * context, psd_layer_effects_pattern_overlay * pattern_overlay);
extern psd_status psd_get_layer_stroke2(psd_context * context, psd_layer_effects_stroke * stroke);
extern psd_status psd_get_layer_satin2(psd_context * context, psd_layer_effects_satin * satin);
extern psd_status psd_layer_effects_blend_drop_shadow(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_inner_shadow(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_outer_glow(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_inner_glow(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_satin(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_color_overlay(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_gradient_overlay(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_pattern_overlay(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_stroke(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_status psd_layer_effects_blend_bevel_emboss(psd_context * context, psd_layer_record * layer, psd_layer_effects * data);
extern psd_bool psd_layer_check_restricted(psd_context * context, psd_layer_record * layer);
extern void psd_layer_blend_normal(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern void psd_layer_blend(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern void psd_layer_blend_normal_restricted(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern void psd_layer_blend_restricted(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);


// Effects Layer (Photoshop 5.0)
psd_status psd_get_layer_effects(psd_context * context, psd_layer_record * layer)
{
	psd_layer_effects * data;
	psd_int i, size;
	psd_uint tag;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_effects;

	data = (psd_layer_effects *)psd_malloc(sizeof(psd_layer_effects));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_effects));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)(void *)data;
	layer->layer_info_count ++;

	// Version: 0
	if(psd_stream_get_short(context) != 0)
		return psd_status_effects_unsupport_version;

	// Effects count: may be 6 (for the 6 effects in Photoshop 5 and 6) or 7 (for
	// Photoshop 7.0)
	data->effects_count = psd_stream_get_short(context);

	for(i = 0; i < data->effects_count; i ++)
	{
		// Signature: '8BIM'
		tag = psd_stream_get_int(context);
		if(tag != '8BIM')
			return psd_status_effects_signature_error;

		// Effects signatures
		tag = psd_stream_get_int(context);
		switch(tag)
		{
			case 'cmnS':	// common state
				// Effects layer, common state info
				// Size of next three items: 7
				size = psd_stream_get_int(context);
				psd_assert(size == 7);
				// Version: 0
				if(psd_stream_get_int(context) != 0)
					return psd_status_common_state_unsupport_version;
				// Visible: always psd_true
				data->visible = psd_stream_get_bool(context);
				// Unused: always 0
				psd_stream_get_short(context);
				break;
				
			case 'dsdw':	// drop shadow
				psd_get_layer_drop_shadow(context, &data->drop_shadow);
				data->fill[psd_layer_effects_type_drop_shadow] = psd_true;
				data->valid[psd_layer_effects_type_drop_shadow] = psd_true;
				break;
			case 'isdw':	// inner shadow
				psd_get_layer_inner_shadow(context, &data->inner_shadow);
				data->fill[psd_layer_effects_type_inner_shadow] = psd_true;
				data->valid[psd_layer_effects_type_inner_shadow] = psd_true;
				break;
			case 'oglw':	// outer glow
				psd_get_layer_outer_glow(context, &data->outer_glow);
				data->fill[psd_layer_effects_type_outer_glow] = psd_true;
				data->valid[psd_layer_effects_type_outer_glow] = psd_true;
				break;
			case 'iglw':	// inner glow
				psd_get_layer_inner_glow(context, &data->inner_glow);
				data->fill[psd_layer_effects_type_inner_glow] = psd_true;
				data->valid[psd_layer_effects_type_inner_glow] = psd_true;
				break;
			case 'bevl':	// bevel
				psd_get_layer_bevel_emboss(context, &data->bevel_emboss);
				data->fill[psd_layer_effects_type_bevel_emboss] = psd_true;
				data->valid[psd_layer_effects_type_bevel_emboss] = psd_true;
				break;
			case 'sofi':	// solid fill (Photoshop 7.0)
				psd_get_layer_color_overlay(context, &data->color_overlay);
				data->fill[psd_layer_effects_type_color_overlay] = psd_true;
				data->valid[psd_layer_effects_type_color_overlay] = psd_true;
				break;
				
			default:
				psd_assert(0);
				return psd_status_unsupport_effects_type;
		}
	}

	return psd_status_done;
}

// Object-based effects layer info (Photoshop 6.0)
psd_status psd_get_layer_effects2(psd_context * context, psd_layer_record * layer)
{
	psd_layer_effects * data;
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_effects2;

	data = (psd_layer_effects *)psd_malloc(sizeof(psd_layer_effects));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_effects));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)(void *)data;
	layer->layer_info_count ++;
	
	// Object effects version: 0
	if(psd_stream_get_int(context) != 0)
		return psd_status_effects_unsupport_version;

	// Descriptor version ( = 16 for Photoshop 6.0).
	if(psd_stream_get_int(context) != 16)
		return psd_status_effects_unsupport_version;

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
	
	while(number_items--)
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
				// pattern overlay
				if(strcmp(keychar, "patternFill") == 0)
				{
					// Descriptor
					psd_assert(type == 'Objc');
					psd_get_layer_pattern_overlay2(context, &data->pattern_overlay);
					data->fill[psd_layer_effects_type_pattern_overlay] = psd_true;
					data->valid[psd_layer_effects_type_pattern_overlay] = psd_true;
					data->effects_count ++;
				}
				else
				{
					psd_assert(0);
					psd_stream_get_object_null(type, context);
				}
				break;

			// scale, do not used
			case 'Scl ':
				// Unit psd_float
				psd_assert(type == 'UntF');

				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				psd_stream_get_double(context);

				number_items --;
				length = psd_stream_get_int(context);
				if(length == 0)
					psd_stream_get_int(context);
				else
					psd_stream_get_null(context, length);
				// Type: OSType key
				type = psd_stream_get_int(context);
				psd_assert(type == 'bool');
				// Boolean value
				data->visible = psd_stream_get_bool(context);
				break;

			// drop shadow
			case 'DrSh':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_drop_shadow2(context, &data->drop_shadow);
				data->fill[psd_layer_effects_type_drop_shadow] = psd_true;
				data->valid[psd_layer_effects_type_drop_shadow] = psd_true;
				data->effects_count ++;
				break;

			// inner shadow
			case 'IrSh':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_inner_shadow2(context, &data->inner_shadow);
				data->fill[psd_layer_effects_type_inner_shadow] = psd_true;
				data->valid[psd_layer_effects_type_inner_shadow] = psd_true;
				data->effects_count ++;
				break;

			// outer glow
			case 'OrGl':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_outer_glow2(context, &data->outer_glow);
				data->fill[psd_layer_effects_type_outer_glow] = psd_true;
				data->valid[psd_layer_effects_type_outer_glow] = psd_true;
				data->effects_count ++;
				break;

			// inner glow
			case 'IrGl':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_inner_glow2(context, &data->inner_glow);
				data->fill[psd_layer_effects_type_inner_glow] = psd_true;
				data->valid[psd_layer_effects_type_inner_glow] = psd_true;
				data->effects_count ++;
				break;

			// bevel and emboss
			case 'ebbl':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_bevel_emboss2(context, &data->bevel_emboss);
				data->fill[psd_layer_effects_type_bevel_emboss] = psd_true;
				data->valid[psd_layer_effects_type_bevel_emboss] = psd_true;
				data->effects_count ++;
				break;

			// satin
			case 'ChFX':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_satin2(context, &data->satin);
				data->fill[psd_layer_effects_type_satin] = psd_true;
				data->valid[psd_layer_effects_type_satin] = psd_true;
				data->effects_count ++;
				break;

			// color overlay
			case 'SoFi':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_color_overlay2(context, &data->color_overlay);
				data->fill[psd_layer_effects_type_color_overlay] = psd_true;
				data->valid[psd_layer_effects_type_color_overlay] = psd_true;
				data->effects_count ++;
				break;

			// gradient overlay
			case 'GrFl':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_gradient_overlay2(context, &data->gradient_overlay);
				data->fill[psd_layer_effects_type_gradient_overlay] = psd_true;
				data->valid[psd_layer_effects_type_gradient_overlay] = psd_true;
				data->effects_count ++;
				break;

			// stroke
			case 'FrFX':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_get_layer_stroke2(context, &data->stroke);
				data->fill[psd_layer_effects_type_stroke] = psd_true;
				data->valid[psd_layer_effects_type_stroke] = psd_true;
				data->effects_count ++;
				break;

			default:
				psd_assert(0);
				psd_stream_get_object_null(type, context);
				break;
		}
	}

	return psd_status_done;
}

psd_status psd_layer_effects_update(psd_layer_record * layer, psd_layer_effects_type type)
{
	psd_layer_effects * data = NULL;
	psd_int i;
	
	if(layer == NULL)
		return psd_status_invalid_layer;
	if(type < 0 || type >= psd_layer_effects_type_count)
		return psd_status_invalid_layer_effects;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_effects)
		{
			data = (psd_layer_effects *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
		return psd_status_invalid_layer_effects;
	
	data->valid[type] = psd_true;

	return psd_status_done;
}


#ifdef PSD_SUPPORT_EFFECTS_BLEND

static void psd_layer_blend_effects_image(psd_context * context, psd_layer_record * layer, 
	psd_rect * dst_rect, psd_layer_effects * data, psd_int index)
{
	psd_rect layer_rect, mask_rect;
	psd_layer_record effects_layer;

	if(data->image_data[index] == NULL || data->opacity[index] == 0)
		return;
	
	memcpy(&effects_layer, layer, sizeof(psd_layer_record));
	effects_layer.left = data->left[index] + layer->left;
	effects_layer.top = data->top[index] + layer->top;
	effects_layer.right = data->right[index] + layer->left;
	effects_layer.bottom = data->bottom[index] + layer->top;
	effects_layer.width = data->width[index];
	effects_layer.height = data->height[index];
	effects_layer.fill_opacity = data->opacity[index];
	effects_layer.blend_mode = data->blend_mode[index];
	effects_layer.image_data = data->image_data[index];
	effects_layer.layer_mask_info.disabled = psd_true;

	psd_make_rect(&layer_rect, effects_layer.left, effects_layer.top, effects_layer.right, effects_layer.bottom);
	if(psd_incept_rect(dst_rect, &layer_rect, &layer_rect) == psd_true)
	{
		if(effects_layer.layer_mask_info.disabled == psd_false && effects_layer.layer_mask_info.mask_data != NULL && effects_layer.layer_mask_info.default_color == 0)
		{
			psd_make_rect(&mask_rect, effects_layer.layer_mask_info.left, effects_layer.layer_mask_info.top,
				effects_layer.layer_mask_info.right, effects_layer.layer_mask_info.bottom);
			if(psd_incept_rect(&mask_rect, &layer_rect, &layer_rect) == psd_false)
				return;
		}
		
		if(psd_layer_check_restricted(context, &effects_layer) == psd_true)
		{
			if(effects_layer.blend_mode != psd_blend_mode_normal ||
				(effects_layer.group_layer != NULL && effects_layer.group_layer->divider_blend_mode != psd_blend_mode_pass_through))
				psd_layer_blend_restricted(context, &effects_layer, &layer_rect);
			else
				psd_layer_blend_normal_restricted(context, &effects_layer, &layer_rect);
		}
		else
		{
			if(effects_layer.blend_mode != psd_blend_mode_normal ||
				(effects_layer.group_layer != NULL && effects_layer.group_layer->divider_blend_mode != psd_blend_mode_pass_through))
				psd_layer_blend(context, &effects_layer, &layer_rect);
			else
				psd_layer_blend_normal(context, &effects_layer, &layer_rect);
		}
	}
}

psd_bool psd_layer_effects_blend_background(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_layer_effects * data = NULL;
	psd_int i;

	if(layer->image_data == NULL || layer->width <= 0 || layer->height <= 0)
		return psd_false;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_effects2)
		{
			data = (psd_layer_effects *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
	{
		for(i = 0; i < layer->layer_info_count; i ++)
		{
			if(layer->layer_info_type[i] == psd_layer_info_type_effects)
			{
				data = (psd_layer_effects *)layer->layer_info_data[i];
				break;
			}
		}
	}
	if(data == NULL)
		return psd_false;

	if(data->effects_count == 0 || data->visible == psd_false)
		return psd_false;

	for(i = psd_layer_effects_type_count - 1; i >= 0; i --)
	{
		if(data->fill[i] == psd_false)
			continue;

		switch(i)
		{
			case psd_layer_effects_type_drop_shadow:
				if(data->drop_shadow.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_drop_shadow(context, layer, data);
				break;
			case psd_layer_effects_type_bevel_emboss:
				if(data->bevel_emboss.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					NULL;
				break;
			default:
				continue;
		}

		if(i == psd_layer_effects_type_bevel_emboss)
		{
			psd_layer_blend_effects_image(context, layer, dst_rect, data, 
				psd_layer_effects_bevel_emboss_outer_shadow);
			psd_layer_blend_effects_image(context, layer, dst_rect, data, 
				psd_layer_effects_bevel_emboss_inner_shadow);
		}
		else
		{
			psd_layer_blend_effects_image(context, layer, dst_rect, data, i);
		}
	}

	return psd_true;
}

psd_bool psd_layer_effects_blend_foreground(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_layer_effects * data = NULL;
	psd_int i;

	if(layer->image_data == NULL || layer->width <= 0 || layer->height <= 0)
		return psd_false;
	
	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_effects2)
		{
			data = (psd_layer_effects *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
	{
		for(i = 0; i < layer->layer_info_count; i ++)
		{
			if(layer->layer_info_type[i] == psd_layer_info_type_effects)
			{
				data = (psd_layer_effects *)layer->layer_info_data[i];
				break;
			}
		}
	}
	if(data == NULL)
		return psd_false;

	if(data->effects_count == 0 || data->visible == psd_false)
		return psd_false;

	for(i = psd_layer_effects_type_count - 1; i >= 0; i --)
	{
		if(data->fill[i] == psd_false)
			continue;

		switch(i)
		{
			case psd_layer_effects_type_inner_shadow:
				if(data->inner_shadow.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_inner_shadow(context, layer, data);
				break;
			case psd_layer_effects_type_outer_glow:
				if(data->outer_glow.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_outer_glow(context, layer, data);
				break;
			case psd_layer_effects_type_inner_glow:
				if(data->inner_glow.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_inner_glow(context, layer, data);
				break;
			case psd_layer_effects_type_bevel_emboss:
				if(data->bevel_emboss.effect_enable == psd_false)
					continue;
				// do nothing
				break;
			case psd_layer_effects_type_satin:
				if(data->satin.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_satin(context, layer, data);
				break;
			case psd_layer_effects_type_color_overlay:
				if(data->color_overlay.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_color_overlay(context, layer, data);
				break;
			case psd_layer_effects_type_gradient_overlay:
				if(data->gradient_overlay.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_gradient_overlay(context, layer, data);
				break;
			case psd_layer_effects_type_pattern_overlay:
				if(data->pattern_overlay.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_pattern_overlay(context, layer, data);
				break;
			case psd_layer_effects_type_stroke:
				if(data->stroke.effect_enable == psd_false)
					continue;
				if(data->valid[i] == psd_true)
					psd_layer_effects_blend_stroke(context, layer, data);
				break;
			default:
				continue;
		}
		
		if(data->image_data[i] == NULL)
			continue;

		if(i == psd_layer_effects_type_bevel_emboss)
		{
			psd_layer_blend_effects_image(context, layer, dst_rect, data, 
				psd_layer_effects_bevel_emboss_outer_light);
			psd_layer_blend_effects_image(context, layer, dst_rect, data, 
				psd_layer_effects_bevel_emboss_inner_light);
			psd_layer_blend_effects_image(context, layer, dst_rect, data, 
				psd_layer_effects_bevel_emboss_texture);
		}
		else
		{
			psd_layer_blend_effects_image(context, layer, dst_rect, data, i);
		}
	}

	return psd_true;
}

void psd_effects_add_noise(psd_bitmap * bitmap, psd_int noise, psd_int left, psd_int top, psd_context * context)
{
	psd_int i, j, x, y, width, height;
	psd_int src_alpha, dst_alpha, next_alpha;
	psd_argb_color * src_data, * dst_data;
	psd_uchar * rand_data;
	psd_bitmap noise_bmp;

	width = bitmap->width;
	height = bitmap->height;
	psd_get_bitmap(&noise_bmp, width, height, context);
	psd_fill_bitmap(&noise_bmp, psd_color_clear);

	for(i = PSD_MAX(-top, 0); i < height; i ++)
	{
		if(i + top >= context->height)
			break;
		
		src_data = bitmap->image_data + i * bitmap->width + PSD_MAX(-left, 0);
		rand_data = context->rand_data + (i + top) * context->width + left;
		for(j = PSD_MAX(-left, 0); j < width; j ++, src_data ++, rand_data ++)
		{
			if(j + left >= context->width)
				break;
			
			x = j + (*rand_data >> 4) - 8;
			y = i + (*rand_data & 0x0F) - 8;
			x = (x + j) >> 1;
			y = (y + i) >> 1;
			if(x < 0 || x >= width || y < 0 || y >= height)
				continue;

			src_alpha = PSD_GET_ALPHA_COMPONENT(*src_data);
			dst_data = noise_bmp.image_data + y * width + x;
			dst_alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
			if(dst_alpha + src_alpha <= 255)
			{
				dst_alpha = dst_alpha + src_alpha;
				*dst_data = (*src_data & 0x00FFFFFF) | (dst_alpha << 24);
			}
			else
			{
				*dst_data = (*src_data & 0x00FFFFFF) | 0xFF000000;
				if(x + 1 < width)
				{
					next_alpha = dst_alpha + src_alpha - 255;
					*(dst_data + 1) = (*src_data & 0x00FFFFFF) | (next_alpha << 24);
				}
			}
		}
	}

	noise = noise * 256 / 100;
	if(noise == 256)
	{
		psd_copy_bitmap(bitmap, &noise_bmp);
	}
	else
	{
		src_data = noise_bmp.image_data;
		dst_data = bitmap->image_data;
		for(i = width * height; i --; src_data ++, dst_data ++)
		{
			src_alpha = PSD_GET_ALPHA_COMPONENT(*src_data);
			dst_alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
			dst_alpha = ((dst_alpha << 8) + (src_alpha - dst_alpha) * noise) >> 8;
			*dst_data = (*dst_data & 0x00FFFFFF) | (dst_alpha << 24);
		}
	}
}

void psd_effects_apply_gradient(psd_bitmap * bitmap, psd_argb_color * gradient_table, psd_bool edge_hidden, 
	psd_int jitter, psd_int left, psd_int top, psd_context * context)
{
	psd_int i, j, alpha, gradient_index;
	psd_argb_color * dst_data;
	psd_uchar * rand_data;
	
	if(jitter > 0)
	{
		jitter = jitter * 256 / 100;
		for(i = PSD_MAX(-top, 0); i < bitmap->height; i ++)
		{
			if(i + top >= context->height)
				break;
			
			dst_data = bitmap->image_data + i * bitmap->width + PSD_MAX(-left, 0);
			rand_data = context->rand_data + (i + top) * context->width + left;
			for(j = PSD_MAX(-left, 0); j < bitmap->width; j ++, dst_data ++, rand_data ++)
			{
				if(j + left >= context->width)
					break;
				alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
				gradient_index = 255 - alpha;
				gradient_index += jitter * *rand_data >> 8;
				gradient_index &= 0xFF;
				
				if(edge_hidden == psd_true && alpha < 24)
				{
					if(PSD_GET_ALPHA_COMPONENT(gradient_table[gradient_index]) != 255)
					{
						*dst_data = gradient_table[gradient_index];
					}
					else
					{
						*dst_data = (gradient_table[gradient_index] & 0x00FFFFFF) | 
							(alpha * 10 * PSD_GET_ALPHA_COMPONENT(gradient_table[gradient_index]) >> 8 << 24);
					}
				}
				else
				{
					*dst_data = gradient_table[gradient_index];
				}
			}
		}
	}
	else
	{
		dst_data = bitmap->image_data;
		for(i = bitmap->width * bitmap->height; i --; )
		{
			alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
			if(edge_hidden == psd_true && alpha < 24)
			{
				if(PSD_GET_ALPHA_COMPONENT(gradient_table[255 - alpha]) != 255)
				{
					*dst_data = gradient_table[255 - alpha];
				}
				else
				{
					*dst_data = (gradient_table[255 - alpha] & 0x00FFFFFF) | 
						(alpha * 10 * PSD_GET_ALPHA_COMPONENT(gradient_table[255 - alpha]) >> 8 << 24);
				}
			}
			else
			{
				*dst_data = gradient_table[255 - alpha];
			}
			dst_data ++;
		}
	}
}

#endif // ifdef PSD_SUPPORT_EFFECTS_BLEND


static void psd_gradient_color_free(psd_gradient_color * gradient_color)
{
	psd_freeif(gradient_color->name);
	psd_freeif(gradient_color->color_stop);
	psd_freeif(gradient_color->transparency_stop);
}

static void psd_pattern_info_free(psd_pattern_info * pattern_info)
{
	psd_freeif(pattern_info->name);
}

void psd_layer_effects_free(psd_uint layer_info)
{
	psd_layer_effects * data;
	psd_int i;
	
	data = (psd_layer_effects *)layer_info;
	if(data == NULL)
		return;

	for(i = 0; i < psd_layer_effects_image_count; i ++)
	{
		psd_freeif(data->image_data[i]);
		data->image_data[i] = NULL;
	}

	psd_pattern_info_free(&data->bevel_emboss.texture_pattern_info);
	psd_gradient_color_free(&data->gradient_overlay.gradient_color);
	psd_pattern_info_free(&data->pattern_overlay.pattern_info);
	psd_gradient_color_free(&data->stroke.gradient_color);
	psd_pattern_info_free(&data->stroke.pattern_info);

	psd_free(data);
}

