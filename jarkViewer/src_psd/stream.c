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
 * $Id: stream.c, created by Patrick in 2006.05.18, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include <string.h>


static
#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
psd_bool psd_test_big_endian(void)
{
	static psd_bool result = 2;
	const psd_int byte_order[1] = { 1 };

	if (result == 2)
	{
		if(((char *)byte_order)[0] == 1)
			result = psd_false;
		else
			result = psd_true;
	}

	return result;
}

psd_int psd_stream_get(psd_context * context, psd_uchar * buffer, psd_int length)
{
    return context->stream.stream_get(context, buffer, length);
}



psd_int psd_stream_get_file(psd_context * context, psd_uchar * buffer, psd_int length)
{
	psd_stream * stream;
	psd_int left, read = 0;

	if(buffer == NULL)
		return 0;
	
	psd_assert(length >= 0);
	
	stream = &context->stream;
	
	if(stream->buffer == NULL)
	{
		stream->buffer = (psd_uchar *)psd_malloc(PSD_STREAM_MAX_READ_LENGTH);
		if(stream->buffer == NULL)
			return 0;
	}

	left = stream->read_in_length - stream->read_out_length;
	if(left > 0 && left <= length)
	{
		memcpy(buffer, stream->buffer + stream->read_out_length, left);
		buffer += left;
		length -= left;
		stream->read_out_length = stream->read_in_length;
		read += left;
	}

	if(length > PSD_STREAM_MAX_READ_LENGTH)
	{
		read += psd_fread(buffer, length, context->file);
		stream->read_out_length = stream->read_in_length;
	}
	else if(stream->read_out_length == stream->read_in_length)
	{
		if(length > 0)
		{
			stream->read_in_length = psd_fread(stream->buffer, PSD_STREAM_MAX_READ_LENGTH, context->file);
			if(length > stream->read_in_length)
				length = stream->read_in_length;
			memcpy(buffer, stream->buffer, length);
			read += length;
			stream->read_out_length = length;
		}
	}
	else
	{
		left = stream->read_in_length - stream->read_out_length;
		if(length > left)
			length = left;
		memcpy(buffer, stream->buffer + stream->read_out_length, length);
		stream->read_out_length += length;
		read += length;
	}

	stream->current_pos += read;

	return read;
}

psd_int psd_stream_get_memory(psd_context * context, psd_uchar * buffer, psd_int length)
{
	psd_stream * stream;
	psd_int left, read = 0;

	if(buffer == NULL)
		return 0;
	
	psd_assert(length >= 0);
	stream = &context->stream;
    memcpy(buffer, context->file_name_or_buffer + stream->current_pos, length);
	stream->current_pos += length;
	return length;
}
psd_int psd_stream_get_null(psd_context * context, psd_int length)
{
    return context->stream.stream_get_null(context, length);
}


psd_int psd_stream_get_null_file(psd_context * context, psd_int length)
{
	psd_stream * stream;
	psd_int left, read = 0;

	psd_assert(length >= 0);

	if(length <= 0)
		return 0;

	stream = &context->stream;
	
	if(stream->buffer == NULL)
	{
		stream->buffer = (psd_uchar *)psd_malloc(PSD_STREAM_MAX_READ_LENGTH);
		if(stream->buffer == NULL)
			return 0;
	}

	left = stream->read_in_length - stream->read_out_length;
	if(left > 0 && left <= length)
	{
		length -= left;
		stream->read_out_length = stream->read_in_length;
		read += left;
	}

	if(length > PSD_STREAM_MAX_READ_LENGTH)
	{
		psd_fseek(context->file, length);
		read += length;
		stream->read_out_length = stream->read_in_length;
	}
	else if(stream->read_out_length == stream->read_in_length)
	{
		if(length > 0)
		{
			stream->read_in_length = psd_fread(stream->buffer, PSD_STREAM_MAX_READ_LENGTH, context->file);
			if(length > stream->read_in_length)
				length = stream->read_in_length;
			read += length;
			stream->read_out_length = length;
		}
	}
	else
	{
		left = stream->read_in_length - stream->read_out_length;
		if(length > left)
			length = left;
		stream->read_out_length += length;
		read += length;
	}

	stream->current_pos += read;

	return read;
}

psd_int psd_stream_get_null_memory(psd_context * context, psd_int length)
{
	psd_stream * stream;
	psd_int left, read = 0;

	psd_assert(length >= 0);

	if(length <= 0)
		return 0;

	stream = &context->stream;

	stream->current_pos += length;

	return length;
}

psd_bool psd_stream_get_bool(psd_context * context)
{
	return psd_stream_get_char(context) > 0 ? psd_true : psd_false;
}

psd_uchar psd_stream_get_char(psd_context * context)
{
	psd_uchar str[4];
	psd_uchar value = 0;

	if(psd_stream_get(context, str, 1) == 1)
		value = str[0];

	return value;
}

psd_short psd_stream_get_short(psd_context * context)
{
	psd_uchar str[4];
	psd_short value = 0;

	if(psd_stream_get(context, str, 2) == 2)
		value = PSD_CHAR_TO_SHORT(str);

	return value;
}

psd_int psd_stream_get_int(psd_context * context)
{
	psd_uchar str[4];
	psd_int value = 0;

	if(psd_stream_get(context, str, 4) == 4)
		value = PSD_CHAR_TO_INT(str);

	return value;
}

psd_float psd_stream_get_float(psd_context * context)
{
	psd_uchar str[4], *dst;
	psd_float value = 0.0;
	psd_int i;

	if(psd_stream_get(context, str, 4) == 4)
	{
		if (psd_test_big_endian() == psd_true)
		{
			memcpy(&value, str, 4);
		}
		else
		{
			dst = (psd_uchar *)&value;
			for(i = 0; i < 4; i ++)
			{
				*dst++ = str[3 - i];
			}
		}
	}

	return value;
}

psd_double psd_stream_get_double(psd_context * context)
{
	psd_uchar str[8], *dst;
	psd_double value = 0.0;
	psd_int i;

	if(psd_stream_get(context, str, 8) == 8)
	{
		if (psd_test_big_endian() == psd_true)
		{
			memcpy(&value, str, 8);
		}
		else
		{
			dst = (psd_uchar *)&value;
			for(i = 0; i < 8; i ++)
			{
				*dst++ = str[7 - i];
			}
		}
	}

	return value;
}

psd_argb_color psd_stream_get_space_color(psd_context * context)
{
	psd_color_space color_space;	// 2 bytes for color space
	psd_ushort color_component[4];		// 4 * 2 byte color component
	psd_int i;
	psd_argb_color color;

	// Color: 2 bytes for space followed by 4 * 2 byte color component
	color_space = psd_stream_get_short(context);
	for(i = 0; i < 4; i ++)
		color_component[i] = psd_stream_get_short(context) >> 8;
	if(psd_color_space_to_argb(&color, color_space, color_component) != psd_status_done)
		return psd_color_clear;

	return color;
}

psd_blend_mode psd_stream_get_blend_mode(psd_context * context)
{
	psd_uint tag;
	psd_blend_mode blend_mode = psd_blend_mode_normal;

	tag = psd_stream_get_int(context);
	switch(tag)
	{
		case 'norm':
			blend_mode = psd_blend_mode_normal;
			break;
		case 'dark':
			blend_mode = psd_blend_mode_darken;
			break;
		case 'lite':
			blend_mode = psd_blend_mode_lighten;
			break;
		case 'hue ':
			blend_mode = psd_blend_mode_hue;
			break;
		case 'sat ':
			blend_mode = psd_blend_mode_saturation;
			break;
		case 'colr':
			blend_mode = psd_blend_mode_color;
			break;
		case 'lum ':
			blend_mode = psd_blend_mode_luminosity;
			break;
		case 'mul ':
			blend_mode = psd_blend_mode_multiply;
			break;
		case 'scrn':
			blend_mode = psd_blend_mode_screen;
			break;
		case 'diss':
			blend_mode = psd_blend_mode_dissolve;
			break;
		case 'over':
			blend_mode = psd_blend_mode_overlay;
			break;
		case 'hLit':
			blend_mode = psd_blend_mode_hard_light;
			break;
		case 'sLit':
			blend_mode = psd_blend_mode_soft_light;
			break;
		case 'diff':
			blend_mode = psd_blend_mode_difference;
			break;
		case 'smud':
			blend_mode = psd_blend_mode_exclusion;
			break;
		case 'div ':
			blend_mode = psd_blend_mode_color_dodge;
			break;
		case 'idiv':
			blend_mode = psd_blend_mode_color_burn;
			break;
		case 'lbrn':
			blend_mode = psd_blend_mode_linear_burn;
			break;
		case 'lddg':
			blend_mode = psd_blend_mode_linear_dodge;
			break;
		case 'vLit':
			blend_mode = psd_blend_mode_vivid_light;
			break;
		case 'lLit':
			blend_mode = psd_blend_mode_linear_light;
			break;
		case 'pLit':
			blend_mode = psd_blend_mode_pin_light;
			break;
		case 'hMix':
			blend_mode = psd_blend_mode_hard_mix;
			break;
		case 'pass':
			blend_mode = psd_blend_mode_pass_through;
			break;
		default:
			psd_assert(0);
			break;
	}

	return blend_mode;
}

void psd_stream_free(psd_context * context)
{
    context->stream.stream_free(context);
}

void psd_stream_free_file(psd_context * context)
{
	if (context->stream.buffer != NULL)
	{
		psd_free(context->stream.buffer);
		context->stream.buffer = NULL;
		context->stream.read_out_length = context->stream.read_in_length = 0;
	}
	if (context->file != NULL)
	{
		psd_fclose(context->file);
		context->file = NULL;
	}
}

void psd_stream_free_memory(psd_context * context)
{

}

void psd_stream_init_file(psd_context * context)
{
    context->stream.stream_free = psd_stream_free_file;
    context->stream.stream_get = psd_stream_get_file;
    context->stream.stream_get_null = psd_stream_get_null_file;
}

void psd_stream_init_memory(psd_context * context)
{
    context->stream.stream_free = psd_stream_free_memory;
    context->stream.stream_get = psd_stream_get_memory;
    context->stream.stream_get_null = psd_stream_get_null_memory;
}


