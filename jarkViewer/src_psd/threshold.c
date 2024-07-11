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
 * $Id: threshold.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"


extern void psd_adjustment_blend_gray(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect,
	psd_uchar * lookup_table);


psd_status psd_get_layer_threshold(psd_context * context, psd_layer_record * layer)
{
	psd_layer_threshold * data;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_threshold;
	layer->layer_type = psd_layer_type_threshold;

	data = (psd_layer_threshold *)psd_malloc(sizeof(psd_layer_threshold));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_threshold));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// (1...255)
	data->level = psd_stream_get_short(context);
	psd_assert(data->level >= 1 && data->level <= 255);

	// for padding ?
	psd_stream_get_short(context);

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

psd_bool psd_layer_blend_threshold(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_uint layer_info_data;
	psd_layer_threshold * data = NULL;
	psd_int i;
	psd_uchar lookup_table[256];

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_threshold)
		{
			layer_info_data = layer->layer_info_data[i];
			data = (psd_layer_threshold *)layer_info_data;
			break;
		}
	}
	if(data == NULL)
		return psd_false;

	memset(lookup_table, 0, data->level);
	memset(lookup_table + data->level, 255, 256 - data->level);

	psd_adjustment_blend_gray(context, layer, dst_rect, lookup_table);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

