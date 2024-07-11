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
 * $Id: adjustment.c, created by Patrick in 2006.06.06, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_blend.h"
#include "psd_math.h"


typedef void psd_adjustment_blend_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue);

extern psd_bool psd_layer_blend_levels(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_curves(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_brightness_contrast(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_invert(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_color_balance(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_hue_saturation(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_selective_color(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_threshold(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_posterize(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_channel_mixer(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_gradient_map(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_blend_photo_filter(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);


psd_status psd_adjustment_layer_update(psd_layer_record * layer)
{
	if(layer == NULL)
		return psd_status_invalid_layer;
	if(layer->layer_type == psd_layer_type_normal)
		return psd_status_invalid_adjustment_layer;
	
	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

psd_bool psd_adjustment_layer_blend(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_bool result = psd_false;
	
	if(layer->layer_type < psd_layer_type_levels)
		return psd_false;
	
	if(layer->image_data == NULL)
	{
		layer->image_data = (psd_argb_color *)psd_malloc(context->width * context->height * 4);
		if(layer->image_data == NULL)
			return psd_false;
		layer->right = layer->width = context->width;
		layer->bottom = layer->height = context->height;
	}

	switch(layer->layer_type)
	{
		case psd_layer_type_levels:
			result = psd_layer_blend_levels(context, layer, dst_rect);
			break;
		case psd_layer_type_curves:
			result = psd_layer_blend_curves(context, layer, dst_rect);
			break;
		case psd_layer_type_brightness_contrast:
			result = psd_layer_blend_brightness_contrast(context, layer, dst_rect);
			break;
		case psd_layer_type_color_balance:
			result = psd_layer_blend_color_balance(context, layer, dst_rect);
			break;
		case psd_layer_type_hue_saturation:
			result = psd_layer_blend_hue_saturation(context, layer, dst_rect);
			break;
		case psd_layer_type_selective_color:
			result = psd_layer_blend_selective_color(context, layer, dst_rect);
			break;
		case psd_layer_type_threshold:
			result = psd_layer_blend_threshold(context, layer, dst_rect);
			break;
		case psd_layer_type_invert:
			result = psd_layer_blend_invert(context, layer, dst_rect);
			break;
		case psd_layer_type_posterize:
			result = psd_layer_blend_posterize(context, layer, dst_rect);
			break;
		case psd_layer_type_channel_mixer:
			result = psd_layer_blend_channel_mixer(context, layer, dst_rect);
			break;
		case psd_layer_type_gradient_map:
			result = psd_layer_blend_gradient_map(context, layer, dst_rect);
			break;
		case psd_layer_type_photo_filter:
			result = psd_layer_blend_photo_filter(context, layer, dst_rect);
			break;
	}

	return result;
}

void psd_adjustment_blend_image(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, psd_uchar * lookup_table)
{
	psd_int i, j, width, height;
	psd_int dst_red, dst_green, dst_blue, dst_alpha, src_red, src_green, src_blue;
	psd_argb_color *src_data, * dst_data, src_color;
	
	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	
	for(i = 0; i < height; i ++)
	{
		src_data = context->blending_image_data + (dst_rect->top + i) * context->width + dst_rect->left;
		dst_data = layer->image_data + (dst_rect->top + i) * layer->width + dst_rect->left;
		
		for(j = 0; j < width; j ++, src_data ++, dst_data ++)
		{
			src_color = *src_data;
			dst_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
			src_red = PSD_GET_RED_COMPONENT(src_color);
			src_green = PSD_GET_GREEN_COMPONENT(src_color);
			src_blue = PSD_GET_BLUE_COMPONENT(src_color);
			
			dst_red = lookup_table[src_red];
			dst_green = lookup_table[src_green];
			dst_blue = lookup_table[src_blue];
			
			*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
		}
	}
}

void psd_adjustment_blend_rgb(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_uchar * red_lookup_table, psd_uchar * green_lookup_table, psd_uchar * blue_lookup_table,
	psd_bool preserve_luminosity)
{
	psd_int i, j, width, height;
	psd_int dst_red, dst_green, dst_blue, dst_alpha, src_red, src_green, src_blue;
	psd_argb_color *src_data, * dst_data, src_color;
	psd_int dst_luminosity, src_luminosity, value;
	
	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	
	for(i = 0; i < height; i ++)
	{
		src_data = context->blending_image_data + (dst_rect->top + i) * context->width + dst_rect->left;
		dst_data = layer->image_data + (dst_rect->top + i) * layer->width + dst_rect->left;
		
		for(j = 0; j < width; j ++, src_data ++, dst_data ++)
		{
			src_color = *src_data;
			dst_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
			src_red = PSD_GET_RED_COMPONENT(src_color);
			src_green = PSD_GET_GREEN_COMPONENT(src_color);
			src_blue = PSD_GET_BLUE_COMPONENT(src_color);
			
			dst_red = red_lookup_table[src_red];
			dst_green = green_lookup_table[src_green];
			dst_blue = blue_lookup_table[src_blue];

			if(preserve_luminosity == psd_true)
			{
				src_luminosity = psd_rgb_get_brightness(src_red, src_green, src_blue);
				dst_luminosity = psd_rgb_get_brightness(dst_red, dst_green, dst_blue);
				value = dst_red * src_luminosity / dst_luminosity;
				src_red = PSD_CONSTRAIN(value, 0, 255);
				value = dst_green * src_luminosity / dst_luminosity;
				src_green = PSD_CONSTRAIN(value, 0, 255);
				value = dst_blue * src_luminosity / dst_luminosity;
				src_blue = PSD_CONSTRAIN(value, 0, 255);
			}
			
			*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
		}
	}
}

void psd_adjustment_blend_gray(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, psd_uchar * lookup_table)
{
	psd_int i, j, width, height;
	psd_int dst_red, dst_green, dst_blue, dst_alpha, src_red, src_green, src_blue;
	psd_argb_color *src_data, * dst_data, src_color;
	psd_int gray;
	
	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	
	for(i = 0; i < height; i ++)
	{
		src_data = context->blending_image_data + (dst_rect->top + i) * context->width + dst_rect->left;
		dst_data = layer->image_data + (dst_rect->top + i) * layer->width + dst_rect->left;
		
		for(j = 0; j < width; j ++, src_data ++, dst_data ++)
		{
			src_color = *src_data;
			dst_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
			src_red = PSD_GET_RED_COMPONENT(src_color);
			src_green = PSD_GET_GREEN_COMPONENT(src_color);
			src_blue = PSD_GET_BLUE_COMPONENT(src_color);
			
			gray = PSD_GET_COLOR_INTENSITY(src_red, src_green, src_blue);
			dst_red = lookup_table[gray];
			dst_green = lookup_table[gray];
			dst_blue = lookup_table[gray];
			
			*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
		}
	}
}

void psd_adjustment_blend_color(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_adjustment_blend_proc * blend_proc, psd_uint layer_info_data)
{
	psd_int i, j, width, height;
	psd_int dst_red, dst_green, dst_blue, dst_alpha, src_red, src_green, src_blue;
	psd_argb_color *src_data, * dst_data, src_color;
	
	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	
	for(i = 0; i < height; i ++)
	{
		src_data = context->blending_image_data + (dst_rect->top + i) * context->width + dst_rect->left;
		dst_data = layer->image_data + (dst_rect->top + i) * layer->width + dst_rect->left;
		
		for(j = 0; j < width; j ++, src_data ++, dst_data ++)
		{
			src_color = *src_data;
			dst_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
			src_red = PSD_GET_RED_COMPONENT(src_color);
			src_green = PSD_GET_GREEN_COMPONENT(src_color);
			src_blue = PSD_GET_BLUE_COMPONENT(src_color);
			
			dst_red = src_red;
			dst_green = src_green;
			dst_blue = src_blue;
			(*blend_proc)(layer_info_data, &dst_red, &dst_green, &dst_blue);
			
			*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
		}
	}
}
