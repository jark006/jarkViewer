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
 * $Id: solid_color.c, created by Patrick in 2006.05.24, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_descriptor.h"
#include "psd_color.h"


// Solid color sheet setting (Photoshop 6.0)
psd_status psd_get_layer_solid_color(psd_context * context, psd_layer_record * layer)
{
	psd_layer_solid_color * data;
	psd_int length, number_items;
	psd_uint key;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_solid_color;
	layer->layer_type = psd_layer_type_solid_color;

	data = (psd_layer_solid_color *)psd_malloc(sizeof(psd_layer_solid_color));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_solid_color));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Version ( = 16 for Photoshop 6.0)
	if(psd_stream_get_int(context) != 16)
		return psd_status_solid_color_unsupport_version;

	// Unicode string: name from classID
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);

	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	data->id = psd_stream_get_int(context);

	// Number of items in descriptor
	number_items = psd_stream_get_int(context);
	// should be 1
	psd_assert(number_items == 1);

	// The following is repeated for each item in descriptor
	// Key: 4 bytes ( length) followed either by string or (if length is zero) 4-byte key
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	psd_assert(key == 'Clr ');

	// Type: OSType key
	key = psd_stream_get_int(context);
	psd_assert(key == 'Objc');

	data->fill_color = psd_stream_get_object_color(context);

	return psd_status_done;
}
