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
 * $Id: gradient_map.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"


typedef void psd_adjustment_blend_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue);
extern void psd_adjustment_blend_color(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_adjustment_blend_proc * blend_proc, psd_uint layer_info_data);


// Gradient settings
psd_status psd_get_layer_gradient_map(psd_context * context, psd_layer_record * layer)
{
	psd_layer_gradient_map * data;
	psd_int i;
	psd_status status;
	psd_color_space color_model;
	psd_ushort color_component[4];
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_gradient_map;
	layer->layer_type = psd_layer_type_gradient_map;

	data = (psd_layer_gradient_map *)psd_malloc(sizeof(psd_layer_gradient_map));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_gradient_map));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Version ( =1 for Photoshop 6.0)
	if(psd_stream_get_short(context) != 1)
		return psd_status_gradient_map_unsupport_version;

	// Is gradient reverse
	data->reverse = psd_stream_get_bool(context);

	// Is gradient dithered
	data->dithered = psd_stream_get_bool(context);

	// Name of the gradient: Unicode string, padded
	data->name_length = psd_stream_get_int(context);
	data->name = (psd_ushort *)psd_malloc(2 * data->name_length);
	if(data->name == NULL)
		return psd_status_malloc_failed;
	psd_stream_get(context, (psd_uchar *)data->name, 2 * data->name_length);

	// Number of color stops to follow
	data->number_color_stops = psd_stream_get_short(context);
	data->color_stop = (psd_gradient_color_stop *)psd_malloc(data->number_color_stops * sizeof(psd_gradient_color_stop));
	if(data->color_stop == NULL)
		return psd_status_malloc_failed;
	memset(data->color_stop, 0, data->number_color_stops * sizeof(psd_gradient_color_stop));

	// Each color stop
	for(i = 0; i < data->number_color_stops; i ++)
	{
		// Location of color stop
		data->color_stop[i].location = psd_stream_get_int(context);
		// Midpoint of color stop
		data->color_stop[i].midpoint = psd_stream_get_int(context);
		
		// Actual color for the stop
		data->color_stop[i].actual_color = psd_stream_get_space_color(context);

		// padding, I don't find any information from adobe sdk
		psd_stream_get_short(context);
		
		data->color_stop[i].color_stop_type = psd_color_stop_type_user_stop;
	}

	// Number of transparency stops to follow
	data->number_transparency_stops = psd_stream_get_short(context);
	data->transparency_stop = (psd_gradient_transparency_stop *)psd_malloc(data->number_transparency_stops * sizeof(psd_gradient_transparency_stop));
	if(data->transparency_stop == NULL)
		return psd_status_malloc_failed;
	memset(data->transparency_stop, 0, data->number_transparency_stops * sizeof(psd_gradient_transparency_stop));

	// Each transparency stop
	for(i = 0; i < data->number_transparency_stops; i ++)
	{
		// Location of transparency stop
		data->transparency_stop[i].location = psd_stream_get_int(context);
		// Midpoint of transparency stop
		data->transparency_stop[i].midpoint = psd_stream_get_int(context);
		// Opacity of transparency stop
		data->transparency_stop[i].opacity = psd_stream_get_short(context);
	}

	// Expansion count ( = 2 for Photoshop 6.0)
	data->expansion_count = psd_stream_get_short(context);
	if(data->expansion_count > 0)
	{
		// Interpolation if length above is non-zero
		data->interpolation = psd_stream_get_short(context);
		// Length (= 32 for Photoshop 6.0)
		data->length = psd_stream_get_short(context);
		if(data->length >= 32)
		{
			// Mode for this gradient
			data->mode = psd_stream_get_short(context);
			// Random number seed
			data->random_number_seed = psd_stream_get_int(context);
			// Flag for showing transparency
			data->showing_transparency_flag = psd_stream_get_short(context);
			// Flag for using vector color
			data->using_vector_color_flag = psd_stream_get_short(context);
			// Roughness factor
			data->roughness_factor = psd_stream_get_int(context);
			
			// Color model
			color_model = psd_stream_get_short(context);
			// 4 * 2 Minimum color values
			for(i = 0; i < 4; i ++)
				color_component[i] = psd_stream_get_short(context) >> 8;
			status = psd_color_space_to_argb(&data->min_color, color_model, 
				color_component);
			//if(status != psd_status_done)
			//	return status;
			// 4 * 2 Maximum color values
			for(i = 0; i < 4; i ++)
				color_component[i] = psd_stream_get_short(context) >> 8;
			status = psd_color_space_to_argb(&data->max_color, color_model, 
				color_component);
			//if(status != psd_status_done)
			//	return status;
		}
	}
	
	// Dummy: not used in Photoshop 6.0
	psd_stream_get_short(context);

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

void psd_layer_gradient_map_free(psd_uint info_data)
{
	psd_layer_gradient_map * data;

	data = (psd_layer_gradient_map *)info_data;
	psd_freeif(data->name);
	psd_freeif(data->color_stop);
	psd_freeif(data->transparency_stop);
	psd_free(data);
}

static void psd_gradient_map_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue)
{
	psd_layer_gradient_map * data = (psd_layer_gradient_map *)layer_info_data;
	psd_int gray;
	psd_argb_color color;
	
	gray = PSD_GET_COLOR_INTENSITY(*red, *green, *blue);
	if(data->reverse == psd_true)
		color = data->lookup_table[255 - gray];
	else
		color = data->lookup_table[gray];

	*red = PSD_GET_RED_COMPONENT(color);
	*green = PSD_GET_GREEN_COMPONENT(color);
	*blue = PSD_GET_BLUE_COMPONENT(color);
}

psd_bool psd_layer_blend_gradient_map(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_uint layer_info_data;
    psd_layer_gradient_map * data = NULL;
	psd_int i, j, start_pos, end_pos, mid_pos;
	psd_int start_red, start_green, start_blue, 
		end_red, end_green, end_blue,
		mid_red, mid_green, mid_blue;
	psd_int dst_red, dst_green, dst_blue;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_gradient_map)
		{
			layer_info_data = layer->layer_info_data[i];
			data = (psd_layer_gradient_map *)layer_info_data;
			break;
		}
	}
	if(data == NULL)
		return psd_false;

	if(layer->adjustment_valid == psd_true)
	{
		psd_color_memset(data->lookup_table, data->color_stop[0].actual_color, data->color_stop[0].location / 16);
		
		for(i = 0; i < data->number_color_stops - 1; i ++)
		{
			start_pos = data->color_stop[i].location / 16;
			end_pos = data->color_stop[i + 1].location / 16;
			mid_pos = start_pos + (end_pos - start_pos) * data->color_stop[i].midpoint / 100;
			start_red = psd_get_red_component( data->color_stop[i].actual_color);
			start_green = psd_get_green_component( data->color_stop[i].actual_color);
			start_blue = psd_get_blue_component( data->color_stop[i].actual_color);
			end_red = psd_get_red_component( data->color_stop[i + 1].actual_color);
			end_green = psd_get_green_component( data->color_stop[i + 1].actual_color);
			end_blue = psd_get_blue_component( data->color_stop[i + 1].actual_color);
			mid_red = (start_red + end_red) / 2;
			mid_green = (start_green + end_green) / 2;
			mid_blue = (start_blue + end_blue) / 2;
			
			for(j = start_pos; j < mid_pos; j ++)
			{
				dst_red = start_red + (mid_red - start_red) * (j - start_pos) / (mid_pos - start_pos);
				dst_green = start_green + (mid_green - start_green) * (j - start_pos) / (mid_pos - start_pos);
				dst_blue = start_blue + (mid_blue - start_blue) * (j - start_pos) / (mid_pos - start_pos);
				data->lookup_table[j] = PSD_RGB_TO_COLOR(dst_red, dst_green, dst_blue);
			}

			for(j = mid_pos; j < end_pos; j ++)
			{
				dst_red = mid_red + (end_red - mid_red) * (j - mid_pos) / (end_pos - mid_pos);
				dst_green = mid_green + (end_green - mid_green) * (j - mid_pos) / (end_pos - mid_pos);
				dst_blue = mid_blue + (end_blue - mid_blue) * (j - mid_pos) / (end_pos - mid_pos);
				data->lookup_table[j] = PSD_RGB_TO_COLOR(dst_red, dst_green, dst_blue);
			}
		}

		psd_color_memset(data->lookup_table + data->color_stop[data->number_color_stops - 1].location / 16, 
			data->color_stop[data->number_color_stops - 1].actual_color, 
			256 - data->color_stop[data->number_color_stops - 1].location / 16);
	}

	psd_adjustment_blend_color(context, layer, dst_rect, psd_gradient_map_proc, layer_info_data);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

