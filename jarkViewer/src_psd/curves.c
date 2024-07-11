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
 * $Id: curves.c, created by Patrick in 2006.05.22, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_math.h"


typedef psd_double psd_matrix[4][4];

static psd_matrix psd_basis_matrix =
{
  { -0.5,  1.5, -1.5,  0.5 },
  {  1.0, -2.5,  2.0, -0.5 },
  { -0.5,  0.0,  0.5,  0.0 },
  {  0.0,  1.0,  0.0,  0.0 },
};

/*  this can be adjusted to give a finer or coarser curve  */
#define PSD_CURVES_SUBDIVIDE  512

extern void psd_adjustment_blend_rgb(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect, 
	psd_uchar * red_lookup_table, psd_uchar * green_lookup_table, psd_uchar * blue_lookup_table,
	psd_bool preserve_luminosity);


// Additional layer -- curves
psd_status psd_get_layer_curves(psd_context * context, psd_layer_record * layer, psd_int data_length)
{
	psd_layer_curves * data;
	psd_int i, j, count, version, prev_stream_pos = context->stream.current_pos;
	psd_uint tag;
	psd_int curve_count, point_count, output_value, input_value;

	layer->layer_info_type[layer->layer_info_count] = psd_layer_info_type_curves;
	layer->layer_type = psd_layer_type_curves;

	data = (psd_layer_curves *)psd_malloc(sizeof(psd_layer_curves));
	if(data == NULL)
		return psd_status_malloc_failed;
	memset(data, 0, sizeof(psd_layer_curves));
	layer->layer_info_data[layer->layer_info_count] = (psd_uint)data;
	layer->layer_info_count ++;

	// padding, document is wrong, maybe photoshop is wrong
	psd_stream_get_char(context);

	// Version ( = 1 or = 4)
	version = psd_stream_get_short(context);
	if(version != 1 && version != 4)
		return psd_status_curves_unsupport_version;

	// curve tag
	tag = psd_stream_get_int(context);
	data->curve_count = 0;
	for(i = 0; i < 32; i ++)
	{
		// Count of curves in the file
		if(tag & (1 << i))
			data->curve_count ++;
	}
	
	data->curve = (psd_layer_curves_data *)psd_malloc(data->curve_count * sizeof(psd_layer_curves_data));
	if(data->curve == NULL)
		return psd_status_malloc_failed;
	memset(data->curve, 0, data->curve_count * sizeof(psd_layer_curves_data));

	// The following is the data for each curve specified by count above
	for(i = 0; i < data->curve_count; i ++)
	{
		// Before each curve is a channel index.
		for(j = 0, count = 0; j < 32; j ++)
		{
			if(tag & (1 << j))
			{
				if(count == i)
				{
					data->curve[i].channel_index = j;
					break;
				}
				count ++;
			}
		}
		
		// Count of points in the curve (psd_short integer from 2...19)
		data->curve[i].point_count = psd_stream_get_short(context);
		psd_assert(data->curve[i].point_count >= 2 && data->curve[i].point_count <= 19);
		for(j = 0; j < data->curve[i].point_count; j ++)
		{
			// Curve points. Each curve point is a pair of psd_short integers where the
			// first number is the output value (vertical coordinate on the Curves
			// dialog graph) and the second is the input value.
			data->curve[i].output_value[j] = psd_stream_get_short(context);
			data->curve[i].input_value[j] = psd_stream_get_short(context);
			// range 0 to 255
			psd_assert(data->curve[i].output_value[j] >= 0 && data->curve[i].output_value[j] <= 255);
			psd_assert(data->curve[i].input_value[j] >= 0 && data->curve[i].input_value[j] <= 255);
		}
	}

	// Additional information
	// At the end of the Version 1 file is the following information
	if(context->stream.current_pos - prev_stream_pos < data_length - 4)
	{
		// = 'Crv ' for extra curve information
		tag = psd_stream_get_int(context);
		if(tag != 'Crv ')
			return psd_status_extra_curves_key_error;

		// Version ( = 4)
		if(psd_stream_get_short(context) != 4)
			return psd_status_extra_curves_unsupport_version;

		// Count of items to follow.
		curve_count = psd_stream_get_int(context);
		psd_assert(curve_count == data->curve_count);
		if(curve_count != data->curve_count)
			return psd_status_done;
		
		// The following is the data for each curve specified by count above
		for(i = 0; i < data->curve_count; i ++)
		{
			// Before each curve is a channel index.
			data->curve[i].channel_index = psd_stream_get_short(context);
			// Count of points in the curve (psd_short integer from 2...19)
			point_count = psd_stream_get_short(context);
			psd_assert(point_count == data->curve[i].point_count);
			if(point_count != data->curve[i].point_count)
				return psd_status_done;
			
			for(j = 0; j < point_count; j ++)
			{
				// Curve points. Each curve point is a pair of psd_short integers where the
				// first number is the output value (vertical coordinate on the Curves
				// dialog graph) and the second is the input value.
				output_value = psd_stream_get_short(context);
				input_value = psd_stream_get_short(context);
				psd_assert(output_value == data->curve[i].output_value[j] && 
					input_value == data->curve[i].input_value[j]);
				if(output_value != data->curve[i].output_value[j] || 
					input_value != data->curve[i].input_value[j])
					return psd_status_done;
			}
		}
	}

	layer->adjustment_valid = psd_true;

	return psd_status_done;
}

void psd_layer_curves_free(psd_uint info_data)
{
	psd_layer_curves * data;

	data = (psd_layer_curves *)info_data;
	psd_freeif(data->curve);
	psd_free(data);
}

static void psd_curves_compose(psd_matrix a, psd_matrix b, psd_matrix ab)
{
	psd_int i, j;
	
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			ab[i][j] = (a[i][0] * b[0][j] +
				a[i][1] * b[1][j] +
				a[i][2] * b[2][j] +
				a[i][3] * b[3][j]);
		}
	}
}

static void psd_curves_plot(psd_layer_curves_data * curve, psd_uchar * table, psd_int p1, psd_int p2, psd_int p3, psd_int p4)
{
	psd_matrix geometry;
	psd_matrix tmp1, tmp2;
	psd_matrix deltas;
	psd_double  x, dx, dx2, dx3;
	psd_double  y, dy, dy2, dy3;
	psd_double  d, d2, d3;
	psd_int     lastx, lasty;
	psd_int     newx, newy;
	psd_int     i;

	/* construct the geometry matrix from the segment */
	for (i = 0; i < 4; i++)
	{
		geometry[i][2] = 0;
		geometry[i][3] = 0;
	}

	geometry[0][0] = curve->input_value[p1];
	geometry[1][0] = curve->input_value[p2];
	geometry[2][0] = curve->input_value[p3];
	geometry[3][0] = curve->input_value[p4];
	geometry[0][1] = curve->output_value[p1];
	geometry[1][1] = curve->output_value[p2];
	geometry[2][1] = curve->output_value[p3];
	geometry[3][1] = curve->output_value[p4];

	/* subdivide the curve */
	d = 1.0 / PSD_CURVES_SUBDIVIDE;
	d2 = d * d;
	d3 = d * d * d;

	/* construct a temporary matrix for determining the forward
	* differencing deltas
	*/
	tmp2[0][0] = 0;       tmp2[0][1] = 0;       tmp2[0][2] = 0;  tmp2[0][3] = 1;
	tmp2[1][0] = d3;      tmp2[1][1] = d2;      tmp2[1][2] = d;  tmp2[1][3] = 0;
	tmp2[2][0] = 6 * d3;  tmp2[2][1] = 2 * d2;  tmp2[2][2] = 0;  tmp2[2][3] = 0;
	tmp2[3][0] = 6 * d3;  tmp2[3][1] = 0;       tmp2[3][2] = 0;  tmp2[3][3] = 0;

	/* compose the basis and geometry matrices */
	psd_curves_compose (psd_basis_matrix, geometry, tmp1);

	/* compose the above results to get the deltas matrix */
	psd_curves_compose (tmp2, tmp1, deltas);

	/* extract the x deltas */
	x   = deltas[0][0];
	dx  = deltas[1][0];
	dx2 = deltas[2][0];
	dx3 = deltas[3][0];

	/* extract the y deltas */
	y   = deltas[0][1];
	dy  = deltas[1][1];
	dy2 = deltas[2][1];
	dy3 = deltas[3][1];

	lastx = (psd_int)PSD_CONSTRAIN(x, 0, 255);
	lasty = (psd_int)PSD_CONSTRAIN(y, 0, 255);

	table[lastx] = lasty;

	/* loop over the curve */
	for (i = 0; i < PSD_CURVES_SUBDIVIDE; i++)
	{
		/* increment the x values */
		x += dx;
		dx += dx2;
		dx2 += dx3;

		/* increment the y values */
		y += dy;
		dy += dy2;
		dy2 += dy3;

		newx = (psd_int)(x + 0.5);
		newx = PSD_CONSTRAIN(newx, 0, 255);
		newy = (psd_int)(y + 0.5);
		newy = PSD_CONSTRAIN(newy, 0, 255);

		/* if this point is different than the last one...then draw it */
		if ((lastx != newx) || (lasty != newy))
			table[newx] = newy;

		lastx = newx;
		lasty = newy;
	}
}

static void psd_curves_calculate_table(psd_layer_curves_data * curve, psd_uchar * table)
{
	psd_int i, x, y;
	psd_int p1, p2, p3, p4;
	
	/*  Initialize boundary curve points */
	for(i = 0; i < curve->input_value[0]; i ++)
		table[i] = (psd_uchar)curve->output_value[0];
	for(i = curve->input_value[curve->point_count - 1]; i < 256; i ++)
		table[i] = (psd_uchar)curve->output_value[curve->point_count - 1];

	for (i = 0; i < curve->point_count - 1; i++)
	{
		p1 = (i == 0) ? i : i - 1;
		p2 = i;
		p3 = i + 1;
		p4 = (i == (curve->point_count - 2)) ? curve->point_count - 1 : i + 2;

		psd_curves_plot(curve, table, p1, p2, p3, p4);
	}

	/* ensure that the control points are used exactly */
	for (i = 0; i < curve->point_count; i++)
	{
		x = curve->input_value[i];
		y = curve->output_value[i];
		table[x] = y;
	}
}

psd_bool psd_layer_blend_curves(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_int i, j;
	psd_layer_curves * data = NULL;
	psd_uchar lookup_table[4][256];
	
	if(context->color_mode != psd_color_mode_rgb && context->color_mode != psd_color_mode_grayscale)
		return psd_false;

	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_type[i] == psd_layer_info_type_curves)
		{
			data = (psd_layer_curves *)layer->layer_info_data[i];
			break;
		}
	}
	if(data == NULL)
		return psd_false;

	if(layer->adjustment_valid == psd_true)
	{
		for(i = 0; i < 256; i ++)
		{
			for(j = 0; j < 4; j ++)
				lookup_table[j][i] = i;
		}
		
		for(i = 0; i < data->curve_count; i ++)
		{
			if(context->color_mode == psd_color_mode_grayscale)
			{
				if(data->curve[i].point_count >= 2 && data->curve[i].channel_index == 0)
					psd_curves_calculate_table(&data->curve[i], data->lookup_table[0]);
			}
			else if(data->curve[i].point_count >= 2 && data->curve[i].channel_index <= 3)
			{
				psd_curves_calculate_table(&data->curve[i], lookup_table[data->curve[i].channel_index]);
			}
		}

		if(context->color_mode == psd_color_mode_rgb)
		{
			for(i = 0; i < 256; i ++)
			{
				for(j = 0; j < 3; j ++)
					data->lookup_table[j][i] = lookup_table[j + 1][lookup_table[0][i]];
			}
		}
	}
	
	psd_adjustment_blend_rgb(context, layer, dst_rect, data->lookup_table[0],
		data->lookup_table[1], data->lookup_table[2], psd_false);

	layer->adjustment_valid = psd_false;

	return psd_true;
}

