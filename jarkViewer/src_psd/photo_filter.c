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
 * $Id: photo_filter.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"


psd_status psd_get_layer_photo_filter(psd_context * context, psd_layer_record * layer)
{
	psd_layer_photo_filter * data;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_photo_filter;
	layer->layer_type = psd_layer_type_photo_filter;

	data = (psd_layer_photo_filter *)psd_malloc(sizeof(psd_layer_photo_filter));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_photo_filter));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Version ( = 3)
	if(psd_stream_get_short(context) != 3)
		return psd_status_photo_filter_unsupport_version;

	// 4 bytes each for XYZ color
	data->x_color = psd_stream_get_int(context) >> 8;
	data->y_color = psd_stream_get_int(context) >> 8;
	data->z_color = psd_stream_get_int(context) >> 8;

	// Density
	data->density = psd_stream_get_int(context);

	// Preserve Luminosity
	data->preserve_luminosity = psd_stream_get_bool(context);

	// for padding
	psd_stream_get_char(context);

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

psd_bool psd_layer_blend_photo_filter(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	return psd_false;
}

