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
 * $Id: color_mode.c, created by Patrick in 2006.05.18, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"


// The color mode data section
psd_status psd_get_color_mode_data(psd_context * context)
{
	psd_uchar * buffer, * r, * g, * b;
	psd_argb_color * color;
	psd_int i;

	// The length of the following color data
	// Only indexed color and duotone (see the mode field in Table 1.2) have color mode data
	context->color_map_length = psd_stream_get_int(context);
	if(context->color_map_length > 0)
	{
		context->color_map_length /= 3;
		context->color_map = (psd_argb_color *)psd_malloc(context->color_map_length * sizeof(psd_argb_color));
		buffer = (psd_uchar *)psd_malloc(context->color_map_length * 3);
		if(context->color_map == NULL || buffer == NULL)
		{
			psd_free(context->color_map);
			context->color_map = NULL;
			psd_free(buffer);
			return psd_status_malloc_failed;
		}
		psd_color_memset(context->color_map, psd_argb_to_color(255, 254, 254, 254), context->color_map_length);

		psd_stream_get(context, buffer, context->color_map_length * 3);
		r = buffer;
		g = r + context->color_map_length;
		b = g + context->color_map_length;
		color = context->color_map;
		for(i = context->color_map_length; i--; )
		{
			*color = PSD_RGB_TO_COLOR(*r, *g, *b);
			r ++;
			g ++;
			b ++;
			color ++;
		}

		psd_free(buffer);
	}

	return psd_status_done;
}

void psd_color_mode_data_free(psd_context * context)
{
	psd_freeif(context->color_map);
}

