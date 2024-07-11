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
 * $Id: path.c, created by Patrick in 2007.02.06, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_fixed.h"
#include "psd_math.h"


#define PSD_MIN_PATH_COUNT		4
#define PSD_MIN_SUBPATH_COUNT	4


#ifdef PSD_GET_PATH_RESOURCE

static psd_status psd_get_path_record(psd_context * context, psd_path * path, psd_int length)
{
	psd_int i, records, malloc_subpath;
	psd_short record_type;
	psd_subpath * subpaths = NULL, * cur_subpath;
	psd_bezier_point * cur_bezier_point = NULL;
	
	memset(path, 0, sizeof(psd_path));

	malloc_subpath = PSD_MIN_SUBPATH_COUNT;
	subpaths = (psd_subpath *)psd_malloc(sizeof(psd_subpath) * malloc_subpath);
	if (subpaths == NULL)
		return psd_status_malloc_failed;

	// These resource blocks consist of a series of 26-byte path point records, so the resource
	// length should always be a multiple of 26.
	records = length / 26;

	for (i = 0; i < records; i ++)
	{
		record_type = psd_stream_get_short(context);
		switch(record_type)
		{
			case 0:		// Closed subpath length record
			case 3:		// Open subpath length record
				if (path->number_of_subpaths >= malloc_subpath)
				{
					malloc_subpath *= 2;
					subpaths = (psd_subpath *)psd_realloc(subpaths, sizeof(psd_subpath) * malloc_subpath);
					if (subpaths == NULL)
						return psd_status_malloc_failed;
				}
				cur_subpath = &subpaths[path->number_of_subpaths];
				path->number_of_subpaths ++;
				// contain the number of Bezier knot records in bytes 2 and 3
				cur_subpath->number_of_points = psd_stream_get_short(context);
				cur_subpath->closed = (record_type == 0 ? psd_true : psd_false);
				cur_subpath->bezier_points = (psd_bezier_point *)psd_malloc(sizeof(psd_bezier_point) * cur_subpath->number_of_points);
				if (cur_subpath->bezier_points == NULL)
					return psd_status_malloc_failed;
				memset(cur_subpath->bezier_points, 0, sizeof(psd_bezier_point) * cur_subpath->number_of_points);
				cur_bezier_point = cur_subpath->bezier_points;
				// remains 22 byets
				psd_stream_get_null(context, 22);
				break;
				
			case 1:		// Closed subpath Bezier knot, linked
			case 2:		// Closed subpath Bezier knot, unlinked
			case 4:		// Open subpath Bezier knot, linked
			case 5:		// Open subpath Bezier knot, unlinked
				if (record_type == 1 || record_type == 4)
					cur_bezier_point->linked = psd_true;
				else
					cur_bezier_point->linked = psd_false;
				// the control point for the Bezier segment preceding the knot
				cur_bezier_point->preceding_control_vertical = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				cur_bezier_point->preceding_control_horizontal = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				// the anchor point for the knot
				cur_bezier_point->anchor_point_vertical = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				cur_bezier_point->anchor_point_horizontal = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				// the control point for the Bezier segment leaving the knot
				cur_bezier_point->leaving_control_vertical = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				cur_bezier_point->leaving_control_horizontal = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				// get the next
				cur_bezier_point ++;
				break;
				
			case 6:		// Path fill rule record
				// The remaining 24 bytes of the first record are zeroes.
				psd_stream_get_null(context, 24);
				break;
				
			case 7:		// Clipboard record
				// contain four fixed-point numbers for the bounding rectangle (top, left, bottom, right)
				path->clipboard_top = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				path->clipboard_left = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				path->clipboard_bottom = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				path->clipboard_right = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				// a single fixed-point number indicating the resolution
				path->resolution = psd_fixed_8_24_tofloat(psd_stream_get_int(context));
				// remains 4 byets
				psd_stream_get_null(context, 4);
				break;
				
			case 8:		// Initial fill rule record
				// contain one two byte record. A value of 1 means that 
				// the fill starts with all pixels.
				path->initial_fill = (psd_bool)psd_stream_get_short(context);
				// remains 22 byets
				psd_stream_get_null(context, 22);
				break;
				
			default:
				psd_stream_get_null(context, 24);
				psd_assert(0);
				break;
		}
	}

	path->subpaths = subpaths;
							
	return psd_status_done;
}

static void psd_path_record_free(psd_path * path)
{
	psd_int i;

	if (path == NULL)
		return;
	
	for (i = 0; i < path->number_of_subpaths; i ++)
		psd_freeif(path->subpaths[i].bezier_points);
	
	psd_freeif(path->subpaths);
}

psd_status psd_get_path(psd_context * context, psd_int length)
{
	psd_status status;
	
	if(context->path_count >= context->malloc_path)
	{
		context->malloc_path = PSD_MAX(context->malloc_path * 2, PSD_MIN_PATH_COUNT);
		context->paths = (psd_path *)psd_realloc(context->paths, context->malloc_path * sizeof(psd_path));
		if(context->paths == NULL)
			return psd_status_malloc_failed;
	}

	status = psd_get_path_record(context, &context->paths[context->path_count], length);
	if(status != psd_status_done)
		return status;
	
	context->path_count ++;
	
	return psd_status_done;
}

void psd_path_free(psd_context * context)
{
	psd_int i;

	if (context->paths == NULL)
		return;

	for (i = 0; i < context->path_count; i ++)
		psd_path_record_free(&context->paths[i]);
	
	psd_freeif(context->paths);
	context->paths = NULL;
}

psd_status psd_get_layer_vector_mask(psd_context * context, psd_layer_record * layer, psd_int size)
{
	psd_uint tag;
	
	// Version ( = 3 for Photoshop 6.0)
	psd_assert(psd_stream_get_int(context) == 3);

	// Flags. bit 1 = invert, bit 2 = not link, bit 3 = disable
	tag = psd_stream_get_int(context);
	layer->vector_mask.invert = tag & 0x01;
	layer->vector_mask.not_link = (tag & (0x01 << 1)) > 0;
	layer->vector_mask.disable = (tag & (0x01 << 2)) > 0;

	// The rest of the data is path components, loop until end of the length
	layer->vector_mask.path = (psd_path *)psd_malloc(sizeof(psd_path));
	if(layer->vector_mask.path == NULL)
		return psd_status_malloc_failed;
	psd_get_path_record(context, layer->vector_mask.path, size - 8);

	return psd_status_done;
}

void psd_layer_vector_mask_free(psd_layer_record * layer)
{
	psd_path_record_free(layer->vector_mask.path);
	psd_freeif(layer->vector_mask.path);
	layer->vector_mask.path = NULL;
}

#endif // ifdef PSD_GET_PATH_RESOURCE
