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
 * $Id: bevel_emboss.c, created by Patrick in 2006.05.23, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_descriptor.h"
#include "psd_bitmap.h"
#include "psd_color.h"
#include "psd_math.h"


extern void psd_bitmap_gaussian_blur_alpha_channel(psd_bitmap * bitmap, psd_double radius);


static void psd_set_layer_bevel_emboss_default(psd_layer_effects_bevel_emboss * bevel_emboss)
{
	psd_int i;
	
	bevel_emboss->style = psd_bevel_inner_bevel;
	bevel_emboss->technique = psd_technique_slope_limit;
	bevel_emboss->direction = psd_direction_up;
	bevel_emboss->depth = 100;
	bevel_emboss->size = 5;
	bevel_emboss->soften = 0;

	bevel_emboss->angle = 120;
	bevel_emboss->use_global_light = psd_true;
	bevel_emboss->altitude = 30;
	
	for(i = 0; i < 256; i ++)
		bevel_emboss->gloss_contour_lookup_table[i] = i;
	bevel_emboss->gloss_anti_aliased = psd_false;
	bevel_emboss->highlight_blend_mode = psd_blend_mode_screen;
	bevel_emboss->highlight_color = bevel_emboss->real_highlight_color = psd_color_white;
	bevel_emboss->highlight_opacity = 191;
	bevel_emboss->shadow_blend_mode = psd_blend_mode_screen;
	bevel_emboss->shadow_color = bevel_emboss->real_shadow_color = psd_color_black;
	bevel_emboss->shadow_opacity = 191;

	bevel_emboss->contour_enable = psd_false;
	for(i = 0; i < 256; i ++)
		bevel_emboss->contour_lookup_table[i] = i;
	bevel_emboss->contour_anti_aliased = psd_false;
	bevel_emboss->contour_range = 50;

	bevel_emboss->texture_enable = psd_false;
	bevel_emboss->texture_scale = 100;
	bevel_emboss->texture_depth = 100;
	bevel_emboss->texture_invert = psd_false;
	bevel_emboss->texture_link = psd_true;
}

psd_status psd_get_layer_bevel_emboss(psd_context * context, psd_layer_effects_bevel_emboss * bevel_emboss)
{
	psd_int size, version;
	psd_uint tag;

	psd_set_layer_bevel_emboss_default(bevel_emboss);

	// Size of the remaining items (58 for version 0, 78 for version 20
	size = psd_stream_get_int(context);
	
	// Version: 0 for Photoshop 5.0; 2 for 5.5
	version = psd_stream_get_int(context);
	if(version != 0 && version != 2)
		return psd_status_bevel_unsupport_version;

	// Angle in degrees
	bevel_emboss->angle = psd_stream_get_int(context);

	// Strength. Depth in pixels
	bevel_emboss->size = psd_stream_get_int(context);

	// Blur value in pixels.
	bevel_emboss->soften = psd_stream_get_int(context);

	// Highlight blend mode: 4 bytes for signature and 4 bytes for the key
	// Blend mode signature: '8BIM'
	tag = psd_stream_get_int(context);
	if(tag != '8BIM')
		return psd_status_blend_mode_signature_error;
	
	// Blend mode key
	bevel_emboss->highlight_blend_mode = psd_stream_get_blend_mode(context);

	// Shadow blend mode: 4 bytes for signature and 4 bytes for the key
	// Blend mode signature: '8BIM'
	tag = psd_stream_get_int(context);
	if(tag != '8BIM')
		return psd_status_blend_mode_signature_error;
	
	// Blend mode key
	bevel_emboss->shadow_blend_mode = psd_stream_get_blend_mode(context);

	// Highlight color
	bevel_emboss->highlight_color = psd_stream_get_space_color(context);

	// Shadow color
	bevel_emboss->shadow_color = psd_stream_get_space_color(context);

	// Bevel style
	bevel_emboss->style = psd_stream_get_char(context);

	// Hightlight opacity as a percent
	bevel_emboss->highlight_opacity = psd_stream_get_char(context);

	// Shadow opacity as a percent
	bevel_emboss->shadow_opacity = psd_stream_get_char(context);

	// Effect enabled
	bevel_emboss->effect_enable = psd_stream_get_bool(context);

	// Use this angle in all of the layer effects
	bevel_emboss->use_global_light = psd_stream_get_bool(context);

	// Up or down
	bevel_emboss->direction = psd_stream_get_char(context);

	// The following are present in version 2 only
	if(version == 2)
	{
		// Real highlight color
		bevel_emboss->real_highlight_color = psd_stream_get_space_color(context);

		// Real shadow color
		bevel_emboss->real_shadow_color = psd_stream_get_space_color(context);
	}

	return psd_status_done;
}

psd_status psd_get_layer_bevel_emboss2(psd_context * context, psd_layer_effects_bevel_emboss * bevel_emboss)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_bevel_emboss_default(bevel_emboss);

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
				// gloss anti-aliased
				if(strcmp(keychar, "antialiasGloss") == 0)
				{
					psd_assert(type == 'bool');
					bevel_emboss->gloss_anti_aliased = psd_stream_get_bool(context);
				}
				// coutour enable
				else if(strcmp(keychar, "useShape") == 0)
				{
					psd_assert(type == 'bool');
					bevel_emboss->contour_enable = psd_stream_get_bool(context);
				}
				// texture enable
				else if(strcmp(keychar, "useTexture") == 0)
				{
					psd_assert(type == 'bool');
					bevel_emboss->texture_enable = psd_stream_get_bool(context);
				}
				// texture depth
				else if(strcmp(keychar, "textureDepth") == 0)
				{
					psd_assert(type == 'UntF');
					// percent
					key = psd_stream_get_int(context);
					psd_assert(key == '#Prc');
					// Actual value (double)
					bevel_emboss->texture_depth = (psd_int)psd_stream_get_double(context);
				}
				else if(strcmp(keychar, "phase") == 0)
				{
					psd_assert(type == 'Objc');
					psd_stream_get_object_point(&bevel_emboss->texture_horz_phase, 
						&bevel_emboss->texture_vert_phase, context);
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
				bevel_emboss->effect_enable = psd_stream_get_bool(context);
				break;

			// highlight blend mode
			case 'hglM':
				psd_assert(type == 'enum');
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// blend mode
				psd_assert(key == 'BlnM');
				bevel_emboss->highlight_blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// highlight color
			case 'hglC':
				// Descriptor
				psd_assert(type == 'Objc');
				bevel_emboss->highlight_color = bevel_emboss->real_highlight_color = 
					psd_stream_get_object_color(context);
				break;

			// highlight opacity
			case 'hglO':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				bevel_emboss->highlight_opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// shadow blend mode
			case 'sdwM':
				psd_assert(type == 'enum');
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// blend mode
				psd_assert(key == 'BlnM');
				bevel_emboss->shadow_blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// shadow color
			case 'sdwC':
				// Descriptor
				psd_assert(type == 'Objc');
				bevel_emboss->shadow_color = bevel_emboss->real_shadow_color = 
					psd_stream_get_object_color(context);
				break;

			// shadow opacity
			case 'sdwO':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				bevel_emboss->shadow_opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// technique
			case 'bvlT':
				psd_assert(type == 'enum');
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// bevel technique
				psd_assert(key == 'bvlT');
				bevel_emboss->technique = psd_stream_get_object_technique(context);
				break;
				
			// style
			case 'bvlS':
				psd_assert(type == 'enum');
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// bevel style
				psd_assert(key == 'BESl');

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
					case 'OtrB':
						bevel_emboss->style = psd_bevel_outer_bevel;
						break;
					case 'InrB':
						bevel_emboss->style = psd_bevel_inner_bevel;
						break;
					case 'Embs':
						bevel_emboss->style = psd_bevel_emboss;
						break;
					case 'PlEb':
						bevel_emboss->style = psd_bevel_pillow_emboss;
						break;
					case 0:
						if(strcmp(keychar, "strokeEmboss") == 0)
							bevel_emboss->style = psd_bevel_stroke_emboss;
						else
							psd_assert(0);
						break;
					default:
						psd_assert(0);
						break;
				}
				break;
				
			// use global light
			case 'uglg':
				psd_assert(type == 'bool');
				bevel_emboss->use_global_light = psd_stream_get_bool(context);
				break;

			// angle
			case 'lagl':
				psd_assert(type == 'UntF');
				// angle: base degrees
				key = psd_stream_get_int(context);
				psd_assert(key == '#Ang');
				bevel_emboss->angle = (psd_int)psd_stream_get_double(context);
				break;

			// altitude
			case 'Lald':
				psd_assert(type == 'UntF');
				// angle: base degrees
				key = psd_stream_get_int(context);
				psd_assert(key == '#Ang');
				bevel_emboss->altitude = (psd_int)psd_stream_get_double(context);
				break;

			// depth
			case 'srgR':
				psd_assert(type == 'UntF');
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				bevel_emboss->depth = (psd_int)psd_stream_get_double(context);
				break;

			// size
			case 'blur':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				bevel_emboss->size = (psd_int)psd_stream_get_double(context);
				break;

			// direction
			case 'bvlD':
				psd_assert(type == 'enum');
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// emboss stamp style
				psd_assert(key == 'BESs');

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
					case 'In  ':
						bevel_emboss->direction = psd_direction_up;
						break;
					case 'Out ':
						bevel_emboss->direction = psd_direction_down;
						break;
					default:
						psd_assert(0);
						break;
				}
				break;

			// gloss contour
			case 'TrnS':
				psd_assert(type == 'Objc');
				psd_stream_get_object_contour(bevel_emboss->gloss_contour_lookup_table, context);
				break;

			// soften
			case 'Sftn':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				bevel_emboss->soften = (psd_int)psd_stream_get_double(context);
				break;

			// contour
			case 'MpgS':
				psd_assert(type == 'Objc');
				psd_stream_get_object_contour(bevel_emboss->contour_lookup_table, context);
				break;

			// contour anti-aliased
			case 'AntA':
				psd_assert(type == 'bool');
				bevel_emboss->contour_anti_aliased = psd_stream_get_bool(context);
				break;

			// contour range
			case 'Inpr':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				bevel_emboss->contour_range = (psd_int)psd_stream_get_double(context);
				break;

			// invert
			case 'InvT':
				psd_assert(type == 'bool');
				bevel_emboss->texture_invert = psd_stream_get_bool(context);
				break;

			// link with layer
			case 'Algn':
				psd_assert(type == 'bool');
				bevel_emboss->texture_link = psd_stream_get_bool(context);
				break;

			// scale
			case 'Scl ':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				bevel_emboss->texture_scale = (psd_int)psd_stream_get_double(context);
				break;

			// texture pattern
			case 'Ptrn':
				psd_assert(type == 'Objc');
				psd_stream_get_object_pattern_info(&bevel_emboss->texture_pattern_info, context);
				break;

			default:
				psd_assert(0);
				psd_stream_get_object_null(type, context);
				break;
		}
	}

	return psd_status_done;
}

psd_status psd_layer_effects_blend_bevel_emboss(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_bevel_emboss * bevel_emboss = &data->bevel_emboss;
	psd_int width, height, index;
	psd_int angle, altitude;
	psd_int distance_x, distance_y;
	psd_bitmap src_bmp, dst_bmp, shadow_bmp;
	psd_layer_mask_info layer_mask_info;

	if(bevel_emboss->use_global_light == psd_true)
	{
		angle = context->global_angle;
		altitude = context->global_altitude;
	}
	else
	{
		angle = bevel_emboss->angle;
		altitude = bevel_emboss->altitude;
	}
	distance_x = -(psd_int)(bevel_emboss->size * cos(PSD_PI * angle / 180) * 
		cos(PSD_PI * altitude / 180) + 0.5);
	distance_y = (psd_int)(bevel_emboss->size * sin(PSD_PI * angle / 180) *
		cos(PSD_PI * altitude / 180) + 0.5);

	for(index = psd_layer_effects_bevel_emboss_outer_shadow; 
		index <= psd_layer_effects_bevel_emboss_texture; index ++)
	{
		data->left[index] = -bevel_emboss->size * 2;
		data->top[index] = -bevel_emboss->size * 2;
		width = layer->width + bevel_emboss->size * 4;
		height = layer->height + bevel_emboss->size * 4;
		data->right[index] = data->left[index] + width;
		data->bottom[index] = data->top[index] + height;
	}
	
	data->blend_mode[psd_layer_effects_bevel_emboss_outer_shadow] = bevel_emboss->shadow_blend_mode;
	data->opacity[psd_layer_effects_bevel_emboss_outer_shadow] = bevel_emboss->shadow_opacity;
	data->blend_mode[psd_layer_effects_bevel_emboss_outer_light] = bevel_emboss->highlight_blend_mode;
	data->opacity[psd_layer_effects_bevel_emboss_outer_light] = bevel_emboss->highlight_opacity;
	data->blend_mode[psd_layer_effects_bevel_emboss_inner_shadow] = bevel_emboss->shadow_blend_mode;
	data->opacity[psd_layer_effects_bevel_emboss_inner_shadow] = bevel_emboss->shadow_opacity;
	data->blend_mode[psd_layer_effects_bevel_emboss_inner_light] = bevel_emboss->highlight_blend_mode;
	data->opacity[psd_layer_effects_bevel_emboss_inner_light] = bevel_emboss->highlight_opacity;
	data->blend_mode[psd_layer_effects_bevel_emboss_texture] = psd_blend_mode_overlay;
	data->opacity[psd_layer_effects_bevel_emboss_texture] = bevel_emboss->texture_depth * 255 / 100;

	for(index = psd_layer_effects_bevel_emboss_outer_shadow; 
		index <= psd_layer_effects_bevel_emboss_texture; index ++)
	{
		if(index == psd_layer_effects_bevel_emboss_texture)
		{
			if(bevel_emboss->texture_enable == psd_false)
			{
				data->opacity[index] = 0;
				continue;
			}
		}
		else
		{
			switch(bevel_emboss->style)
			{
				case psd_bevel_outer_bevel:
					if(index != psd_layer_effects_bevel_emboss_outer_shadow &&
						index != psd_layer_effects_bevel_emboss_outer_light)
					{
						data->opacity[index] = 0;
						continue;
					}
					break;
				case psd_bevel_inner_bevel:
					if(index != psd_layer_effects_bevel_emboss_inner_shadow &&
						index != psd_layer_effects_bevel_emboss_inner_light)
					{
						data->opacity[index] = 0;
						continue;
					}
					break;
				case psd_bevel_emboss:
					break;
				case psd_bevel_pillow_emboss:
					break;
				case psd_bevel_stroke_emboss:
					break;
				default:
					psd_assert(0);
					break;
			}
		}
		
		if(data->image_data[index] != NULL)
		{
			if(data->width[index] != width ||
				data->height[index] != height)
			{
				psd_free(data->image_data[index]);
				data->image_data[index] = (psd_argb_color *)psd_malloc(width * height * 4);
				if(data->image_data[index] == NULL)
					return psd_status_malloc_failed;
			}
		}
		else
		{
			data->image_data[index] = (psd_argb_color *)psd_malloc(width * height * 4);
			if(data->image_data[index] == NULL)
				return psd_status_malloc_failed;
		}
		data->width[index] = width;
		data->height[index] = height;
	}

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	psd_create_bitmap(&shadow_bmp, width, height);

	psd_inflate_bitmap(&shadow_bmp, &src_bmp, bevel_emboss->size * 2, bevel_emboss->size * 2);
	psd_fill_bitmap_without_alpha_channel(&shadow_bmp, bevel_emboss->shadow_color);
	psd_bitmap_gaussian_blur_alpha_channel(&shadow_bmp, bevel_emboss->size);

	dst_bmp.width = width;
	dst_bmp.height = height;
	switch(bevel_emboss->style)
	{
		case psd_bevel_outer_bevel:
			dst_bmp.image_data = data->image_data[psd_layer_effects_bevel_emboss_outer_shadow];
			psd_copy_bitmap(&dst_bmp, &shadow_bmp);
			psd_offset_bitmap(&dst_bmp, distance_x, distance_y, psd_color_clear);
			dst_bmp.image_data = data->image_data[psd_layer_effects_bevel_emboss_outer_light];
			psd_copy_bitmap(&dst_bmp, &shadow_bmp);
			psd_fill_bitmap_without_alpha_channel(&dst_bmp, bevel_emboss->highlight_color);
			psd_offset_bitmap(&dst_bmp, -distance_x, -distance_y, psd_color_clear);

			psd_inflate_bitmap(&shadow_bmp, &src_bmp, bevel_emboss->size * 2, bevel_emboss->size * 2);
			dst_bmp.image_data = data->image_data[psd_layer_effects_bevel_emboss_outer_shadow];
			psd_bitmap_knock_out(&dst_bmp, &shadow_bmp);
			dst_bmp.image_data = data->image_data[psd_layer_effects_bevel_emboss_outer_light];
			psd_bitmap_knock_out(&dst_bmp, &shadow_bmp);
			break;
			
		case psd_bevel_inner_bevel:
			break;
			
		case psd_bevel_emboss:
			break;
			
		case psd_bevel_pillow_emboss:
			break;
			
		case psd_bevel_stroke_emboss:
			break;
	}

	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		for(index = psd_layer_effects_bevel_emboss_outer_shadow; 
			index <= psd_layer_effects_bevel_emboss_texture; index ++)
		{
			if(data->image_data[index] != NULL && data->opacity[index] > 0)
			{
				layer_mask_info.left -= data->left[index];
				layer_mask_info.top -= data->top[index];
				layer_mask_info.right -= data->left[index];
				layer_mask_info.bottom -= data->top[index];
				psd_bitmap_blend_mask(&dst_bmp, &layer_mask_info);
			}
		}
	}

	psd_bitmap_contour_alpha_channel(&dst_bmp, bevel_emboss->contour_lookup_table, 
		psd_true, psd_true);

	psd_free_bitmap(&shadow_bmp);
	
	data->valid[psd_layer_effects_type_bevel_emboss] = psd_false;
	
	return psd_status_done;
}

