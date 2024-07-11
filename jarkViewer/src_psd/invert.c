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
 * $Id: invert.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_blend.h"


extern void psd_adjustment_blend_image(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect,
	psd_uchar * lookup_table);


psd_status psd_get_layer_invert(psd_context * context, psd_layer_record * layer)
{
	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_invert;
	layer->layer_type = psd_layer_type_invert;
	layer->layer_info_count ++;
	// there is no parameter in invert layer.

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

psd_bool psd_layer_blend_invert(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_int i;
	psd_uchar lookup_table[256];

	for(i = 0; i < 256; i ++)
		lookup_table[i] = 255 - i;

	psd_adjustment_blend_image(context, layer, dst_rect, lookup_table);

	layer->adjustment_valid = psd_false;
	
	return psd_true;
}

