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
 * $Id: levelsl.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_math.h"


extern void psd_adjustment_blend_rgb(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_uchar * red_lookup_table, psd_uchar * green_lookup_table, psd_uchar * blue_lookup_table,
	psd_bool preserve_luminosity);


// Additional layer -- levels
// Levels settings files are loaded and saved in the Levels dialog.
psd_status psd_get_layer_levels(psd_context * context, psd_layer_record * layer, psd_int data_length)
{
	psd_layer_levels * data;
	psd_int i, prev_stream_pos = context->stream.current_pos;
	psd_uint tag;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_levels;
	layer->layer_type = psd_layer_type_levels;

	data = (psd_layer_levels *)psd_malloc(sizeof(psd_layer_levels));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_levels));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;
	
	// Version ( = 2)
	if(psd_stream_get_short(context) != 2)
		return psd_status_levels_unsupport_version;

	// 29 sets of level records, each level containing 5 psd_short integers
	for(i = 0; i < 29; i ++)
	{
		// Input floor (0...253)
		data->record[i].input_floor = psd_stream_get_short(context);
		// Input ceiling (2...255)
		data->record[i].input_ceiling = psd_stream_get_short(context);
		// Output floor (0...255). Matched to input floor.
		data->record[i].output_floor = psd_stream_get_short(context);
		// Output ceiling (0...255)
		data->record[i].output_ceiling = psd_stream_get_short(context);
		// Gamma. Short integer from 10...999 representing 0.1...9.99. Applied
		// to all image data.
		data->record[i].gamma = psd_stream_get_short(context) / 100.0f;
		// Sets 28 and 29 are reserved and should be set to zeros.
		if(i < 27)
		{
			psd_assert(data->record[i].input_floor >= 0 && data->record[i].input_floor <= 255);
			psd_assert(data->record[i].input_ceiling >= 2 && data->record[i].input_ceiling <= 255);
			psd_assert(data->record[i].output_floor >= 0 && data->record[i].output_floor <= 255);
			psd_assert(data->record[i].output_ceiling >= 0 && data->record[i].output_ceiling <= 255);
			psd_assert(data->record[i].gamma >= 0.1 && data->record[i].gamma <= 9.99);
		}
	}

	// Photoshop CS (8.0) Additional information
	// At the end of the Version 2 file is the following information
	if(context->stream.current_pos - prev_stream_pos < data_length - 4)
	{
		// = 'Lvls' for extra level information
		tag = psd_stream_get_int(context);
		if(tag != 'Lvls')
			return psd_status_extra_levels_key_error;

		// Version ( = 3)
		if(psd_stream_get_short(context) != 3)
			return psd_status_extra_levels_unsupport_version;

		// Count of total level record structures. Subtract the legacy number of
		// level record structures, 29, to determine how many are remaining in
		// the file for reading.
		data->extra_level_count = psd_stream_get_short(context) - 29;
		psd_assert(data->extra_level_count >= 0);
		data->extra_record = (psd_layer_level_record *)psd_malloc(data->extra_level_count * sizeof(psd_layer_level_record));
		if(data->extra_record == NULL)
			return psd_status_malloc_failed;
		memset(data->extra_record, 0, data->extra_level_count * sizeof(psd_layer_level_record));

		// Additianol level records according to count.
		for(i = 0; i < data->extra_level_count; i ++)
		{
			// Input floor (0...253)
			data->extra_record[i].input_floor = psd_stream_get_short(context);
			// Input ceiling (2...255)
			data->extra_record[i].input_ceiling = psd_stream_get_short(context);
			// Output floor (0...255). Matched to input floor.
			data->extra_record[i].output_floor = psd_stream_get_short(context);
			// Output ceiling (0...255)
			data->extra_record[i].output_ceiling = psd_stream_get_short(context);
			// Gamma. Short integer from 10...999 representing 0.1...9.99. Applied
			// to all image data.
			data->extra_record[i].gamma = psd_stream_get_short(context) / 100.0f;
			// Sets 28 and 29 are reserved and should be set to zeros.
		}
	}

	if(context->stream.current_pos - prev_stream_pos != data_length)
		return psd_status_levels_dismatch_data_length;

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

void psd_layer_levels_free(psd_uint info_data)
{
	psd_layer_levels * data;

	data = (psd_layer_levels *)info_data;
	psd_freeif(data->extra_record);
	psd_free(data);
}

psd_bool psd_layer_blend_levels(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_int i, j, range, floor, value;
	psd_float gamma;
	psd_layer_levels * data = NULL;
	psd_uchar input_table[256], output_table[256];

	if(context->color_mode != psd_color_mode_rgb && context->color_mode != psd_color_mode_grayscale)
		return psd_false;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_levels)
		{
			data = (psd_layer_levels *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
		return psd_false;

	if(layer->adjustment_valid == psd_true)
	{
		for(i = 0; i < 3; i ++)
		{
			for(j = 0; j < 256; j ++)
				data->lookup_table[i][j] = j;
		}
		for(i = 0; i < 4; i ++)
		{
			if(data->record[i].input_floor != 0 || data->record[i].input_ceiling != 255 ||
				data->record[i].output_floor != 0 || data->record[i].output_ceiling != 255 ||
				data->record[i].gamma != 1.0)
			{
				if(layer->adjustment_valid == psd_true)
				{
					if(data->record[i].input_floor != 0 || data->record[i].input_ceiling != 255 ||
						data->record[i].gamma != 1.0)
					{
						range = data->record[i].input_ceiling - data->record[i].input_floor;
						floor = data->record[i].input_floor;
						if(data->record[i].gamma != 1.0)
						{
							gamma = 1 / data->record[i].gamma;
							for(j = 0; j < 256; j ++)
							{
								value = (psd_int)(pow((psd_float)(j - floor) / range, gamma) * 255 + 0.5);
								input_table[j] = PSD_CONSTRAIN(value, 0, 255);
							}
						}
						else
						{
							for(j = 0; j < 256; j ++)
							{
								value = (psd_int)((j - floor) * 255.0 / range);
								input_table[j] = PSD_CONSTRAIN(value, 0, 255);
							}
						}
					}
					else
					{
						for(j = 0; j < 256; j ++)
							input_table[j] = j;
					}

					if(data->record[i].output_floor != 0 || data->record[i].output_ceiling != 255)
					{
						range = data->record[i].output_ceiling - data->record[i].output_floor;
						floor = data->record[i].output_floor;
						for(j = 0; j < 256; j ++)
						{
							output_table[j] = (j * range + 128) / 255 + floor;
						}
					}
					else
					{
						for(j = 0; j < 256; j ++)
							output_table[j] = j;
					}

					if(i == 0)
					{
						for(j = 0; j < 256; j ++)
						{
							data->lookup_table[0][j] = data->lookup_table[1][j] = 
								data->lookup_table[2][j] = output_table[input_table[j]];
						}
					}
					else
					{
						for(j = 0; j < 256; j ++)
							data->lookup_table[i - 1][j] = output_table[input_table[data->lookup_table[i - 1][j]]];
					}
				}
			}
			
			if(context->color_mode == psd_color_mode_grayscale)
				break;
		}
	}
	
	psd_adjustment_blend_rgb(context, layer, dst_rect, data->lookup_table[0],
		data->lookup_table[1], data->lookup_table[2], psd_false);

	layer->adjustment_valid = psd_false;

	return psd_true;
}

