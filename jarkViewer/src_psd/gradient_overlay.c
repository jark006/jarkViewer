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
 * $Id: gradient_overlay.c, created by Patrick in 2006.06.18, libpsd@graphest.com Exp $
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
#include "psd_gradient.h"
#include "psd_math.h"


static void psd_set_layer_gradient_overlay_default(psd_layer_effects_gradient_overlay * gradient_overlay)
{
	gradient_overlay->blend_mode = psd_blend_mode_normal;
	gradient_overlay->opacity = 255;
	gradient_overlay->reverse = psd_false;
	gradient_overlay->style = 0;
	gradient_overlay->align_width_layer = psd_true;
	gradient_overlay->angle = 90;
	gradient_overlay->scale = 100;
}

psd_status psd_get_layer_gradient_overlay2(psd_context * context, psd_layer_effects_gradient_overlay * gradient_overlay)
{
	psd_int length, number_items;
	psd_uint rootkey, type, key;
	psd_uchar keychar[256];
	
	psd_set_layer_gradient_overlay_default(gradient_overlay);

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
				gradient_overlay->effect_enable = psd_stream_get_bool(context);
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
				gradient_overlay->blend_mode = psd_stream_get_object_blend_mode(context);
				break;

			// opacity
			case 'Opct':
				psd_assert(type == 'UntF');
				// percent
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
				// Actual value (double)
				gradient_overlay->opacity = (psd_int)(psd_stream_get_double(context) * 2.55 + 0.5);
				break;
				
			// gradient color
			case 'Grad':
				// Descriptor
				psd_assert(type == 'Objc');
				psd_stream_get_object_gradient_color(&gradient_overlay->gradient_color, context);
				break;

			case 'Angl':	// angle
				// Unit psd_float
				psd_assert(type == 'UntF');
				
				// '#Ang' = angle: base degrees
				key = psd_stream_get_int(context);
				psd_assert(key == '#Ang');

				// Actual value (double)
				gradient_overlay->angle = (psd_int)psd_stream_get_double(context);
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
				gradient_overlay->style = psd_stream_get_object_gradient_style(context);
				break;

			case 'Rvrs':	// reverse
				// boolean
				psd_assert(type == 'bool');
				gradient_overlay->reverse = psd_stream_get_bool(context);
				break;

			case 'Algn':	// align with layer
				// boolean
				psd_assert(type == 'bool');
				gradient_overlay->align_width_layer = psd_stream_get_bool(context);
				break;

			case 'Scl ':	// scale
				// Unit psd_float
				psd_assert(type == 'UntF');

				// '#Prc' = percent:
				key = psd_stream_get_int(context);
				psd_assert(key == '#Prc');
								
				// Actual value (double)
				gradient_overlay->scale = (psd_int)psd_stream_get_double(context);
				break;

			// offset, not documented
			case 'Ofst':
				psd_assert(type == 'Objc');
				psd_stream_get_object_point(&gradient_overlay->horz_offset, 
					&gradient_overlay->vert_offset, context);
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
psd_status psd_layer_effects_blend_gradient_overlay(psd_context * context, psd_layer_record * layer, psd_layer_effects * data)
{
	psd_layer_effects_gradient_overlay * gradient_overlay = &data->gradient_overlay;
	psd_bitmap src_bmp, dst_bmp;
	psd_layer_mask_info layer_mask_info;
	psd_int width, height, center_x, center_y, radius_x, radius_y, radius_corner;
	psd_int corner_angle, angle;
	psd_int sign_x = 1, sign_y = 1;

	data->left[psd_layer_effects_type_gradient_overlay] = 0;
	data->top[psd_layer_effects_type_gradient_overlay] = 0;
	data->right[psd_layer_effects_type_gradient_overlay] = layer->width;
	data->bottom[psd_layer_effects_type_gradient_overlay] = layer->height;
	data->blend_mode[psd_layer_effects_type_gradient_overlay] = gradient_overlay->blend_mode;
	data->opacity[psd_layer_effects_type_gradient_overlay] = gradient_overlay->opacity;
	data->width[psd_layer_effects_type_gradient_overlay] = layer->width;
	data->height[psd_layer_effects_type_gradient_overlay] = layer->height;

	if(data->image_data[psd_layer_effects_type_gradient_overlay] == NULL)
	{
		data->image_data[psd_layer_effects_type_gradient_overlay] = (psd_argb_color *)psd_malloc(layer->width * layer->height * 4);
		if(data->image_data[psd_layer_effects_type_gradient_overlay] == NULL)
			return psd_status_malloc_failed;
	}

	src_bmp.width = layer->width;
	src_bmp.height = layer->height;
	src_bmp.image_data = layer->image_data;
	dst_bmp.width = layer->width;
	dst_bmp.height = layer->height;
	dst_bmp.image_data = data->image_data[psd_layer_effects_type_gradient_overlay];

	if(gradient_overlay->align_width_layer == psd_false)
	{
		center_x = context->width / 2 + gradient_overlay->horz_offset * context->width / 100;
		center_y = context->height / 2 + gradient_overlay->vert_offset * context->height / 100;
		center_x -= layer->left;
		center_y -= layer->top;
		corner_angle = (psd_int)(atan((psd_float)context->height / context->width) * 180 / PSD_PI + 0.5);
		angle = gradient_overlay->angle;
		width = (context->width * gradient_overlay->scale + 100) / 200;
		height = (context->height * gradient_overlay->scale + 100) / 200;
	}
	else
	{
		center_x = layer->width / 2 + gradient_overlay->horz_offset * layer->width / 100;
		center_y = layer->height / 2 + gradient_overlay->vert_offset * layer->height / 100;
		corner_angle = (psd_int)(atan((psd_float)layer->height / layer->width) * 180 / PSD_PI + 0.5);
		angle = gradient_overlay->angle;
		width = (layer->width * gradient_overlay->scale + 100) / 200;
		height = (layer->height * gradient_overlay->scale + 100) / 200;
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

	switch(gradient_overlay->style)
	{
		case psd_gradient_style_linear:
			psd_gradient_fill_linear(&dst_bmp, &gradient_overlay->gradient_color, gradient_overlay->reverse, 
				center_x - sign_x * radius_x, center_y + sign_y * radius_y, 
				center_x + sign_x * radius_x, center_y - sign_y * radius_y);
			break;
		case psd_gradient_style_radial:
			psd_gradient_fill_radial(&dst_bmp, &gradient_overlay->gradient_color, gradient_overlay->reverse, 
				center_x, center_y, radius_corner);
			break;
		case psd_gradient_style_angle:
			psd_gradient_fill_angle(&dst_bmp, &gradient_overlay->gradient_color, gradient_overlay->reverse, 
				center_x, center_y, gradient_overlay->angle);
			break;
		case psd_gradient_style_reflected:
			psd_gradient_fill_reflected(&dst_bmp, &gradient_overlay->gradient_color, gradient_overlay->reverse, 
				center_x - sign_x * radius_x, center_y + sign_y * radius_y, 
				center_x + sign_x * radius_x, center_y - sign_y * radius_y);
			break;
		case psd_gradient_style_diamond:
			psd_gradient_fill_diamond(&dst_bmp, &gradient_overlay->gradient_color, gradient_overlay->reverse, 
				center_x, center_y, radius_corner, gradient_overlay->angle);
			break;
		default:
			psd_assert(0);
			break;
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

	data->valid[psd_layer_effects_type_gradient_overlay] = psd_false;
	
	return psd_status_done;
}
#endif

