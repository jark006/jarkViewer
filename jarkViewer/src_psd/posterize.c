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
 * $Id: posterize.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_rect.h"
#include "psd_math.h"


extern void psd_adjustment_blend_image(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect,
	psd_uchar * lookup_table);


psd_status psd_get_layer_posterize(psd_context * context, psd_layer_record * layer)
{
	psd_layer_posterize * data;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_posterize;
	layer->layer_type = psd_layer_type_posterize;

	data = (psd_layer_posterize *)psd_malloc(sizeof(psd_layer_posterize));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_posterize));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// (2...255)
	data->levels = psd_stream_get_short(context);
	psd_assert(data->levels >= 2 && data->levels <= 255);

	// for padding ?
	psd_stream_get_short(context);

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

psd_bool psd_layer_blend_posterize(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_layer_posterize * data = NULL;
	psd_int i, value;
	psd_float reciprocal;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_posterize)
		{
			data = (psd_layer_posterize *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
		return psd_false;
	
	if(layer->adjustment_valid == psd_true)
	{
		reciprocal = 255.0f / data->levels;
		for(i = 0; i < 256; i ++)
		{
			value = (psd_int)((psd_int)(i / reciprocal + 0.5) * reciprocal + 0.5);
			data->lookup_table[i] = PSD_CONSTRAIN(value, 0, 255);
		}
	}

	psd_adjustment_blend_image(context, layer, dst_rect, data->lookup_table);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

