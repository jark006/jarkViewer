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
 * $Id: brightness_contrast.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_rect.h"
#include "psd_math.h"


extern void psd_adjustment_blend_image(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect,
	psd_uchar * lookup_table);


// Brightness and Contrast
psd_status psd_get_layer_brightness_contrast(psd_context * context, psd_layer_record * layer)
{
	psd_layer_brightness_contrast * data;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_brightness_contrast;
	layer->layer_type = psd_layer_type_brightness_contrast;

	data = (psd_layer_brightness_contrast *)psd_malloc(sizeof(psd_layer_brightness_contrast));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_brightness_contrast));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Brightness
	data->brightness = psd_stream_get_short(context);
	// Contrast
	data->contrast = psd_stream_get_short(context);
	// Mean value for brightness and contrast
	data->mean_value = psd_stream_get_short(context);
	// Lab color only
	data->Lab_color = psd_stream_get_char(context);

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

psd_bool psd_layer_blend_brightness_contrast(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_layer_brightness_contrast * data = NULL;
	psd_int i;
	psd_float brightness, contrast, slant, value;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_brightness_contrast)
		{
			data = (psd_layer_brightness_contrast *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
		return psd_false;
	
	if(layer->adjustment_valid == psd_true)
	{
		if(data->brightness != 0 && data->contrast != 0)
		{
			brightness = (psd_float)data->brightness / data->mean_value;
			contrast = (psd_float)data->contrast / data->mean_value;
			slant = (psd_float)tan((contrast + 1) * PSD_PI_4);

			for(i = 0; i < 256; i ++)
			{
				value = i / 255.0f;
				if(brightness < 0.0)
					value = value * (1.0f + brightness);
				else if(brightness > 0.0)
					value = value + ((1.0f - value) * brightness);

				if(contrast != 0.0)
					value = (value - 0.5f) * slant + 0.5f;
				value = 255 * value + 0.5f;
				value = PSD_CONSTRAIN(value, 0, 255);
				data->lookup_table[i] = (psd_int)value;
			}
		}
		else
		{
			for(i = 0; i < 256; i ++)
				data->lookup_table[i] = i;
		}
	}

	psd_adjustment_blend_image(context, layer, dst_rect, data->lookup_table);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

