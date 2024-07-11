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
 * $Id: color_balance.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_math.h"


#define PSD_SQR(x)			((x) * (x))

static psd_bool transfer_initialized = psd_false;

/*  for lightening  */
static psd_float  highlights_add[256] = { 0 };
static psd_float  midtones_add[256]   = { 0 };
static psd_float  shadows_add[256]    = { 0 };

/*  for darkening  */
static psd_float  highlights_sub[256] = { 0 };
static psd_float  midtones_sub[256]   = { 0 };
static psd_float  shadows_sub[256]    = { 0 };

extern void psd_adjustment_blend_rgb(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_uchar * red_lookup_table, psd_uchar * green_lookup_table, psd_uchar * blue_lookup_table,
	psd_bool preserve_luminosity);


psd_status psd_get_layer_color_balance(psd_context * context, psd_layer_record * layer)
{
	psd_layer_color_balance * data;
	psd_int i;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_color_balance;
	layer->layer_type = psd_layer_type_color_balance;

	data = (psd_layer_color_balance *)psd_malloc(sizeof(psd_layer_color_balance));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_color_balance));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// shadows, midtones, highlights
	for(i = 0; i < 3; i ++)
	{
		data->cyan_red[i] = psd_stream_get_short(context);
		data->magenta_green[i] = psd_stream_get_short(context);
		data->yellow_blue[i] = psd_stream_get_short(context);
		// (-100...100). 
		psd_assert(data->cyan_red[i] >= -100 && data->cyan_red[i] <= 100);
		psd_assert(data->magenta_green[i] >= -100 && data->cyan_red[i] <= 100);
		psd_assert(data->yellow_blue[i] >= -100 && data->cyan_red[i] <= 100);
	}

	// Preserve luminosity
	data->preserve_luminosity = (psd_bool)psd_stream_get_short(context);
	psd_assert(data->preserve_luminosity == psd_true || data->preserve_luminosity == psd_false);

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

static void psd_color_balance_transfer_init(void)
{
	psd_int i;

	for (i = 0; i < 256; i++)
	{
		highlights_add[i] = shadows_sub[255 - i] = 
			(1.075f - 1 / ( i / 16.0f + 1));
		midtones_add[i] = midtones_sub[i] = 
			0.667f * (1 - PSD_SQR((i - 127.0f) / 127.0f));
		shadows_add[i] = highlights_sub[i] = 
			0.667f * (1 - PSD_SQR((i - 127.0f) / 127.0f));
		//midtones_sub[i] = 0.64f * (1 - PSD_SQR((i - 127.0f) / 127.0f));
	}
}

static void psd_color_balance_calculate_table(psd_layer_color_balance * data,
	psd_uchar * red_lookup_table, psd_uchar * green_lookup_table, psd_uchar * blue_lookup_table)
{
	psd_float *cyan_red_transfer[3];
	psd_float *magenta_green_transfer[3];
	psd_float *yellow_blue_transfer[3];
	psd_int i, value;

	cyan_red_transfer[0] = (data->cyan_red[0] > 0) ? shadows_add : shadows_sub;
	cyan_red_transfer[1] = (data->cyan_red[1] > 0) ? midtones_add : midtones_sub;
	cyan_red_transfer[2] = (data->cyan_red[2] > 0) ? highlights_add : highlights_sub;
	magenta_green_transfer[0] = (data->magenta_green[0] > 0) ? shadows_add : shadows_sub;
	magenta_green_transfer[1] = (data->magenta_green[1] > 0) ? midtones_add : midtones_sub;
	magenta_green_transfer[2] = (data->magenta_green[2] > 0) ? highlights_add : highlights_sub;
	yellow_blue_transfer[0] = (data->yellow_blue[0] > 0) ? shadows_add : shadows_sub;
	yellow_blue_transfer[1] = (data->yellow_blue[1] > 0) ? midtones_add : midtones_sub;
	yellow_blue_transfer[2] = (data->yellow_blue[2] > 0) ? highlights_add : highlights_sub;

	for(i = 0; i < 256; i ++)
	{
		value = i;
		value += (psd_int)(data->cyan_red[0] * cyan_red_transfer[0][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		value += (psd_int)(data->cyan_red[1] * cyan_red_transfer[1][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		value += (psd_int)(data->cyan_red[2] * cyan_red_transfer[2][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		red_lookup_table[i] = value;

		value = i;
		value += (psd_int)(data->magenta_green[0] * magenta_green_transfer[0][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		value += (psd_int)(data->magenta_green[1] * magenta_green_transfer[1][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		value += (psd_int)(data->magenta_green[2] * magenta_green_transfer[2][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		green_lookup_table[i] = value;

		value = i;
		value += (psd_int)(data->yellow_blue[0] * yellow_blue_transfer[0][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		value += (psd_int)(data->yellow_blue[1] * yellow_blue_transfer[1][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		value += (psd_int)(data->yellow_blue[2] * yellow_blue_transfer[2][value]);
		value = PSD_CONSTRAIN(value, 0, 255);
		blue_lookup_table[i] = value;
	}
}

psd_bool psd_layer_blend_color_balance(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_layer_color_balance * data = NULL;
	psd_int i;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_color_balance)
		{
			data = (psd_layer_color_balance *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
		return psd_false;

	if(layer->adjustment_valid == psd_true)
	{
		if(transfer_initialized == psd_false)
		{
			psd_color_balance_transfer_init();
			transfer_initialized = psd_true;
		}
		psd_color_balance_calculate_table(data, data->lookup_table[0], 
			data->lookup_table[1], data->lookup_table[2]);
	}

	psd_adjustment_blend_rgb(context, layer, dst_rect, data->lookup_table[0], 
		data->lookup_table[1], data->lookup_table[2], data->preserve_luminosity);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

