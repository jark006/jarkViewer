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
 * $Id: channel_mixer, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_math.h"


typedef void psd_adjustment_blend_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue);
extern void psd_adjustment_blend_color(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_adjustment_blend_proc * blend_proc, psd_uint layer_info_data);


psd_status psd_get_layer_channel_mixer(psd_context * context, psd_layer_record * layer)
{
	psd_layer_channel_mixer * data;
	psd_int i;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_channel_mixer;
	layer->layer_type = psd_layer_type_channel_mixer;

	data = (psd_layer_channel_mixer *)psd_malloc(sizeof(psd_layer_channel_mixer));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_channel_mixer));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Version ( = 1)
	if(psd_stream_get_short(context) != 1)
		return psd_status_channel_mixer_unsupport_version;

	// Monochrome
	data->monochrome = (psd_bool)psd_stream_get_short(context);

	// RGB or CMYK color plus constant for the mixer settings. 4 * 2 bytes of
	// color with 2 bytes of constant.
	for(i = 0; i < 4; i ++)
	{
		data->red_cyan[i] = psd_stream_get_short(context);
		data->green_magenta[i] = psd_stream_get_short(context);
		data->blue_yellow[i] = psd_stream_get_short(context);
		data->black[i] = psd_stream_get_short(context);
		data->constant[i] = psd_stream_get_short(context);
	}

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

static void psd_channel_mixer_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue)
{
	psd_layer_channel_mixer * data = (psd_layer_channel_mixer *)layer_info_data;
	psd_int dst_red, dst_green, dst_blue, src_red, src_green, src_blue, gray;
	
	src_red = *red;
	src_green = *green;
	src_blue = *blue;

	if(data->monochrome == psd_true)
	{
		gray = 0;
		gray += src_red * data->red_cyan[0] / 100;
		gray += src_green * data->green_magenta[0] / 100;
		gray += src_blue * data->blue_yellow[0] / 100;
		gray += data->constant[0] * 255 / 100;
		gray = PSD_CONSTRAIN(gray, 0, 255);
		*red = gray;
		*green = gray;
		*blue = gray;
	}
	else
	{
		if(data->red_cyan[0] != 100 || data->green_magenta[0] != 0 || 
			data->blue_yellow[0] != 0 || data->constant[0] != 0)
		{
			dst_red = 0;
			dst_red += src_red * data->red_cyan[0] / 100;
			dst_red += src_green * data->green_magenta[0] / 100;
			dst_red += src_blue * data->blue_yellow[0] / 100;
			dst_red += data->constant[0] * 255 / 100;
			*red = PSD_CONSTRAIN(dst_red, 0, 255);
		}

		if(data->green_magenta[1] != 100 || data->red_cyan[1] != 0 || 
			data->blue_yellow[1] != 0 || data->constant[1] != 0)
		{
			dst_green = 0;
			dst_green += src_green * data->green_magenta[1] / 100;
			dst_green += src_red * data->red_cyan[1] / 100;
			dst_green += src_blue * data->blue_yellow[1] / 100;
			dst_green += data->constant[1] * 255 / 100;
			*green = PSD_CONSTRAIN(dst_green, 0, 255);
		}

		if(data->blue_yellow[2] != 100 || data->red_cyan[2] != 0 || 
			data->green_magenta[2] != 0 || data->constant[2] != 0)
		{
			dst_blue = 0;
			dst_blue += src_blue * data->blue_yellow[2] / 100;
			dst_blue += src_red * data->red_cyan[2] / 100;
			dst_blue += src_green * data->green_magenta[2] / 100;
			dst_blue += data->constant[2] * 255 / 100;
			*blue = PSD_CONSTRAIN(dst_blue, 0, 255);
		}
	}
}

psd_bool psd_layer_blend_channel_mixer(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_uint layer_info_data;
	psd_int i;

	if(context->color_mode != psd_color_mode_rgb)
		return psd_false;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_channel_mixer)
		{
			layer_info_data = layer->layer_info_data[i];
			break;
		}
	}
	if(layer_info_data == 0)
		return psd_false;

	psd_adjustment_blend_color(context, layer, dst_rect, psd_channel_mixer_proc, layer_info_data);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

