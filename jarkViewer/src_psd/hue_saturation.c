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
 * $Id: hue_saturation.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
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


// New Hue/saturation, Photoshop 5.0
psd_status psd_get_layer_hue_saturation(psd_context * context, psd_layer_record * layer)
{
	psd_layer_hue_saturation * data;
	psd_int i, j;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_new_hue_saturation;
	layer->layer_type = psd_layer_type_hue_saturation;

	data = (psd_layer_hue_saturation *)psd_malloc(sizeof(psd_layer_hue_saturation));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_hue_saturation));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Version ( = 2)
	if(psd_stream_get_short(context) != 2)
		return psd_status_hue_saturation_unsupport_version;

	// 0 = Use settings for hue-adjustment; 1 = Use settings for colorization.
	data->hue_or_colorization = psd_stream_get_char(context);

	// Padding byte; must be present but is ignored by Photoshop.
	psd_stream_get_char(context);

	// Colorization.
	// Photoshop 5.0: The actual values are stored for the new version. Hue is -
	// 180...180, Saturation is 0...100, and Lightness is -100...100.
	// Photoshop 4.0: Three psd_short integers Hue, Saturation, and Lightness
	// from 每100...100. The user interface represents hue as 每180...180,
	// saturation as 0...100, and Lightness as -100...1000, as the traditional
	// HSB color wheel, with red = 0.
	data->colorization_hue = psd_stream_get_short(context);
	data->colorization_saturation = psd_stream_get_short(context);
	data->colorization_lightness = psd_stream_get_short(context);

	// Master hue, saturation and lightness values.
	data->master_hue = psd_stream_get_short(context);
	data->master_saturation = psd_stream_get_short(context);
	data->master_lightness = psd_stream_get_short(context);

	// 6 sets of the following 14 bytes (4 range values followed by 3 settings values)
	for(i = 0; i < 6; i ++)
	{
		//For RGB and CMYK, those values apply to each of the six hextants in
		// the HSB color wheel: those image pixels nearest to red, yellow, green,
		// cyan, blue, or magenta. These numbers appear in the user interface
		// from 每60...60, however the slider will reflect each of the possible 201
		// values from 每100...100.
		for(j = 0; j < 4; j ++)
			data->range_values[i][j] = psd_stream_get_short(context);

		// For Lab, the first four of the six values are applied to image pixels in the
		// four Lab color quadrants, yellow, green, blue, and magenta. The other
		// two values are ignored ( = 0). The values appear in the user interface
		// from 每90 to 90.
		for(j = 0; j < 3; j ++)
			data->setting_values[i][j] = psd_stream_get_short(context);
	}

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

static void psd_hue_saturation_proc(psd_uint layer_info_data, psd_int * red, psd_int * green, psd_int * blue)
{
	psd_layer_hue_saturation * data = (psd_layer_hue_saturation *)layer_info_data;
	psd_int src_hue, src_saturation, src_lightness;
	psd_int dst_hue, dst_saturation, dst_lightness;
	psd_int i, opacity;

	psd_rgb_to_inthsb(*red, *green, *blue, &src_hue, &src_saturation, &src_lightness);
	
	dst_hue = data->master_hue;
	dst_saturation = data->master_saturation;
	dst_lightness = data->master_lightness;
	for(i = 0; i < 6; i ++)
	{
		opacity = data->lookup_table[i][src_hue];
		if(opacity > 0)
		{
			dst_hue += data->setting_values[i][0] * opacity / 255;
			dst_saturation += data->setting_values[i][1] * opacity / 255;
			dst_lightness += data->setting_values[i][2] * opacity / 255;
		}
	}

	if(dst_hue != 0 || dst_saturation != 0 || dst_lightness != 0)
	{
		dst_hue = (src_hue + dst_hue + 2160) % 360;
		dst_saturation = src_saturation + src_saturation * PSD_CONSTRAIN(dst_saturation, -100, 100) / 100;
		dst_saturation = PSD_CONSTRAIN(dst_saturation, 0, 255);
		dst_lightness = src_lightness + src_lightness * PSD_CONSTRAIN(dst_lightness, -100, 100) / 100;
		dst_lightness = PSD_CONSTRAIN(dst_lightness, 0, 255);
		
		psd_inthsb_to_rgb(dst_hue, dst_saturation, dst_lightness, red, green, blue);
	}
}

psd_bool psd_layer_blend_hue_saturation(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_uint layer_info_data;
	psd_layer_hue_saturation * data = NULL;
	psd_int i, j, increase, range_value[4];

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_new_hue_saturation)
		{
			layer_info_data = layer->layer_info_data[i];
			data = (psd_layer_hue_saturation *)layer_info_data;
			break;
		}
	}
	if(data == NULL)
		return psd_false;

	if(layer->adjustment_valid == psd_true)
	{
		for(i = 0; i < 6; i ++)
		{
			if(data->setting_values[i][0] != 0 || data->setting_values[i][1] != 0 ||
				data->setting_values[i][2] != 0)
			{
				if(data->range_values[i][0] > data->range_values[i][1] ||
					data->range_values[i][1] > data->range_values[i][2] ||
					data->range_values[i][2] > data->range_values[i][3])
				{
					increase = 0;
					range_value[0] = data->range_values[i][0];
					if(data->range_values[i][0] > data->range_values[i][1])
						increase = 360;
					range_value[1] = data->range_values[i][1] + increase;
					if(data->range_values[i][1] > data->range_values[i][2] && increase == 0)
						increase = 360;
					range_value[2] = data->range_values[i][2] + increase;
					if(data->range_values[i][2] > data->range_values[i][3] && increase == 0)
						increase = 360;
					range_value[3] = data->range_values[i][3] + increase;
					
					for(j = 0; j < 360; j ++)
					{
						if(j >= range_value[0] && j < range_value[3])
						{
							if(j >= range_value[1] && j < range_value[2])
								data->lookup_table[i][j] = 255;
							else if(j < range_value[1] && range_value[1] > range_value[0])
								data->lookup_table[i][j] = 255 * (j - range_value[0]) / (range_value[1] - range_value[0]);
							else if(j >= range_value[2] && range_value[3] > range_value[2])
								data->lookup_table[i][j] = 255 * (range_value[3] - j) / (range_value[3] - range_value[2]);
							else
								data->lookup_table[i][j] = 0;
						}
						else if(j + 360 >= range_value[0] && j + 360 < range_value[3])
						{
							if(j + 360 >= range_value[1] && j + 360 < range_value[2])
								data->lookup_table[i][j] = 255;
							else if(j + 360 < range_value[1] && range_value[1] > range_value[0])
								data->lookup_table[i][j] = 255 * (j + 360 - range_value[0]) / (range_value[1] - range_value[0]);
							else if(j + 360 >= range_value[2] && range_value[3] > range_value[2])
								data->lookup_table[i][j] = 255 * (range_value[3] - (j + 360)) / (range_value[3] - range_value[2]);
							else
								data->lookup_table[i][j] = 0;
						}
						else
						{
							data->lookup_table[i][j] = 0;
						}
					}
				}
				else
				{
					for(j = 0; j < 360; j ++)
					{
						if(j >= data->range_values[i][0] && j < data->range_values[i][3])
						{
							if(j >= data->range_values[i][1] && j < data->range_values[i][2])
								data->lookup_table[i][j] = 255;
							else if(j < data->range_values[i][1] && data->range_values[i][1] > data->range_values[i][0])
								data->lookup_table[i][j] = 255 * (j - data->range_values[i][0]) / (data->range_values[i][1] - data->range_values[i][0]);
							else if(j >= data->range_values[i][2] && data->range_values[i][3] > data->range_values[i][2])
								data->lookup_table[i][j] = 255 * (data->range_values[i][3] - j) / (data->range_values[i][3] - data->range_values[i][2]);
							else
								data->lookup_table[i][j] = 0;
						}
						else
						{
							data->lookup_table[i][j] = 0;
						}
					}
				}
			}
			else
			{
				memset(data->lookup_table[i], 0, 360);
			}
		}
	}

	psd_adjustment_blend_color(context, layer, dst_rect, psd_hue_saturation_proc, layer_info_data);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

