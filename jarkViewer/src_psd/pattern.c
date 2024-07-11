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
 * $Id: pattern.c, created by Patrick in 2006.06.21, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_bitmap.h"
#include "psd_math.h"


#define PSD_MIN_PATTERN_COUNT		4

#ifdef PSD_INCLUDE_ZLIB
extern psd_status psd_unzip_without_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len);
extern psd_status psd_unzip_with_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len, 
	psd_int row_size, psd_int color_depth);
#endif


static psd_status psd_combine_bitmap1_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i, j, rowstride;
	psd_uchar * black, * cur_black;
	psd_int mask;
	psd_argb_color * dst_color = pattern->image_data;

	black = context->temp_image_data;

	rowstride = (context->width + 7) / 8;
	for(i = 0; i < pattern->height; i ++)
	{
		cur_black = black + rowstride * i;
		mask = 0x80;
		for(j = 0; j < pattern->width; j ++)
		{
			if(*cur_black & mask)
				*dst_color = psd_color_black;
			else
				*dst_color = psd_color_white;
			dst_color ++;

			mask >>= 1;
			if(mask == 0)
			{
				mask = 0x80;
				cur_black ++;
			}
		}
	}

	return psd_status_done;
}

static psd_status psd_combine_grayscale8_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * gray, * alpha, gray_color;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 2)
	{
		alpha = context->temp_image_data;
		gray = context->temp_image_data + context->per_channel_length;

		for(i = pattern->width * pattern->height; i --; )
		{
			gray_color = *gray;
			*dst_color = PSD_ARGB_TO_COLOR(*alpha, gray_color, gray_color, gray_color);
			dst_color ++;
			alpha ++;
			gray ++;
		}
	}
	else
	{
		gray = context->temp_image_data;
		
		for(i = pattern->width * pattern->height; i --; )
		{
			gray_color = *gray;
			*dst_color = PSD_RGB_TO_COLOR(gray_color, gray_color, gray_color);
			dst_color ++;
			gray ++;
		}
	}

	return psd_status_done;
}

static psd_status psd_combine_grayscale16_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * gray, * alpha, gray_color;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 2)
	{
		alpha = context->temp_image_data;
		gray = context->temp_image_data + context->per_channel_length;

		for(i = pattern->width * pattern->height; i --; )
		{
			gray_color = PSD_CHAR_TO_SHORT(gray) >> 8;
			*dst_color = PSD_ARGB_TO_COLOR(PSD_CHAR_TO_SHORT(alpha) >> 8, gray_color, gray_color, gray_color);
			dst_color ++;
			alpha += 2;
			gray += 2;
		}
	}
	else
	{
		gray = context->temp_image_data;
		
		for(i = pattern->width * pattern->height; i --; )
		{
			gray_color = PSD_CHAR_TO_SHORT(gray) >> 8;
			*dst_color = PSD_RGB_TO_COLOR(gray_color, gray_color, gray_color);
			dst_color ++;
			gray += 2;
		}
	}

	return psd_status_done;
}

static psd_status psd_combine_indexed8_channel(psd_context * context, psd_pattern * pattern, 
	psd_argb_color * color_map)
{
	psd_int i;
	psd_uchar * index, indexed;
	psd_argb_color * dst_color = pattern->image_data;

	index = context->temp_image_data;

	for(i = pattern->width * pattern->height; i --; )
	{
		indexed = *index;
		*dst_color = color_map[indexed];
		dst_color ++;
		index ++;
	}

	return psd_status_done;
}

static psd_status psd_combine_rgb8_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * red, * green, * blue, * alpha;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 4)
	{
		alpha = context->temp_image_data;
		red = context->temp_image_data + context->per_channel_length;
		green = context->temp_image_data + context->per_channel_length * 2;
		blue = context->temp_image_data + context->per_channel_length * 3;
		
		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = PSD_ARGB_TO_COLOR(*alpha, *red, *green, *blue);
			dst_color ++;
			alpha ++;
			red ++;
			green ++;
			blue ++;
		}
	}
	else
	{
		red = context->temp_image_data;
		green = context->temp_image_data + context->per_channel_length;
		blue = context->temp_image_data + context->per_channel_length * 2;
		
		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = PSD_RGB_TO_COLOR(*red, *green, *blue);
			dst_color ++;
			red ++;
			green ++;
			blue ++;
		}
	}

	return psd_status_done;
}

static psd_status psd_combine_rgb16_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * red, * green, * blue, * alpha;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 4)
	{
		alpha = context->temp_image_data;
		red = context->temp_image_data + context->per_channel_length;
		green = context->temp_image_data + context->per_channel_length * 2;
		blue = context->temp_image_data + context->per_channel_length * 3;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = PSD_ARGB_TO_COLOR(PSD_CHAR_TO_SHORT(alpha) >> 8, 
				PSD_CHAR_TO_SHORT(red) >> 8, PSD_CHAR_TO_SHORT(green) >> 8, PSD_CHAR_TO_SHORT(blue) >> 8);
			dst_color ++;
			alpha += 2;
			red += 2;
			green += 2;
			blue += 2;
		}
	}
	else
	{
		red = context->temp_image_data;
		green = context->temp_image_data + context->per_channel_length;
		blue = context->temp_image_data + context->per_channel_length * 2;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = PSD_RGB_TO_COLOR(PSD_CHAR_TO_SHORT(red) >> 8, 
				PSD_CHAR_TO_SHORT(green) >> 8, PSD_CHAR_TO_SHORT(blue) >> 8);
			dst_color ++;
			red += 2;
			green += 2;
			blue += 2;
		}
	}

	return psd_status_done;
}


#ifdef PSD_SUPPORT_CMYK

static psd_status psd_combine_cmyk8_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * cyan, * magenta, * yellow, * black, * alpha;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 5)
	{
		alpha = context->temp_image_data;
		cyan = context->temp_image_data + context->per_channel_length;
		magenta = context->temp_image_data + context->per_channel_length * 2;
		yellow = context->temp_image_data + context->per_channel_length * 3;
		black = context->temp_image_data + context->per_channel_length * 4;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_intacmyk_to_color(*alpha, 255 - *cyan, 255 - *magenta, 255 - *yellow, 255 - *black);
			dst_color ++;
			alpha ++;
			cyan ++;
			magenta ++;
			yellow ++;
			black ++;
		}
	}
	else
	{
		cyan = context->temp_image_data;
		magenta = context->temp_image_data + context->per_channel_length;
		yellow = context->temp_image_data + context->per_channel_length * 2;
		black = context->temp_image_data + context->per_channel_length * 3;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_intcmyk_to_color(255 - *cyan, 255 - *magenta, 255 - *yellow, 255 - *black);
			dst_color ++;
			cyan ++;
			magenta ++;
			yellow ++;
			black ++;
		}
	}

	return psd_status_done;
}

static psd_status psd_combine_cmyk16_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * cyan, * magenta, * yellow, * black, * alpha;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 5)
	{
		alpha = context->temp_image_data;
		cyan = context->temp_image_data + context->per_channel_length;
		magenta = context->temp_image_data + context->per_channel_length * 2;
		yellow = context->temp_image_data + context->per_channel_length * 3;
		black = context->temp_image_data + context->per_channel_length * 4;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_intacmyk_to_color(PSD_CHAR_TO_SHORT(alpha) >> 8, 
				255 - (PSD_CHAR_TO_SHORT(cyan) >> 8), 255 - (PSD_CHAR_TO_SHORT(magenta) >> 8), 
				255 - (PSD_CHAR_TO_SHORT(yellow) >> 8), 255 - (PSD_CHAR_TO_SHORT(black) >> 8));
			dst_color ++;
			alpha += 2;
			cyan += 2;
			magenta += 2;
			yellow += 2;
			black += 2;
		}
	}
	else
	{
		cyan = context->temp_image_data;
		magenta = context->temp_image_data + context->per_channel_length;
		yellow = context->temp_image_data + context->per_channel_length * 2;
		black = context->temp_image_data + context->per_channel_length * 3;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_intcmyk_to_color(255 - (PSD_CHAR_TO_SHORT(cyan) >> 8), 
				255 - (PSD_CHAR_TO_SHORT(magenta) >> 8), 255 - (PSD_CHAR_TO_SHORT(yellow) >> 8), 
				255 - (PSD_CHAR_TO_SHORT(black) >> 8));
			dst_color ++;
			cyan += 2;
			magenta += 2;
			yellow += 2;
			black += 2;
		}
	}

	return psd_status_done;
}

#endif // ifdef PSD_SUPPORT_CMYK


#ifdef PSD_SUPPORT_LAB

static psd_status psd_combine_lab8_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * lightness, * a, * b, * alpha;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 4)
	{
		alpha = context->temp_image_data;
		lightness = context->temp_image_data + context->per_channel_length;
		a = context->temp_image_data + context->per_channel_length * 2;
		b = context->temp_image_data + context->per_channel_length * 3;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_alab_to_color(*alpha, *lightness * 100 >> 8, *a - 128, *b - 128);
			dst_color ++;
			alpha ++;
			lightness ++;
			a ++;
			b ++;
		}
	}
	else
	{
		lightness = context->temp_image_data;
		a = context->temp_image_data + context->per_channel_length;
		b = context->temp_image_data + context->per_channel_length * 2;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_lab_to_color(*lightness * 100 >> 8, *a - 128, *b - 128);
			dst_color ++;
			lightness ++;
			a ++;
			b ++;
		}
	}

	return psd_status_done;
}

static psd_status psd_combine_lab16_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * lightness, * a, * b, * alpha;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 4)
	{
		alpha = context->temp_image_data;
		lightness = context->temp_image_data + context->per_channel_length;
		a = context->temp_image_data + context->per_channel_length * 2;
		b = context->temp_image_data + context->per_channel_length * 3;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_alab_to_color(PSD_CHAR_TO_SHORT(alpha) >> 8, (PSD_CHAR_TO_SHORT(lightness) >> 8) * 100 >> 8, 
				(PSD_CHAR_TO_SHORT(a) >> 8) - 128, (PSD_CHAR_TO_SHORT(b) >> 8) - 128);
			dst_color ++;
			alpha += 2;
			lightness += 2;
			a += 2;
			b += 2;
		}
	}
	else
	{
		lightness = context->temp_image_data;
		a = context->temp_image_data + context->per_channel_length;
		b = context->temp_image_data + context->per_channel_length * 2;

		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_lab_to_color((PSD_CHAR_TO_SHORT(lightness) >> 8) * 100 >> 8, 
				(PSD_CHAR_TO_SHORT(a) >> 8) - 128, (PSD_CHAR_TO_SHORT(b) >> 8) - 128);
			dst_color ++;
			lightness += 2;
			a += 2;
			b += 2;
		}
	}

	return psd_status_done;
}

#endif // ifdef PSD_SUPPORT_LAB

#ifdef PSD_SUPPORT_MULTICHANNEL

static psd_status psd_combine_multichannel8_channel(psd_context * context, psd_pattern * pattern)
{
	psd_int i;
	psd_uchar * cyan, * magenta, * yellow, * black;
	psd_argb_color * dst_color = pattern->image_data;

	if(pattern->channel_number == 4)
	{
		cyan = context->temp_image_data;
		magenta = context->temp_image_data + context->per_channel_length;
		yellow = context->temp_image_data + context->per_channel_length * 2;
		black = context->temp_image_data + context->per_channel_length * 3;
		
		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_intcmyk_to_color(255 - *cyan, 255 - *magenta, 255 - *yellow, 255 - *black);
			dst_color ++;
			cyan ++;
			magenta ++;
			yellow ++;
			black ++;
		}
	}
	else
	{
		cyan = context->temp_image_data;
		magenta = context->temp_image_data + context->per_channel_length;
		yellow = context->temp_image_data + context->per_channel_length * 2;
		
		for(i = pattern->width * pattern->height; i --; )
		{
			*dst_color = psd_intcmyk_to_color(255 - *cyan, 255 - *magenta, 255 - *yellow, 0);
			dst_color ++;
			cyan ++;
			magenta ++;
			yellow ++;
		}
	}

	return psd_status_done;
}

#endif // ifdef PSD_SUPPORT_MULTICHANNEL


// Patterns (Photoshop 6.0 and CS (8.0))
// The following is repeated for each pattern.
psd_status psd_get_pattern(psd_context * context)
{
	psd_int pattern_length, prev_stream_pos, index, len, i, j, k,
		red, green, blue, max_channels, depth, compression;
	psd_bool written;
	psd_pattern * pattern;
	psd_argb_color * color_map = NULL;
	psd_int pixels, length;
	psd_uchar * image_data, * count_data, * pixel_data;
	psd_int pixel_count, byte_count;
	psd_status status = psd_status_done;

	// Length of this pattern
	pattern_length = psd_stream_get_int(context);
	pattern_length = (pattern_length + 3) & ~3;
	prev_stream_pos = context->stream.current_pos;

	// Version ( =1)
	if(psd_stream_get_int(context) != 1)
		return psd_status_pattern_unsupport_version;

	index = context->pattern_count;
	if(index >= context->malloc_pattern)
	{
		context->malloc_pattern = PSD_MAX(context->malloc_pattern * 2, PSD_MIN_PATTERN_COUNT);
		context->patterns = (psd_pattern *)psd_realloc(context->patterns, context->malloc_pattern * sizeof(psd_pattern));
		if(context->patterns == NULL)
			return psd_status_malloc_failed;
	}

	pattern = &context->patterns[index];
	memset(pattern, 0, sizeof(psd_pattern));
	
	// The image mode of the file. Supported values are: Bitmap = 0; Grayscale
	// = 1; Indexed = 2; RGB = 3; CMYK = 4; Multichannel = 7; Duotone = 8;
	// Lab = 9.
	pattern->color_mode = psd_stream_get_int(context);
	switch(pattern->color_mode)
	{
		case psd_color_mode_bitmap:
		case psd_color_mode_grayscale:
		case psd_color_mode_indexed:
		case psd_color_mode_rgb:
		case psd_color_mode_duotone:
			break;
		case psd_color_mode_cmyk:
#ifndef PSD_SUPPORT_CMYK
			psd_assert(0);
			return psd_status_unknown_color_mode;
#endif
			break;
		case psd_color_mode_lab:
#ifndef PSD_SUPPORT_LAB
			psd_assert(0);
			return psd_status_unknown_color_mode;
#endif
			break;
		case psd_color_mode_multichannel:
#ifndef PSD_SUPPORT_MULTICHANNEL
			psd_assert(0);
			return psd_status_unknown_color_mode;
#endif
			break;
		default:
			psd_assert(0);
			return psd_status_unknown_color_mode;
	}

	// Point: vertical, 2 bytes and horizontal, 2 bytes
	pattern->height = psd_stream_get_short(context);
	pattern->width = psd_stream_get_short(context);

	// Name: Unicode string
	pattern->name_length = psd_stream_get_int(context);
	pattern->name = (psd_ushort *)psd_malloc(2 * pattern->name_length);
	if(pattern->name == NULL)
		return psd_status_malloc_failed;
	psd_stream_get(context, (psd_uchar *)pattern->name, 2 * pattern->name_length);

	// Unique ID for this pattern: Pascal string
	len = psd_stream_get_char(context);
	psd_stream_get(context, pattern->unique_id, len);

	// Index color table (256 * 3 RGB values): only present when image mode is
	// indexed color
	if(pattern->color_mode == psd_color_mode_indexed)
	{
		color_map = (psd_argb_color *)psd_malloc(256 * 4);
		if(color_map == NULL)
			return psd_status_malloc_failed;
		
		for(i = 0; i < 256; i ++)
		{
			red = psd_stream_get_char(context);
			green = psd_stream_get_char(context);
			blue = psd_stream_get_char(context);
			color_map[i] = PSD_RGB_TO_COLOR(red, green, blue);
		}
	}

	// The following is the pattern data. It is called a virtual memory array list.

	// Version
	pattern->version = psd_stream_get_int(context);

	// Length, we do not use this
	psd_stream_get_int(context);

	// Rectangle: top, left, bottom, right
	pattern->top = psd_stream_get_int(context);
	pattern->left = psd_stream_get_int(context);
	pattern->bottom = psd_stream_get_int(context);
	pattern->right = psd_stream_get_int(context);

	// Max channels
	max_channels = psd_stream_get_int(context);

	// The following is a virtual memory array, repeated for the number of channels in the
	// image mode, not to exceed the max channels.

	for(i = 0; i < max_channels; i ++)
	{
		// Boolean indicating whether array is written
		written = psd_stream_get_int(context);

		// Length
		len = psd_stream_get_int(context);
		len -= 4 + 4 * 4 + 2 + 1;
		if(len < 0)
		{
			if(prev_stream_pos + pattern_length - context->stream.current_pos < 13)
				break;
			continue;
		}

		// Pixel depth: 1, 8 or 16
		if(written == psd_true)
		{
			if(pattern->channel_number == 0)
				depth = psd_stream_get_int(context);
			else
				psd_assert(depth == psd_stream_get_int(context));
		}
		else
		{
			psd_stream_get_int(context);
		}

		// not docutmented, top, left, bottom, right, Pixel depth
		psd_stream_get_null(context, 4 * 4 + 2);

		// Compression mode of data to follow. 'Pat2' compression is zip.
		compression = psd_stream_get_char(context);
		if(written == psd_true)
		{
			if(pattern->channel_number == 0)
			{
				pixels = length = pattern->width * pattern->height;
				switch(context->depth)
				{
					case 1:			// 1bit
						length = (pattern->width + 7) / 8 * pattern->height;
						break;
					case 8:			// 8bit
						break;
					case 16:		// 16bit
						length *= 2;
						pixels *= 2;
						break;
					default:
						psd_assert(0);
						break;
				}
				context->per_channel_length = length;
				switch(pattern->color_mode)
				{
					case psd_color_mode_bitmap:
					case psd_color_mode_indexed:
						break;
					case psd_color_mode_grayscale:
					case psd_color_mode_duotone:
						length *= 2;		// gray, alpha
						break;
						break;
					case psd_color_mode_rgb:
						length *= 4;		// red, green, blue, alpha
						break;
					case psd_color_mode_cmyk:
						length *= 5;		// cyan, magenta, yellow, black, alpha
						break;
					case psd_color_mode_lab:
						length *= 4;		// lightness, a, b, alpha
						break;
					case psd_color_mode_multichannel:
						length *= 4;		// cyan, magenta, yellow, black
						break;
				}
				if(length > context->temp_image_length)
				{
					psd_freeif(context->temp_image_data);
					context->temp_image_length = PSD_MAX(context->temp_image_length * 2, length);
					context->temp_image_data = (psd_uchar *)psd_malloc(context->temp_image_length);
					if(context->temp_image_data == NULL)
					{
						context->temp_image_length = 0;
						psd_freeif(color_map);
						return psd_status_malloc_failed;
					}
				}
			}
			
			// Actual data based on parameters and compression
			if(len > context->temp_channel_length)
			{
				psd_freeif(context->temp_channel_data);
				context->temp_channel_length = PSD_MAX(context->temp_channel_length * 2, len);
				context->temp_channel_data = (psd_uchar *)psd_malloc(context->temp_channel_length);
				if(context->temp_channel_data == NULL)
				{
					context->temp_channel_length = 0;
					psd_freeif(color_map);
					return psd_status_malloc_failed;
				}
			}

			if(psd_stream_get(context, context->temp_channel_data, len) != len)
			{
				psd_freeif(color_map);
				return psd_status_fread_error;
			}

			image_data = context->temp_image_data + pattern->channel_number * context->per_channel_length;

			// Compression. 0 = Raw Data, 1 = RLE compressed, 2 = ZIP without
			// prediction, 3 = ZIP with prediction.		
			switch(compression)
			{
				// If the compression code is 0, the image data is just the raw image data,
				// whose size is calculated as (LayerBottom每LayerTop)*
				// (LayerRight每LayerLeft).
				case 0:
					memcpy(image_data, context->temp_channel_data, context->per_channel_length);
					break;

				// If the compression code is 1, the image data starts with the byte counts
				// for all the scan lines in the channel (LayerBottom每LayerTop), with
				// each count stored as a two每byte value.(**PSB** each count stored as a
				// four-byte value.) The RLE compressed data follows, with each scan line
				// compressed separately. The RLE compression is the same compression
				// algorithm used by the Macintosh ROM routine PackBits, and the TIFF
				// standard.
				case 1:		// RLE compressed
					psd_assert(context->depth == 8 || context->depth == 16);
					pixel_count = 0;
					
					count_data = context->temp_channel_data;
					pixel_data = context->temp_channel_data + pattern->height * 2;
					for(j = 0; j < pattern->height; j ++)
					{
						byte_count = PSD_CHAR_TO_SHORT(count_data);
						for(k = 0; k < byte_count;)
						{
							len = *pixel_data;
							pixel_data ++;
							k ++;
							if(len < 128)
							{
								len ++;
								pixel_count += len;
								memcpy(image_data, pixel_data, len);
								image_data += len;
								pixel_data += len;
								k += len;
							}
							else if(len > 128)
							{
								// Next -len+1 bytes in the dest are replicated from next source byte.
								// (Interpret len as a negative 8-bit psd_int.)
								len ^= 0xff;
								len += 2;
								pixel_count += len;
								memset(image_data, *pixel_data, len);
								image_data += len;
								pixel_data ++;
								k ++;
							}
							else// len == 128
							{
								// do nothing
							}
						}
						count_data += 2;
					}
					
					psd_assert(pixel_count == pixels);
					break;

				// ZIP without prediction
				case 2:
#ifdef PSD_INCLUDE_ZLIB
					status = psd_unzip_without_prediction(context->temp_channel_data, len, image_data, pixels);
					if (status != psd_status_done)
						return status;
#else
					psd_assert(0);
#endif
					break;

				// ZIP with prediction.
				case 3:
#ifdef PSD_INCLUDE_ZLIB
					status = psd_unzip_with_prediction(context->temp_channel_data, len, image_data, pixels, 
						pattern->width, context->depth);
					if (status != psd_status_done)
						return status;
#else
					psd_assert(0);
#endif
					break;

				default:
					psd_assert(0);
					psd_freeif(color_map);
					return psd_status_unsupport_image_compression;
			}
			
			pattern->channel_number ++;
		}
		else if(len > 0)
		{
			psd_stream_get_null(context, len);
		}

		if(prev_stream_pos + pattern_length - context->stream.current_pos < 13)
			break;
	}

	pattern->image_data = (psd_argb_color *)psd_malloc(pattern->width * pattern->height * 4);
	if(pattern->image_data == NULL)
	{
		psd_freeif(color_map);
		return psd_status_malloc_failed;
	}

	// combine each channel to image data
	switch(pattern->color_mode)
	{
		// 1bit
		case psd_color_mode_bitmap:
			if(depth == 1)
				status = psd_combine_bitmap1_channel(context, pattern);
			else
				psd_assert(0);
			break;

		// 8bit or 16bit
		case psd_color_mode_grayscale:
		// Other applications that read Photoshop files can treat a
		// duotone image as a gray image
		case psd_color_mode_duotone:
			if(context->depth == 8)
				status = psd_combine_grayscale8_channel(context, pattern);
			else if(context->depth == 16)
				status = psd_combine_grayscale16_channel(context, pattern);
			else
				psd_assert(0);
			break;

		// 8bit
		case psd_color_mode_indexed:
			if(context->depth == 8)
				status = psd_combine_indexed8_channel(context, pattern, color_map);
			else
				psd_assert(0);
			break;
			
		// 8bit or 16bit
		case psd_color_mode_rgb:
			if(context->depth == 8)
				status = psd_combine_rgb8_channel(context, pattern);
			else if(context->depth == 16)
				status = psd_combine_rgb16_channel(context, pattern);
			else
				psd_assert(0);
			break;
			
		// 8bit or 16bit
		case psd_color_mode_cmyk:
#ifdef PSD_SUPPORT_CMYK
			if(context->depth == 8)
				status = psd_combine_cmyk8_channel(context, pattern);
			else if(context->depth == 16)
				status = psd_combine_cmyk16_channel(context, pattern);
			else
				psd_assert(0);
#endif
			break;
			
		// 8bit or 16bit
		case psd_color_mode_lab:
#ifdef PSD_SUPPORT_LAB
			if(context->depth == 8)
				status = psd_combine_lab8_channel(context, pattern);
			else if(context->depth == 16)
				status = psd_combine_lab16_channel(context, pattern);
			else
				psd_assert(0);
#endif
			break;

		case psd_color_mode_multichannel:
#ifdef PSD_SUPPORT_MULTICHANNEL
			if(context->depth == 8)
				status = psd_combine_multichannel8_channel(context, pattern);
			else
				psd_assert(0);
#endif
			break;

		default:
			psd_assert(0);
			break;
	}

	if(status != psd_status_done)
	{
		psd_freeif(color_map);
		return status;
	}

	psd_freeif(color_map);
	context->pattern_count ++;
	psd_stream_get_null(context, prev_stream_pos + pattern_length - context->stream.current_pos);
	
	return psd_status_done;
}

psd_status psd_pattern_fill(psd_bitmap * dst_bmp, psd_pattern * pattern, psd_int scale, psd_int offset_x, psd_int offset_y)
{
	psd_bitmap src_bmp, pat_bmp;
	psd_rect dst_rect;
	psd_int i, j;
	psd_status status;

	src_bmp.width = pattern->width;
	src_bmp.height = pattern->height;
	src_bmp.image_data = pattern->image_data;
	if(scale != 100)
	{
		pat_bmp = src_bmp;
		status = psd_create_bitmap(&src_bmp, (pattern->width * scale + 50) / 100, (pattern->height * scale + 50) / 100);
		if(status != psd_status_done)
			return status;
		status = psd_scale_bitmap(&src_bmp, &pat_bmp);
		if(status != psd_status_done)
			return status;
	}

	offset_x %= src_bmp.width;
	offset_y %= src_bmp.height;
	if(offset_x > 0)
		offset_x -= src_bmp.width;
	if(offset_y > 0)
		offset_y -= src_bmp.height;

	for(i = offset_y; i < dst_bmp->height; i += src_bmp.height)
	{
		for(j = offset_x; j < dst_bmp->width; j += src_bmp.width)
		{
			psd_make_rect(&dst_rect, j, i, j + src_bmp.width, i + src_bmp.height);
			psd_draw_bitmap(dst_bmp, &src_bmp, &dst_rect, NULL);
		}
	}

	if(scale != 100)
		psd_free_bitmap(&src_bmp);
	
	return psd_status_done;
}

void psd_pattern_free(psd_context * context)
{
	psd_int i;
	psd_pattern * pattern;
	
	if(context->patterns == NULL)
		return;

	pattern = context->patterns;
	for(i = 0; i < context->pattern_count; i ++)
	{
		psd_freeif(pattern->name);
		psd_freeif(pattern->image_data);
		pattern ++;
	}

	psd_free(context->patterns);
}

