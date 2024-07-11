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
 * $Id: type_tool.c, created by Patrick in 2006.05.30, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"


// Type Tool Info (Photoshop 5.0 and 5.5 only)
psd_status psd_get_layer_type_tool(psd_context * context, psd_layer_record * layer)
{
	psd_layer_type_tool * data;
	psd_int i, j, length;
	
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_type_tool;

	data = (psd_layer_type_tool *)psd_malloc(sizeof(psd_layer_type_tool));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_type_tool));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// Version ( = 1)
	psd_assert(psd_stream_get_short(context) == 1);

	// 6 * 8 double precision numbers for the transform information
	for(i = 0; i < 6; i ++)
		data->transform_info[i] = psd_stream_get_double(context);

	/***********************************************************************/
	// Font information
	/***********************************************************************/
	// Version ( = 6)
	psd_assert(psd_stream_get_short(context) == 6);

	// Count of faces
	data->faces_count = psd_stream_get_short(context);
	data->face = (psd_layer_type_face *)psd_malloc(data->faces_count * sizeof(psd_layer_type_face));
	if(data->face == NULL)
		return psd_status_malloc_failed;
	memset(data->face, 0, data->faces_count * sizeof(psd_layer_type_face));

	// The next 8 fields are repeated for each count specified above
	for(i = 0; i < data->faces_count; i ++)
	{
		// Mark value
		data->face[i].mark = psd_stream_get_short(context);

		// Font type data
		data->face[i].font_type = psd_stream_get_int(context);

		// Pascal string of font name
		length = psd_stream_get_char(context);
		psd_stream_get(context, data->face[i].font_name, length);

		// Pascal string of font family name
		length = psd_stream_get_char(context);
		psd_stream_get(context, data->face[i].font_family_name, length);

		// Pascal string of font style name
		length = psd_stream_get_char(context);
		psd_stream_get(context, data->face[i].font_style_name, length);

		// Script value
		data->face[i].script = psd_stream_get_short(context);

		// Number of design axes vector to follow
		data->face[i].number_axes_vector = psd_stream_get_int(context);
		data->face[i].vector = (psd_int *)psd_malloc(data->face[i].number_axes_vector * 4);
		if(data->face[i].vector == NULL)
			return psd_status_malloc_failed;

		// Design vector value
		for(j = 0; j < data->face[i].number_axes_vector; j ++)
			data->face[i].vector[j] = psd_stream_get_int(context);
	}

	/***********************************************************************/
	// Style information
	/***********************************************************************/
	// Count of styles
	data->styles_count = psd_stream_get_short(context);
	data->style = (psd_layer_type_style *)psd_malloc(data->styles_count * sizeof(psd_layer_type_style));
	if(data->style == NULL)
		return psd_status_malloc_failed;
	memset(data->style, 0, data->styles_count * sizeof(psd_layer_type_style));

	// The next 10 fields are repeated for each count specified above
	for(i = 0; i < data->styles_count; i ++)
	{
		// Mark value
		data->style[i].mark = psd_stream_get_short(context);

		// Face mark value
		data->style[i].face_mark = psd_stream_get_short(context);

		// Size value
		data->style[i].size = psd_stream_get_int(context);

		// Tracking value
		data->style[i].tracking = psd_stream_get_int(context);

		// Kerning value
		data->style[i].kerning = psd_stream_get_int(context);

		// Leading value
		data->style[i].leading = psd_stream_get_int(context);

		// Base shift value
		data->style[i].base_shift = psd_stream_get_int(context);

		// Auto kern on/off
		data->style[i].auto_kern = psd_stream_get_bool(context);

		// Only present in version <= 5
		psd_stream_get_char(context);

		// Rotate up/down
		data->style[i].rotate = psd_stream_get_bool(context);
	}

	/***********************************************************************/
	// Text information
	/***********************************************************************/
	// Type value
	data->type = psd_stream_get_short(context);

	// Scaling factor value
	data->scaling_factor = psd_stream_get_int(context);

	// Sharacter count value
	data->sharacter_count = psd_stream_get_int(context);

	// Horizontal placement
	data->horz_place = psd_stream_get_int(context);

	// Vertical placement
	data->vert_place = psd_stream_get_int(context);

	// Select start value
	data->select_start = psd_stream_get_int(context);

	// Select end value
	data->select_end = psd_stream_get_int(context);

	// Line count
	data->lines_count = psd_stream_get_short(context);
	data->line = (psd_layer_type_line *)psd_malloc(data->lines_count * sizeof(psd_layer_type_line));
	if(data->line == NULL)
		return psd_status_malloc_failed;
	memset(data->line, 0, data->lines_count * sizeof(psd_layer_type_line));

	// The next 5 fields are repeated for each item in line count.
	for(i = 0; i < data->lines_count; i ++)
	{
		// Character count value
		data->line[i].char_count = psd_stream_get_int(context);

		// Orientation value
		data->line[i].orientation = psd_stream_get_short(context);

		// Alignment value
		data->line[i].alignment = psd_stream_get_short(context);

		// Actual character as a double byte character
		data->line[i].actual_char = psd_stream_get_short(context);

		// Style value
		data->line[i].style = psd_stream_get_short(context);
	}

	/***********************************************************************/
	// Color information
	/***********************************************************************/
	// Color space value
	data->color = psd_stream_get_space_color(context);

	// Anti alias on/off
	data->anti_alias = psd_stream_get_bool(context);
	
	return psd_status_done;
}

void psd_layer_type_tool_free(psd_uint info_data)
{
	psd_layer_type_tool * data;
	psd_int i;

	data = (psd_layer_type_tool *)info_data;
	for(i = 0; i < data->faces_count; i ++)
		psd_free(data->face[i].vector);
	psd_free(data->face);
	psd_free(data->style);
	psd_free(data->line);
	psd_free(data);
}

