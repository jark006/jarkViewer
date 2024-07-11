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
 * $Id: descriptor.c, created by Patrick in 2006.06.18, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_descriptor.h"
#include "psd_stream.h"
#include "psd_color.h"
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

static void psd_stream_get_object_list(psd_context * context);


static void psd_stream_get_unicode_name(psd_context * context)
{
	psd_int length;
	
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);
}

static void psd_stream_get_object_id(psd_context * context)
{
	psd_int length;
	
	length = psd_stream_get_int(context);
	if(length == 0)
		psd_stream_get_int(context);
	else
		psd_stream_get_null(context, length);
}

// 'obj ' = Reference
static void psd_stream_get_object_reference(psd_context * context)
{
	psd_uint type;
	psd_int number_items;

	// Number of items
	number_items = psd_stream_get_int(context);

	while(number_items --)
	{
		// OSType key for type to use
		type = psd_stream_get_int(context);

		switch(type)
		{
			// 'prop' = Property
			case 'prop':
				// Unicode string: name from classID
				psd_stream_get_unicode_name(context);
				// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte classID	
				psd_stream_get_object_id(context);
				// KeyID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte keyID
				psd_stream_get_object_id(context);
				break;
				
			// 'Clss' = Class
			case 'Clss':
				// Unicode string: name from classID
				psd_stream_get_unicode_name(context);
				// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte classID				
				psd_stream_get_object_id(context);
				break;
				
			// 'Enmr' = Enumerated Reference
			case 'Enmr':
				// Unicode string: name from classID
				psd_stream_get_unicode_name(context);
				// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte classID				
				psd_stream_get_object_id(context);
				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				psd_stream_get_object_id(context);
				// enum: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte enum
				psd_stream_get_object_id(context);
				break;
				
			// 'rele' = Offset
			case 'rele':
				// Unicode string: name from classID
				psd_stream_get_unicode_name(context);
				// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte classID				
				psd_stream_get_object_id(context);
				// Value of the offset
				psd_stream_get_int(context);
				break;
				
			// 'Idnt' = Identifier
			case 'Idnt':
				psd_stream_get_int(context);
				break;
				
			// 'indx' = Index
			case 'indx':
				psd_stream_get_int(context);
				break;
			
			// 'name' =Name
			case 'name':
				psd_stream_get_unicode_name(context);
				break;

			default:
				psd_assert(0);
				break;
		}
	}
}

// 'doub' = Double
static void psd_stream_get_object_double(psd_context * context)
{
	// Actual value (double)
	psd_stream_get_null(context, 8);
}

// 'UntF' = Unit psd_float
static void psd_stream_get_object_unit_float(psd_context * context)
{
	// Units the following value is in
	psd_stream_get_int(context);

	// Actual value (double)
	psd_stream_get_null(context, 8);
}

// 'TEXT' = String
static void psd_stream_get_object_string(psd_context * context)
{
	psd_stream_get_unicode_name(context);
}

// 'enum'= Enumerated
static void psd_stream_get_object_enumerated(psd_context * context)
{
	// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte typeID
	psd_stream_get_object_id(context);
	// enum: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte enum
	psd_stream_get_object_id(context);
}

// 'long' = Integer
static void psd_stream_get_object_integer(psd_context * context)
{
	// Value
	psd_stream_get_int(context);
}

// 'bool' = Boolean
static void psd_stream_get_object_boolean(psd_context * context)
{
	// Boolean value
	psd_stream_get_char(context);
}

// 'type' or GlbC'= Class
static void psd_stream_get_object_class(psd_context * context)
{
	// Unicode string: name from classID
	psd_stream_get_unicode_name(context);
	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID				
	psd_stream_get_object_id(context);
}

// 'alis' = Alias
static void psd_stream_get_object_alias(psd_context * context)
{
	psd_int length;
	
	// Length of data to follow
	length = psd_stream_get_int(context);

	// FSSpec for Macintosh or a handle to a string to the full path on Windows
	psd_stream_get_null(context, length);
}

// 'Objc' = Descriptor
static void psd_stream_get_object_descriptor(psd_context * context)
{
	psd_uint type;
	psd_int length, number_items;
	
	// Unicode string: name from classID
	psd_stream_get_unicode_name(context);

	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID				
	psd_stream_get_object_id(context);

	// Number of items in descriptor
	number_items = psd_stream_get_int(context);

	while(number_items --)
	{
		// Key: 4 bytes ( length) followed either by string or (if length is zero) 4-byte
		// key
		length = psd_stream_get_int(context);
		if(length == 0)
			psd_stream_get_int(context);
		else
			psd_stream_get_null(context, length);
		// Type: OSType key
		type = psd_stream_get_int(context);

		switch(type)
		{
			case 'obj ':
				psd_stream_get_object_reference(context);
				break;
			case 'Objc':
			case 'GlbO':	// GlobalObject same as Descriptor
				psd_stream_get_object_descriptor(context);
				break;
			case 'VlLs':
				psd_stream_get_object_list(context);
				break;
			case 'doub':
				psd_stream_get_object_double(context);
				break;
			case 'UntF':
				psd_stream_get_object_unit_float(context);
				break;
			case 'TEXT':
				psd_stream_get_object_string(context);
				break;
			case 'enum':
				psd_stream_get_object_enumerated(context);
				break;
			case 'long':
				psd_stream_get_object_integer(context);
				break;
			case 'bool':
				psd_stream_get_object_boolean(context);
				break;
			case 'type':
			case 'GlbC':	// 'type' or 'GlbC'= Class
				psd_stream_get_object_class(context);
				break;
			case 'alis':
				psd_stream_get_object_alias(context);
				break;
			default:
				psd_assert(0);
				break;
		}
	}
}

// 'VlLs' = List
static void psd_stream_get_object_list(psd_context * context)
{
	psd_uint type;
	psd_int number_items;

	// Number of items in the list
	number_items = psd_stream_get_int(context);

	while(number_items --)
	{
		// OSType key for type to use.
		type = psd_stream_get_int(context);
		
		switch(type)
		{
			case 'obj ':
				psd_stream_get_object_reference(context);
				break;
			case 'Objc':
			case 'GlbO':	// GlobalObject same as Descriptor
				psd_stream_get_object_descriptor(context);
				break;
			case 'VlLs':
				psd_stream_get_object_list(context);
				break;
			case 'doub':
				psd_stream_get_object_double(context);
				break;
			case 'UntF':
				psd_stream_get_object_unit_float(context);
				break;
			case 'TEXT':
				psd_stream_get_object_string(context);
				break;
			case 'enum':
				psd_stream_get_object_enumerated(context);
				break;
			case 'long':
				psd_stream_get_object_integer(context);
				break;
			case 'bool':
				psd_stream_get_object_boolean(context);
				break;
			case 'type':
			case 'GlbC':	// 'type' or 'GlbC'= Class
				psd_stream_get_object_class(context);
				break;
			case 'alis':
				psd_stream_get_object_alias(context);
				break;
			default:
				psd_assert(0);
				break;
		}
	}
}

void psd_stream_get_object_null(psd_uint type, psd_context * context)
{
	switch(type)
	{
		case 'obj ':
			psd_stream_get_object_reference(context);
			break;
		case 'Objc':
		case 'GlbO':	// GlobalObject same as Descriptor
			psd_stream_get_object_descriptor(context);
			break;
		case 'VlLs':
			psd_stream_get_object_list(context);
			break;
		case 'doub':
			psd_stream_get_object_double(context);
			break;
		case 'UntF':
			psd_stream_get_object_unit_float(context);
			break;
		case 'TEXT':
			psd_stream_get_object_string(context);
			break;
		case 'enum':
			psd_stream_get_object_enumerated(context);
			break;
		case 'long':
			psd_stream_get_object_integer(context);
			break;
		case 'bool':
			psd_stream_get_object_boolean(context);
			break;
		case 'type':
		case 'GlbC':	// 'type' or 'GlbC'= Class
			psd_stream_get_object_class(context);
			break;
		case 'alis':
			psd_stream_get_object_alias(context);
			break;
		default:
			psd_assert(0);
			break;
	}
}

psd_argb_color psd_stream_get_object_color(psd_context * context)
{
	psd_uint key;
	psd_int length, number_colors;
	psd_int red, green, blue;
	
	// Unicode string: name from classID
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);

	/***************************************************************************/
	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// rgb color
	psd_assert(key == 'RGBC');
	
	// Number of color component
	number_colors = psd_stream_get_int(context);
	// must be 3
	psd_assert(number_colors == 3);

	// Key: 4 bytes ( length) followed either by string or (if length is zero) 4-byte key
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// red component
	psd_assert(key == 'Rd  ');
	// Type: OSType key
	key = psd_stream_get_int(context);
	// Double
	psd_assert(key == 'doub');
	// Actual value (double)
	red = (psd_int)psd_stream_get_double(context);

	// Key: 4 bytes ( length) followed either by string or (if length is zero) 4-byte key
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// green component
	psd_assert(key == 'Grn ');
	// Type: OSType key
	key = psd_stream_get_int(context);
	// Double
	psd_assert(key == 'doub');
	// Actual value (double)
	green = (psd_int)psd_stream_get_double(context);

	// Key: 4 bytes ( length) followed either by string or (if length is zero) 4-byte key
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// blue component
	psd_assert(key == 'Bl  ');
	// Type: OSType key
	key = psd_stream_get_int(context);
	// Double
	psd_assert(key == 'doub');
	// Actual value (double)
	blue = (psd_int)psd_stream_get_double(context);

	return psd_rgb_to_color(red, green, blue);
}

static void psd_contour_compose(psd_matrix a, psd_matrix b, psd_matrix ab)
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

static void psd_contour_plot(psd_uchar * table, 
	psd_uchar * input_value, psd_uchar * output_value, psd_bool * corner, 
	psd_int p1, psd_int p2, psd_int p3, psd_int p4)
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

	if(corner[p2] == psd_true)
		p1 = p2;
	if(corner[p3] == psd_true)
		p4 = p3;

	/* construct the geometry matrix from the segment */
	for (i = 0; i < 4; i++)
	{
		geometry[i][2] = 0;
		geometry[i][3] = 0;
	}

	geometry[0][0] = input_value[p1];
	geometry[1][0] = input_value[p2];
	geometry[2][0] = input_value[p3];
	geometry[3][0] = input_value[p4];
	geometry[0][1] = output_value[p1];
	geometry[1][1] = output_value[p2];
	geometry[2][1] = output_value[p3];
	geometry[3][1] = output_value[p4];

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
	psd_contour_compose (psd_basis_matrix, geometry, tmp1);

	/* compose the above results to get the deltas matrix */
	psd_contour_compose (tmp2, tmp1, deltas);

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

static void psd_contour_calculate_table(psd_uchar * lookup_table, psd_int number_point,
	psd_uchar * input_value, psd_uchar * output_value, psd_bool * corner)
{
	psd_int i, x, y;
	psd_int p1, p2, p3, p4;

	/*  Initialize boundary curve points */
	for(i = 0; i < input_value[0]; i ++)
		lookup_table[i] = output_value[0];
	for(i = input_value[number_point - 1]; i < 256; i ++)
		lookup_table[i] = output_value[number_point - 1];

	for(i = 0; i < number_point - 1; i ++)
	{
		p1 = (i == 0) ? i : i - 1;
		p2 = i;
		p3 = i + 1;
		p4 = (i == (number_point - 2)) ? number_point - 1 : i + 2;

		psd_contour_plot(lookup_table, input_value, output_value, corner, p1, p2, p3, p4);
	}

	/* ensure that the control points are used exactly */
	for (i = 0; i < number_point; i++)
	{
		x = input_value[i];
		y = output_value[i];
		lookup_table[x] = y;
	}
}

void psd_stream_get_object_contour(psd_uchar * lookup_table, psd_context * context)
{
	psd_uchar input_value[256], output_value[256];
	psd_bool corner[256];
	psd_uint key, type;
	psd_int i, length, number_item, number_point, number_param;

	// Unicode string: name from classID
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);

	/***************************************************************************/
	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// shape contour
	psd_assert(key == 'ShpC');

	// Number of items in descriptor
	number_item = psd_stream_get_int(context);

	if(number_item == 2)
	{
		length = psd_stream_get_int(context);
		psd_assert(length == 0);
		key = psd_stream_get_int(context);
		// name of contour
		psd_assert(key == 'Nm  ');
		type = psd_stream_get_int(context);
		psd_assert(type == 'TEXT');

		length = psd_stream_get_int(context) * 2;
		psd_stream_get_null(context, length);
	}

	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// curve
	psd_assert(key == 'Crv ');
	type = psd_stream_get_int(context);
	// list
	psd_assert(type == 'VlLs');

	// Number of items in the list
	number_point = psd_stream_get_int(context);
	for(i = 0; i < number_point; i ++)
	{
		type = psd_stream_get_int(context);
		psd_assert(type == 'Objc');

		length = psd_stream_get_int(context) * 2;
		psd_stream_get_null(context, length);

		length = psd_stream_get_int(context);
		psd_assert(length == 0);
		key = psd_stream_get_int(context);
		// curve point
		psd_assert(key == 'CrPt');

		// set default value
		input_value[i] = output_value[i] = 0;
		corner[i] = psd_false;

		number_param = psd_stream_get_int(context);
		while(number_param --)
		{
			length = psd_stream_get_int(context);
			psd_assert(length == 0);
			key = psd_stream_get_int(context);
			type = psd_stream_get_int(context);

			switch(key)
			{
				// horizontal coordinate
				case 'Hrzn':
					psd_assert(type == 'doub');
					input_value[i] = (psd_int)psd_stream_get_double(context);
					break;
				// vertical coordinate
				case 'Vrtc':
					psd_assert(type == 'doub');
					output_value[i] = (psd_int)psd_stream_get_double(context);
					break;
				// point corner
				case 'Cnty':
					psd_assert(type == 'bool');
					corner[i] = psd_true - psd_stream_get_bool(context);
					break;
				default:
					psd_assert(0);
					psd_stream_get_object_null(type, context);
					break;
			}
		}
	}

	psd_contour_calculate_table(lookup_table, number_point, 
		input_value, output_value, corner);
}

psd_blend_mode psd_stream_get_object_blend_mode(psd_context * context)
{
	psd_blend_mode blend_mode = psd_blend_mode_normal;
	psd_int length;
	psd_uint tag;
	psd_uchar keychar[256];

	length = psd_stream_get_int(context);
	if(length == 0)
	{
		tag = psd_stream_get_int(context);
		switch(tag)
		{
			case 'Nrml':
				blend_mode = psd_blend_mode_normal;
				break;
			case 'Dslv':
				blend_mode = psd_blend_mode_dissolve;
				break;
			case 'Drkn':
				blend_mode = psd_blend_mode_darken;
				break;
			case 'Mltp':
				blend_mode = psd_blend_mode_multiply;
				break;
			case 'CBrn':
				blend_mode = psd_blend_mode_color_burn;
				break;
			case 'Lghn':
				blend_mode = psd_blend_mode_lighten;
				break;
			case 'Scrn':
				blend_mode = psd_blend_mode_screen;
				break;
			case 'CDdg':
				blend_mode = psd_blend_mode_color_dodge;
				break;
			case 'Ovrl':
				blend_mode = psd_blend_mode_overlay;
				break;
			case 'SftL':
				blend_mode = psd_blend_mode_soft_light;
				break;
			case 'HrdL':
				blend_mode = psd_blend_mode_hard_light;
				break;
			case 'Dfrn':
				blend_mode = psd_blend_mode_difference;
				break;
			case 'Xclu':
				blend_mode = psd_blend_mode_exclusion;
				break;
			case 'H   ':
				blend_mode = psd_blend_mode_hue;
				break;
			case 'Strt':
				blend_mode = psd_blend_mode_saturation;
				break;
			case 'Clr ':
				blend_mode = psd_blend_mode_color;
				break;
			case 'Lmns':
				blend_mode = psd_blend_mode_luminosity;
				break;
			default:
				psd_assert(0);
				break;
		}
	}
	else
	{
		psd_stream_get(context, keychar, length);
		keychar[length] = 0;
		if(strcmp(keychar, "linearBurn") == 0)
			blend_mode = psd_blend_mode_linear_burn;
		else if(strcmp(keychar, "linearDodge") == 0)
			blend_mode = psd_blend_mode_linear_dodge;
		else if(strcmp(keychar, "vividLight") == 0)
			blend_mode = psd_blend_mode_vivid_light;
		else if(strcmp(keychar, "linearLight") == 0)
			blend_mode = psd_blend_mode_linear_light;
		else if(strcmp(keychar, "pinLight") == 0)
			blend_mode = psd_blend_mode_pin_light;
		else if(strcmp(keychar, "hardMix") == 0)
			blend_mode = psd_blend_mode_hard_mix;
		else
			psd_assert(0);
	}

	return blend_mode;
}

psd_technique_type psd_stream_get_object_technique(psd_context * context)
{
	psd_technique_type technique_type = psd_technique_softer;
	psd_uint tag;
	psd_int length;

	length = psd_stream_get_int(context);
	if(length == 0)
	{
		tag = psd_stream_get_int(context);
		switch(tag)
		{
			case 'SfBL':
				technique_type = psd_technique_softer;
				break;
			case 'PrBL':
				technique_type = psd_technique_precise;
				break;
			case 'Slmt':
				technique_type = psd_technique_slope_limit;
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	return technique_type;
}

psd_gradient_style psd_stream_get_object_gradient_style(psd_context * context)
{
	psd_gradient_style style = psd_gradient_style_linear;
	psd_int length;
	psd_uint tag;
	
	// enum: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte enum
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	tag = psd_stream_get_int(context);
	switch(tag)
	{
		case 'Lnr ':
			style = psd_gradient_style_linear;
			break;
		case 'Rdl ':
			style = psd_gradient_style_radial;
			break;
		case 'Angl':
			style = psd_gradient_style_angle;
			break;
		case 'Rflc':
			style = psd_gradient_style_reflected;
			break;
		case 'Dmnd':
			style = psd_gradient_style_diamond;
			break;
		default:
			psd_assert(0);
			break;
	}

	return style;
}

void psd_stream_get_object_gradient_color(psd_gradient_color * gradient_color, psd_context * context)
{
	psd_int i, length, number_items, number_lists, number_data;
	psd_uint key, rootkey, type;
	psd_uchar keychar[256];
	
	// Unicode string: name from classID
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);

	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// Gradient
	psd_assert(key == 'Grdn');

	// Number of items in descriptor
	number_items = psd_stream_get_int(context);

	while(number_items --)
	{
		length = psd_stream_get_int(context);
		psd_assert(length == 0);
		if(length == 0)
			rootkey = psd_stream_get_int(context);
		else
		{
			rootkey = 0;
			psd_stream_get(context, keychar, length);
			keychar[length] = 0;
		}
		// Type: OSType key
		type = psd_stream_get_int(context);

		switch(rootkey)
		{
			case 'Nm  ':	// name
				// String
				psd_assert(type == 'TEXT');

				// String value as Unicode string
				gradient_color->name_length = psd_stream_get_int(context);
				gradient_color->name = (psd_ushort *)psd_malloc(2 * gradient_color->name_length);
				if(gradient_color->name == NULL)
					return;
				memset(gradient_color->name, 0, 2 * gradient_color->name_length);

				psd_stream_get(context, (psd_uchar *)gradient_color->name, 2 * gradient_color->name_length);
				break;

			case 'GrdF':	// gradient custom
				// Enumerated
				psd_assert(type == 'enum');

				// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte typeID
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				// Gradient Type
				psd_assert(key == 'GrdF');

				// enum: 4 bytes (length), followed either by string or (if length is zero) 4-
				// byte enum
				length = psd_stream_get_int(context);
				psd_assert(length == 0);
				key = psd_stream_get_int(context);
				psd_assert(key == 'CstS');
				break;

			case 'Intr':	// smoothness
				// double
				psd_assert(type == 'doub');

				// for Interpolation ?
				gradient_color->smoothness = (psd_int)psd_stream_get_double(context);
				break;

			case 'Clrs':	// color stop
				// value list
				psd_assert(type == 'VlLs');

				number_lists = psd_stream_get_int(context);
				// Number of color stops
				gradient_color->number_color_stops = number_lists;
				gradient_color->color_stop = (psd_gradient_color_stop *)psd_malloc(
					gradient_color->number_color_stops * sizeof(psd_gradient_color_stop));
				if(gradient_color->color_stop == NULL)
					return;
				memset(gradient_color->color_stop, 0, gradient_color->number_color_stops * sizeof(psd_gradient_color_stop));

				/***************************************************************************/
				for(i = 0; i < gradient_color->number_color_stops; i ++)
				{
					/***************************************************************************/
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Descriptor
					psd_assert(type == 'Objc');

					// Unicode string: name from classID
					length = psd_stream_get_int(context) * 2;
					psd_stream_get_null(context, length);

					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// color stop
					psd_assert(key == 'Clrt');

					// Number of items in descriptor
					number_data = psd_stream_get_int(context);
					// maybe be 4, include color, color stop type, location and midpoint
					psd_assert(number_data == 4);
					
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					psd_assert(key == 'Clr ');

					// Type: OSType key
					type = psd_stream_get_int(context);
					// Descriptor
					psd_assert(type == 'Objc');

					gradient_color->color_stop[i].actual_color = psd_stream_get_object_color(context);

					/***************************************************************************/
					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// Type
					psd_assert(key == 'Type');
					
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Enumerated
					psd_assert(type == 'enum');

					// TypeID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte typeID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// Gradient Type
					//psd_assert(key == 'GrdF');

					// enum: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte enum
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					switch(key)
					{
						case 'FrgC':
							gradient_color->color_stop[i].color_stop_type = psd_color_stop_type_foreground_color;
							break;
						case 'BckC':
							gradient_color->color_stop[i].color_stop_type = psd_color_stop_type_background_Color;
							break;
						case 'UsrS':
							gradient_color->color_stop[i].color_stop_type = psd_color_stop_type_user_stop;
							break;
						default:
							psd_assert(0);
							break;
					}

					/***************************************************************************/
					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// Location
					psd_assert(key == 'Lctn');
					
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Integer
					psd_assert(type == 'long');

					gradient_color->color_stop[i].location = psd_stream_get_int(context);

					/***************************************************************************/
					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// midpoint
					psd_assert(key == 'Mdpn');
					
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Integer
					psd_assert(type == 'long');

					gradient_color->color_stop[i].midpoint = psd_stream_get_int(context);
				}
				break;

			case 'Trns':	// tranparency stop
				// value list
				psd_assert(type == 'VlLs');

				number_lists = psd_stream_get_int(context);
				// Number of color stops
				gradient_color->number_transparency_stops = number_lists;
				gradient_color->transparency_stop = (psd_gradient_transparency_stop *)psd_malloc(
					gradient_color->number_transparency_stops * sizeof(psd_gradient_transparency_stop));
				if(gradient_color->transparency_stop == NULL)
					return;
				memset(gradient_color->transparency_stop, 0, gradient_color->number_transparency_stops * 
					sizeof(psd_gradient_transparency_stop));

				/***************************************************************************/
				for(i = 0; i < gradient_color->number_transparency_stops; i ++)
				{
					/***************************************************************************/
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Descriptor
					psd_assert(type == 'Objc');

					// Unicode string: name from classID
					length = psd_stream_get_int(context) * 2;
					psd_stream_get_null(context, length);

					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// transparency stop
					psd_assert(key == 'TrnS');

					// Number of items in descriptor
					number_data = psd_stream_get_int(context);
					// maybe be 3, include opacity, location and midpoint
					psd_assert(number_data == 3);

					/***************************************************************************/
					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// opacity
					psd_assert(key == 'Opct');
					
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Unit psd_float
					psd_assert(type == 'UntF');

					// '#Prc' = percent:
					key = psd_stream_get_int(context);
					psd_assert(key == '#Prc');

					// Actual value (double)
					gradient_color->transparency_stop[i].opacity = (psd_int)psd_stream_get_double(context);

					/***************************************************************************/
					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// Location
					psd_assert(key == 'Lctn');
					
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Integer
					psd_assert(type == 'long');

					gradient_color->transparency_stop[i].location = psd_stream_get_int(context);

					/***************************************************************************/
					// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
					// byte classID
					length = psd_stream_get_int(context);
					psd_assert(length == 0);
					key = psd_stream_get_int(context);
					// midpoint
					psd_assert(key == 'Mdpn');
					
					// Type: OSType key
					type = psd_stream_get_int(context);
					// Integer
					psd_assert(type == 'long');

					gradient_color->transparency_stop[i].midpoint = psd_stream_get_int(context);
				}
				break;
				
			default:
				psd_assert(0);
				psd_stream_get_object_null(type, context);
				break;
		}
	}
}

void psd_stream_get_object_pattern_info(psd_pattern_info * pattern_info, psd_context * context)
{
	psd_int i, length, number_items, identifier_length;
	psd_uint key, rootkey, type;
	psd_uchar keychar[256];
	
	// Unicode string: name from classID
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);

	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// pattern
	psd_assert(key == 'Ptrn');

	// Number of items in descriptor
	number_items = psd_stream_get_int(context);

	while(number_items --)
	{
		length = psd_stream_get_int(context);
		psd_assert(length == 0);
		if(length == 0)
			rootkey = psd_stream_get_int(context);
		else
		{
			rootkey = 0;
			psd_stream_get(context, keychar, length);
			keychar[length] = 0;
		}
		// Type: OSType key
		type = psd_stream_get_int(context);

		switch(rootkey)
		{
			case 'Nm  ':
				// String
				psd_assert(type == 'TEXT');
				// String value as Unicode string
				pattern_info->name_length = psd_stream_get_int(context);
				pattern_info->name = (psd_ushort *)psd_malloc(2 * pattern_info->name_length);
				if(pattern_info->name == NULL)
					return;
				memset(pattern_info->name, 0, 2 * pattern_info->name_length);

				psd_stream_get(context, (psd_uchar *)pattern_info->name, 2 * pattern_info->name_length);
				break;
				
			case 'Idnt':
				// String
				psd_assert(type == 'TEXT');
				// String value as Unicode string
				identifier_length = psd_stream_get_int(context);
				for(i = 0; i < identifier_length; i ++)
					pattern_info->identifier[i] = psd_stream_get_short(context) & 0x00FF;
				break;

			default:
				psd_assert(0);
				psd_stream_get_object_null(type, context);
				break;
		}
	}
}

void psd_stream_get_object_point(psd_int * horz, psd_int * vert, psd_context * context)
{
	psd_int length, number_item;
	psd_uint key, rootkey, type;
	
	// Unicode string: name from classID
	length = psd_stream_get_int(context) * 2;
	psd_stream_get_null(context, length);

	// classID: 4 bytes (length), followed either by string or (if length is zero) 4-
	// byte classID
	length = psd_stream_get_int(context);
	psd_assert(length == 0);
	key = psd_stream_get_int(context);
	// point
	psd_assert(key == 'Pnt ');

	// Number of items in descriptor
	number_item = psd_stream_get_int(context);

	while(number_item --)
	{
		length = psd_stream_get_int(context);
		psd_assert(length == 0);
		rootkey = psd_stream_get_int(context);
		// Type: OSType key
		type = psd_stream_get_int(context);

		switch(rootkey)
		{
			// horizontal
			case 'Hrzn':
				if(type == 'UntF')
				{
					// '#Prc' = percent:
					key = psd_stream_get_int(context);
					psd_assert(key == '#Prc');
				}
				//else if(type == 'doub')
				// Actual value (double)
				*horz = (psd_int)psd_stream_get_double(context);
				break;

			// vertical
			case 'Vrtc':
				if(type == 'UntF')
				{
					// '#Prc' = percent:
					key = psd_stream_get_int(context);
					psd_assert(key == '#Prc');
				}
				//else if(type == 'doub')
				// Actual value (double)
				*vert = (psd_int)psd_stream_get_double(context);
				break;

			default:
				psd_assert(0);
				psd_stream_get_object_null(type, context);
				break;
		}
	}
}

