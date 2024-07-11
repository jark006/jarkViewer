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
 * $Id: selective_color.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_math.h"


typedef void psd_adjustment_blend_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue);
extern void psd_adjustment_blend_color(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_adjustment_blend_proc * blend_proc, psd_uint layer_info_data);


psd_status psd_get_layer_selective_color(psd_context * context, psd_layer_record * layer)
{
	psd_layer_selective_color * data;
	psd_int i;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_selective_color;
	layer->layer_type = psd_layer_type_selective_color;

	data = (psd_layer_selective_color *)psd_malloc(sizeof(psd_layer_selective_color));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_selective_color));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Version ( = 1)
	if(psd_stream_get_short(context) != 1)
		return psd_status_selective_color_unsupport_version;

	// Correction method.. 0 = Apply color correction in relative mode; 1 =
	// Apply color correction in absolute mode.	
	data->correction_method = psd_stream_get_short(context);

	// Ten eight-byte plate correction records.
	// The first record is ignored by Photoshop and is reserved for future use. It
	// should be set to all zeroes.
	// The rest of the records apply to specific areas of colors or lightness
	// values in the image, in the following order: reds, yellows, greens, cyans,
	// blues, magentas, whites, neutrals, blacks.
	for(i = 0; i < 10; i ++)
	{
		// Amount of cyan correction. Short integer from 每100...100.
		data->cyan_correction[i] = psd_stream_get_short(context);
		// Amount of magenta correction. Short integer from 每100...100.
		data->magenta_correction[i] = psd_stream_get_short(context);
		// Amount of yellow correction. Short integer from 每100...100.
		data->yellow_correction[i] = psd_stream_get_short(context);
		// Amount of black correction. Short integer from 每100...100.
		data->black_correction[i] = psd_stream_get_short(context);
		psd_assert(data->cyan_correction[i] >= -100 && data->cyan_correction[i] <= 100);
		psd_assert(data->magenta_correction[i] >= -100 && data->cyan_correction[i] <= 100);
		psd_assert(data->yellow_correction[i] >= -100 && data->cyan_correction[i] <= 100);
		psd_assert(data->black_correction[i] >= -100 && data->cyan_correction[i] <= 100);
	}

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

static void psd_selective_color_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue)
{
	psd_layer_selective_color * data = (psd_layer_selective_color *)layer_info_data;
	psd_int hue, saturation, lightness;
	psd_int src_cyan, src_magenta, src_yellow, src_black, dst_cyan, dst_magenta, dst_yellow, dst_black;
	psd_int opacity, i, range[4];

	psd_rgb_to_intcmyk(*red, *green, *blue, 
		&src_cyan, &src_magenta, &src_yellow, &src_black);
	dst_cyan = src_cyan;
	dst_magenta = src_magenta;
	dst_yellow = src_yellow;
	dst_black = src_black;
	psd_rgb_to_inthsb(*red, *green, *blue, &hue, &saturation, &lightness);

	for(i = 1; i < 7; i ++)
	{
		range[0] = -105 + i * 60;
		range[1] = range[0] + 30;
		range[2] = range[1] + 30;
		range[3] = range[2] + 30;
		if(hue >= range[0] && hue < range[3])
		{
			if(hue >= range[1] && hue < range[2])
				opacity = 255;
			else if(hue < range[1])
				opacity = (hue - range[0]) * 255 / 30;
			else
				opacity = (range[3] - hue) * 255 / 30;
				
			if(data->cyan_correction[i] != 0 || data->magenta_correction[i] != 0 ||
				data->yellow_correction[i] != 0 || data->black_correction[i] != 0)
			{
				if(data->correction_method == 0)
				{
					if(data->cyan_correction[i] != 0)
						dst_cyan += src_cyan * data->cyan_correction[i] * opacity / 25500;
					if(data->magenta_correction[i] != 0)
						dst_magenta += src_magenta * data->magenta_correction[i] * opacity / 25500;
					if(data->yellow_correction[i] != 0)
						dst_yellow += src_yellow * data->yellow_correction[i] * opacity / 25500;
					if(data->black_correction[i] != 0)
						dst_black += src_black * data->black_correction[i] * opacity / 25500;
				}
				else
				{
					if(data->cyan_correction[i] != 0)
						dst_cyan += 255 * data->cyan_correction[i] * opacity / 25500;
					if(data->magenta_correction[i] != 0)
						dst_magenta += 255 * data->magenta_correction[i] * opacity / 25500;
					if(data->yellow_correction[i] != 0)
						dst_yellow += 255 * data->yellow_correction[i] * opacity / 25500;
					if(data->black_correction[i] != 0)
						dst_black += 255 * data->black_correction[i] * opacity / 25500;
				}
			}
		}
	}
	
	for(i = 7; i < 10; i ++)
	{
		if((i == 7 && src_black == 0) ||
			(i == 8 && src_black > 0 && src_black < 255) ||
			(i == 9 && src_black == 255))
		{
			if(data->correction_method == 0)
			{
				if(data->cyan_correction[i] != 0)
					dst_cyan += src_cyan * data->cyan_correction[i] / 100;
				if(data->magenta_correction[i] != 0)
					dst_magenta += src_magenta * data->magenta_correction[i] / 100;
				if(data->yellow_correction[i] != 0)
					dst_yellow += src_yellow * data->yellow_correction[i] / 100;
				if(data->black_correction[i] != 0)
					dst_black += src_black * data->black_correction[i] / 100;
			}
			else
			{
				if(data->cyan_correction[i] != 0)
					dst_cyan += 255 * data->cyan_correction[i] / 100;
				if(data->magenta_correction[i] != 0)
					dst_magenta += 255 * data->magenta_correction[i] / 100;
				if(data->yellow_correction[i] != 0)
					dst_yellow += 255 * data->yellow_correction[i] / 100;
				if(data->black_correction[i] != 0)
					dst_black += 255 * data->black_correction[i] / 100;
			}
		}
	}

	dst_cyan = PSD_CONSTRAIN(dst_cyan, 0, 255);
	dst_magenta = PSD_CONSTRAIN(dst_magenta, 0, 255);
	dst_yellow = PSD_CONSTRAIN(dst_yellow, 0, 255);
	dst_black = PSD_CONSTRAIN(dst_black, 0, 255);
	psd_intcmyk_to_rgb(dst_cyan, dst_magenta, dst_yellow, dst_black, 
		red, green, blue);
}

psd_bool psd_layer_blend_selective_color(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_uint layer_info_data;
	psd_layer_selective_color * data = NULL;
	psd_int i;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_selective_color)
		{
			layer_info_data = layer->layer_info_data[i];
			data = (psd_layer_selective_color *)layer_info_data;
			break;
		}
	}
	if(data == NULL)
		return psd_false;

	psd_adjustment_blend_color(context, layer, dst_rect, psd_selective_color_proc, layer_info_data);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

