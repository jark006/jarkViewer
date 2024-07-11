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
 * $Id: stroke.c, created by Patrick in 2006.06.18, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_bitmap.h"
#include "psd_gradient.h"
#include "psd_math.h"
#include "psd_descriptor.h"


extern psd_bool psd_draw_stroke(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, psd_int stroke_size);
extern psd_status psd_pattern_fill(psd_bitmap * dst_bmp, psd_pattern * pattern, psd_int scale, psd_int offset_x, psd_int offset_y);


static void psd_set_layer_stroke_default(psd_layer_effects_stroke * stroke)
{
	stroke->size = 3;
	stroke->position = psd_stroke_outside;
	stroke->blend_mode = psd_blend_mode_normal;
	stroke->opacity = 255;

	stroke->fill_type = psd_fill_solid_color;
	
	stroke->fill_color = psd_color_red;

	stroke->gradient_reverse = psd_false;
	stroke->gradient_style = 0;
	stroke->gradient_align = psd_true;
	stroke->gradient_angle = 90;
	stroke->gradient_scale = 100;

	stroke->pattern_scale = 100;
	stroke->pattern_link = psd_true;
}

psd_status psd_get_layer_stroke2(psd_context * context, psd_layer_effects_stroke * stroke)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_stroke_default(stroke);

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
					psd_stream_get_object_point(&stroke->pattern_horz_phase, 
						&stroke->pattern_vert_phase, context);
					stroke->pattern_horz_phase = 0;
					stroke->pattern_vert_phase = 0;
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
				stroke->effect_enable = psd_stream_get_bool(context);
				break;

			// position
			case 'Styl':
				psd_assert(type == 'enum');
				
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// fill style
				psd_assert(key == 'FStl');
				
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
					case 'OutF':
						stroke->position = psd_stroke_outside;
						break;
					case 'InsF':
						stroke->position = psd_stroke_inside;
						break;
					case 'CtrF':
						stroke->position = psd_stroke_center;
						break;
					default:
						psd_assert(0);
						break;
				}
				break;

			// fill type
			case 'PntT':
				psd_assert(type == 'enum');
				
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// fill style
				psd_assert(key == 'FrFl');
				
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
					case 'SClr':
						stroke->fill_type = psd_fill_solid_color;
						break;
					case 'GrFl':
						stroke->fill_type = psd_fill_gradient;
						break;
					case 'Ptrn':
						stroke->fill_type = psd_fill_pattern;
						break;
					default:
						psd_assert(0);
						break;
				}
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
				stroke->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				stroke->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;

			// size
			case 'Sz  ':
				psd_assert(type == 'UntF');
				// pixels: tagged unit value
				key = psd_stream_get_int(context);
				psd_assert(key == '#Pxl');
				stroke->size = (psd_int)psd_stream_get_double(context);
				break;

			// color
			case 'Clr ':
				// Descriptor
				psd_assert(type == 'Objc');
				stroke->fill_color = psd_stream_get_object_color(context);
				break;

			// gradient color
			case 'Grad':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_stream_get_object_gradient_color(&stroke->gradient_color, context);
				break;

			case 'Angl':	// angle
				// Unit psd_float
				psd_assert(type == 'UntF');
				
				// '#Ang' = angle: base degrees
				key = psd_stream_get_int(context);
				psd_assert(key == '#Ang');

				// Actual value (double)
				stroke->gradient_angle = (psd_int)psd_stream_get_double(context);
				break;

			case 'Type':	// gradient style
				// Enumerated
				psd_assert(type == 'enum');

				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// Gradient Type
				psd_assert(key == 'GrdT');
				stroke->gradient_style = psd_stream_get_object_gradient_style(context);
				break;
				
			case 'Rvrs':	// reverse
				// boolean
				psd_assert(type == 'bool');
				stroke->gradient_reverse = psd_stream_get_bool(context);
				break;

			case 'Scl ':	// scale
				// Unit psd_float
				psd_assert(type == 'UntF');

				// '#Prc' = percent:
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
								
				// Actual value (double)
				if(stroke->fill_type == psd_fill_gradient)
					stroke->gradient_scale = (psd_int)psd_stream_get_double(context);
				else if(stroke->fill_type == psd_fill_pattern)
					stroke->pattern_scale = (psd_int)psd_stream_get_double(context);
				break;

			case 'Algn':	// align with layer
				// boolean
				psd_assert(type == 'bool');
				stroke->gradient_align = psd_stream_get_bool(context);
				break;

			// offset, not documented
			case 'Ofst':
				psd_assert(type == 'Objc');
				psd_stream_get_object_point(&stroke->gradient_horz_offset, 
					&stroke->gradient_vert_offset, context);
				break;

			// pattern
			case 'Ptrn':
				psd_assert(type == 'Objc');
				psd_stream_get_object_pattern_info(&stroke->pattern_info, context);
				break;

			// link with layer
			case 'Lnkd':
				psd_assert(type == 'bool');
				stroke->pattern_link = psd_stream_get_bool(context);
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
psd_status psd_layer_effects_blend_stroke(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_stroke * stroke = &data->stroke;
	psd_int i, width, height, center_x, center_y, radius_x, radius_y, radius_corner;
	psd_int corner_angle, angle;
	psd_int sign_x = 1, sign_y = 1;
	psd_bitmap layer_bmp, src_bmp, dst_bmp;
	psd_layer_mask_info layer_mask_info;

	width = layer->width + stroke->size * 2;
	height = layer->height + stroke->size * 2;
	data->left[psd_layer_effects_type_stroke] = -stroke->size;
	data->top[psd_layer_effects_type_stroke] = -stroke->size;
	data->right[psd_layer_effects_type_stroke] = layer->width + stroke->size;
	data->bottom[psd_layer_effects_type_stroke] = layer->height + stroke->size;
	data->blend_mode[psd_layer_effects_type_stroke] = stroke->blend_mode;
	data->opacity[psd_layer_effects_type_stroke] = stroke->opacity;

	if(data->image_data[psd_layer_effects_type_stroke] != NULL)
	{
		if(data->width[psd_layer_effects_type_stroke] != width ||
			data->height[psd_layer_effects_type_stroke] != height)
		{
			psd_free(data->image_data[psd_layer_effects_type_stroke]);
			data->image_data[psd_layer_effects_type_stroke] = (psd_argb_color *)psd_malloc(width * height * 4);
			if(data->image_data[psd_layer_effects_type_stroke] == NULL)
				return psd_status_malloc_failed;
		}
	}
	else
	{
		data->image_data[psd_layer_effects_type_stroke] = (psd_argb_color *)psd_malloc(width * height * 4);
		if(data->image_data[psd_layer_effects_type_stroke] == NULL)
			return psd_status_malloc_failed;
	}
	data->width[psd_layer_effects_type_stroke] = width;
	data->height[psd_layer_effects_type_stroke] = height;

	layer_bmp.width = layer->width;
	layer_bmp.height = layer->height;
	layer_bmp.image_data = layer->image_data;
	psd_create_bitmap(&src_bmp, width, height);
	psd_inflate_bitmap(&src_bmp, &layer_bmp, stroke->size, stroke->size);
	dst_bmp.width = width;
	dst_bmp.height = height;
	dst_bmp.image_data = data->image_data[psd_layer_effects_type_stroke];

	psd_fill_bitmap(&dst_bmp, psd_color_clear);
	switch(stroke->position)
	{
		case psd_stroke_outside:
			psd_draw_stroke(&dst_bmp, &src_bmp, stroke->size * 2);
			psd_bitmap_find_edge(&src_bmp, psd_true);
			psd_bitmap_knock_out(&dst_bmp, &src_bmp);
			break;
		case psd_stroke_inside:
			psd_draw_stroke(&dst_bmp, &src_bmp, stroke->size * 2);
			psd_bitmap_find_edge(&src_bmp, psd_true);
			psd_bitmap_reverse_alpha_channel(&src_bmp);
			psd_bitmap_knock_out(&dst_bmp, &src_bmp);
			break;
		case psd_stroke_center:
			psd_draw_stroke(&dst_bmp, &src_bmp, stroke->size);
			break;
		default:
			psd_assert(0);
			break;
	}

	psd_inflate_bitmap(&src_bmp, &layer_bmp, stroke->size, stroke->size);
	memcpy(&layer_mask_info, &layer->layer_mask_info, sizeof(psd_layer_mask_info));
	if(layer_mask_info.disabled == psd_false && (layer_mask_info.default_color != 255 || layer_mask_info.mask_data != NULL))
	{
		layer_mask_info.left -= layer->left - stroke->size;
		layer_mask_info.top -= layer->top - stroke->size;
		layer_mask_info.right -= layer->left - stroke->size;
		layer_mask_info.bottom -= layer->top - stroke->size;
		psd_bitmap_blend_mask(&src_bmp, &layer_mask_info);
	}
	psd_bitmap_reverse_mixed_alpha_channel(&src_bmp);
	psd_bitmap_blend_alpha_channel(&dst_bmp, &src_bmp);

	switch(stroke->fill_type)
	{
		case psd_fill_solid_color:
			psd_fill_bitmap_without_alpha_channel(&dst_bmp, stroke->fill_color);
			break;
			
		case psd_fill_gradient:
			if(stroke->gradient_align == psd_false)
			{
				center_x = context->width / 2 + stroke->gradient_horz_offset * context->width / 100;
				center_y = context->height / 2 + stroke->gradient_vert_offset * context->height / 100;
				center_x -= layer->left;
				center_y -= layer->top;
				corner_angle = (psd_int)(atan((psd_float)context->height / context->width) * 180 / PSD_PI + 0.5);
				angle = stroke->gradient_angle;
				width = (context->width * stroke->gradient_scale + 100) / 200;
				height = (context->height * stroke->gradient_scale + 100) / 200;
			}
			else
			{
				center_x = width / 2 + stroke->gradient_horz_offset * layer->width / 100;
				center_y = height / 2 + stroke->gradient_vert_offset * layer->height / 100;
				switch(stroke->position)
				{
					case psd_stroke_outside:
						width = layer->width + stroke->size * 2;
						height = layer->height + stroke->size * 2;
						break;
					case psd_stroke_inside:
						width = layer->width;
						height = layer->height;
						break;
					case psd_stroke_center:
						width = layer->width + stroke->size;
						height = layer->height + stroke->size;
						break;
				}
				center_x += stroke->gradient_horz_offset * width / 100;
				center_y += stroke->gradient_vert_offset * height / 100;
				corner_angle = (psd_int)(atan((psd_float)height / width) * 180 / PSD_PI + 0.5);
				angle = stroke->gradient_angle;
				width = (width * stroke->gradient_scale + 100) / 200;
				height = (height * stroke->gradient_scale + 100) / 200;
			}
			if(angle < 0)
				angle += 360;
			if(angle >= 90 && angle < 180)
			{
				angle = 180 - angle;
				sign_x = -1;
			}
			else if(angle >= 180 && angle < 270)
			{
				angle = angle - 180;
				sign_x = -1;
				sign_y = -1;
			}
			else if(angle >= 270 && angle <= 360)
			{
				angle = 360 - angle;
				sign_y = -1;
			}
			if(angle <= corner_angle)
			{
				radius_x = width;
				radius_y = (psd_int)(radius_x * PSD_TAN(angle) + 0.5);
			}
			else
			{
				radius_y = height;
				radius_x = (psd_int)(radius_y / PSD_TAN(angle) + 0.5);
			}
			radius_corner = (psd_int)(psd_carm_sqrt((psd_float)(radius_x * radius_x + radius_y * radius_y)) + 0.5);

			switch(stroke->gradient_style)
			{
				case psd_gradient_style_linear:
					psd_gradient_fill_linear(&src_bmp, &stroke->gradient_color, stroke->gradient_reverse, 
						center_x - sign_x * radius_x, center_y + sign_y * radius_y, 
						center_x + sign_x * radius_x, center_y - sign_y * radius_y);
					break;
				case psd_gradient_style_radial:
					psd_gradient_fill_radial(&src_bmp, &stroke->gradient_color, stroke->gradient_reverse, 
						center_x, center_y, radius_corner);
					break;
				case psd_gradient_style_angle:
					psd_gradient_fill_angle(&src_bmp, &stroke->gradient_color, stroke->gradient_reverse, 
						center_x, center_y, stroke->gradient_angle);
					break;
				case psd_gradient_style_reflected:
					psd_gradient_fill_reflected(&src_bmp, &stroke->gradient_color, stroke->gradient_reverse, 
						center_x - sign_x * radius_x, center_y + sign_y * radius_y, 
						center_x + sign_x * radius_x, center_y - sign_y * radius_y);
					break;
				case psd_gradient_style_diamond:
					psd_gradient_fill_diamond(&src_bmp, &stroke->gradient_color, stroke->gradient_reverse, 
						center_x, center_y, radius_corner, stroke->gradient_angle);
					break;
				default:
					psd_assert(0);
					break;
			}

			psd_bitmap_copy_without_alpha_channel(&dst_bmp, &src_bmp);
			break;
			
		case psd_fill_pattern:
			for(i = 0; i < context->pattern_count; i ++)
			{
				if(strcmp(context->patterns[i].unique_id, stroke->pattern_info.identifier) == 0)
				{
					psd_pattern_fill(&src_bmp, &context->patterns[i], stroke->pattern_scale, 
						stroke->pattern_horz_phase + stroke->size, 
						stroke->pattern_vert_phase + stroke->size);
					break;
				}
			}
			
			psd_bitmap_copy_without_alpha_channel(&dst_bmp, &src_bmp);
			break;
			
		default:
			psd_assert(0);
			break;
	}

	psd_free_bitmap(&src_bmp);

	data->valid[psd_layer_effects_type_stroke] = psd_false;
	
	return psd_status_done;
}
#endif

