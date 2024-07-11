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
 * $Id: image_data.c, created by Patrick in 2006.05.25, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"


#ifdef PSD_INCLUDE_ZLIB
extern psd_status psd_unzip_without_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len);
extern psd_status psd_unzip_with_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len, 
	psd_int row_size, psd_int color_depth);
#endif


// 1bit bitamp
static psd_status psd_combine_bitmap1_channel(psd_context * context)
{
	psd_int i, j, rowstride;
	psd_uchar * black, * cur_black;
	psd_int mask;
	psd_argb_color * dst_color = context->merged_image_data;

	black = context->temp_image_data;

	rowstride = (context->width + 7) / 8;
	for(i = 0; i < context->height; i ++)
	{
		cur_black = black + rowstride * i;
		mask = 0x80;
		for(j = 0; j < context->width; j ++)
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

// 8bit grayscale
static psd_status psd_combine_grayscale8_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * gray, * alpha, gray_color;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 2)
	{
		alpha = context->temp_image_data;
		gray = context->temp_image_data + context->per_channel_length;

		for(i = context->width * context->height; i --; )
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
		
		for(i = context->width * context->height; i --; )
		{
			gray_color = *gray;
			*dst_color = PSD_RGB_TO_COLOR(gray_color, gray_color, gray_color);
			dst_color ++;
			gray ++;
		}
	}

	return psd_status_done;
}

// 16bit grayscale
static psd_status psd_combine_grayscale16_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * gray, * alpha, gray_color;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 2)
	{
		alpha = context->temp_image_data;
		gray = context->temp_image_data + context->per_channel_length;

		for(i = context->width * context->height; i --; )
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
		
		for(i = context->width * context->height; i --; )
		{
			gray_color = PSD_CHAR_TO_SHORT(gray) >> 8;
			*dst_color = PSD_RGB_TO_COLOR(gray_color, gray_color, gray_color);
			dst_color ++;
			gray += 2;
		}
	}

	return psd_status_done;
}

// 8bit indexed
static psd_status psd_combine_indexed8_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * index, indexed;
	psd_argb_color * dst_color = context->merged_image_data;

	index = context->temp_image_data;

	for(i = context->width * context->height; i --; )
	{
		indexed = *index;
		if(indexed >= context->indexed_color_table_count)
			*dst_color = psd_color_clear;
		else if(indexed == context->transparency_index)
			*dst_color = psd_color_clear;
		else
			*dst_color = context->color_map[indexed];
		dst_color ++;
		index ++;
	}

	return psd_status_done;
}

// 8bit rgb
static psd_status psd_combine_rgb8_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * red, * green, * blue, * alpha;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 4)
	{
		alpha = context->temp_image_data;
		red = context->temp_image_data + context->per_channel_length;
		green = context->temp_image_data + context->per_channel_length * 2;
		blue = context->temp_image_data + context->per_channel_length * 3;
		
		for(i = context->width * context->height; i --; )
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
		
		for(i = context->width * context->height; i --; )
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

// 16bit rgb
static psd_status psd_combine_rgb16_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * red, * green, * blue, * alpha;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 4)
	{
		alpha = context->temp_image_data;
		red = context->temp_image_data + context->per_channel_length;
		green = context->temp_image_data + context->per_channel_length * 2;
		blue = context->temp_image_data + context->per_channel_length * 3;

		for(i = context->width * context->height; i --; )
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

		for(i = context->width * context->height; i --; )
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

// 8bit cmyk
static psd_status psd_combine_cmyk8_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * cyan, * magenta, * yellow, * black, * alpha;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 5)
	{
		alpha = context->temp_image_data;
		cyan = context->temp_image_data + context->per_channel_length;
		magenta = context->temp_image_data + context->per_channel_length * 2;
		yellow = context->temp_image_data + context->per_channel_length * 3;
		black = context->temp_image_data + context->per_channel_length * 4;

		for(i = context->width * context->height; i --; )
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

		for(i = context->width * context->height; i --; )
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

// 16bit cmyk
static psd_status psd_combine_cmyk16_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * cyan, * magenta, * yellow, * black, * alpha;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 5)
	{
		alpha = context->temp_image_data;
		cyan = context->temp_image_data + context->per_channel_length;
		magenta = context->temp_image_data + context->per_channel_length * 2;
		yellow = context->temp_image_data + context->per_channel_length * 3;
		black = context->temp_image_data + context->per_channel_length * 4;

		for(i = context->width * context->height; i --; )
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

		for(i = context->width * context->height; i --; )
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

// 8bit lab
static psd_status psd_combine_lab8_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * lightness, * a, * b, * alpha;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 4)
	{
		alpha = context->temp_image_data;
		lightness = context->temp_image_data + context->per_channel_length;
		a = context->temp_image_data + context->per_channel_length * 2;
		b = context->temp_image_data + context->per_channel_length * 3;

		for(i = context->width * context->height; i --; )
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

		for(i = context->width * context->height; i --; )
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

// 16bit lab
static psd_status psd_combine_lab16_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * lightness, * a, * b, * alpha;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 4)
	{
		alpha = context->temp_image_data;
		lightness = context->temp_image_data + context->per_channel_length;
		a = context->temp_image_data + context->per_channel_length * 2;
		b = context->temp_image_data + context->per_channel_length * 3;

		for(i = context->width * context->height; i --; )
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

		for(i = context->width * context->height; i --; )
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

static psd_status psd_combine_multichannel8_channel(psd_context * context)
{
	psd_int i;
	psd_uchar * cyan, * magenta, * yellow, * black;
	psd_argb_color * dst_color = context->merged_image_data;

	if(context->color_channels == 4)
	{
		cyan = context->temp_image_data;
		magenta = context->temp_image_data + context->per_channel_length;
		yellow = context->temp_image_data + context->per_channel_length * 2;
		black = context->temp_image_data + context->per_channel_length * 3;
		
		for(i = context->width * context->height; i --; )
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
		
		for(i = context->width * context->height; i --; )
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


// The last section of a Photoshop file contains the image pixel data. Image data is
// stored in planar order: first all the red data, then all the green data, etc. Each plane is
// stored in scan-line order, with no pad bytes,
psd_status psd_get_image_data(psd_context * context)
{
	psd_int pixels, length, left_size;
	psd_int i, j, k, byte_count, len, pixel_count, start_channel;
	psd_short compression;
	psd_uchar * image_data = NULL, * compress_data = NULL;
	psd_uchar * count_data, * pixel_data, * channel_data;
	psd_uchar * dst_alpha, * src_alpha;
	psd_status status;
	psd_layer_record * layer;
	
	if(context->load_tag == psd_load_tag_thumbnail || context->load_tag == psd_load_tag_exif)
		return psd_status_done;
	if(context->load_tag == psd_load_tag_layer && context->layer_count > 0)
		return psd_status_done;
	
	pixels = length = context->width * context->height;
	switch(context->depth)
	{
		case 1:			// 1bit
			length = (context->width + 7) / 8 * context->height;
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
	length *= context->channels;

	// Compression method
	compression = psd_stream_get_short(context);
	
	image_data = (psd_uchar *)psd_malloc(length);
	if(image_data == NULL)
		return psd_status_malloc_failed;
	context->temp_image_data = image_data;
	
	left_size = context->stream.file_length - context->stream.current_pos;
	if(compression != 0)
	{
		compress_data = (psd_uchar *)psd_malloc(left_size);
		if(compress_data == NULL)
		{
			psd_free(image_data);
			context->temp_image_data = NULL;
			return psd_status_malloc_failed;
		}
		psd_stream_get(context, compress_data, left_size);
	}

	switch(compression)
	{
		// Raw image data
		case 0:
			psd_assert(length <= left_size);
			psd_stream_get(context, image_data, length);
			break;

		// RLE compressed the image data starts with the byte counts for all the
		// scan lines (rows * color_channels), with each count stored as a two每byte value.
		// The RLE compressed data follows, with each scan line compressed
		// separately. The RLE compression is the same compression algorithm
		// used by the Macintosh ROM routine PackBits, and the TIFF standard.
		case 1:
			psd_assert(context->depth == 8 || context->depth == 16);
			count_data = compress_data;
			pixel_data = compress_data + context->height * context->channels * 2;
			channel_data = image_data;
			for(i = 0; i < context->channels; i ++)
			{
				pixel_count = 0;
				for(j = 0; j < context->height; j ++)
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
							memcpy(channel_data, pixel_data, len);
							channel_data += len;
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
							memset(channel_data, *pixel_data, len);
							channel_data += len;
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
			}
			break;

		// ZIP without prediction
		case 2:
#ifdef PSD_INCLUDE_ZLIB
			status = psd_unzip_without_prediction(compress_data, left_size, image_data, length);
			if (status != psd_status_done)
				return status;
#else
			psd_assert(0);
#endif
			break;

		// ZIP with prediction.
		case 3:
#ifdef PSD_INCLUDE_ZLIB
			status = psd_unzip_with_prediction(compress_data, left_size, image_data, length, 
				context->width, context->depth);
			if (status != psd_status_done)
				return status;
#else
			psd_assert(0);
#endif
			break;

		default:
			psd_assert(0);
			return psd_status_unsupport_image_compression;
	}
	psd_freeif(compress_data);

	// process image data
	context->merged_image_data = (psd_argb_color *)psd_malloc(context->width * context->height * 4);
	if(context->merged_image_data == NULL)
	{
		psd_freeif(image_data);
		context->temp_image_data = NULL;
		return psd_status_malloc_failed;
	}

	// combine each channel to image data
	switch(context->color_mode)
	{
		// 1bit
		case psd_color_mode_bitmap:
			if(context->depth == 1)
				status = psd_combine_bitmap1_channel(context);
			else
				psd_assert(0);
			break;

		// 8bit or 16bit
		case psd_color_mode_grayscale:
		// Other applications that read Photoshop files can treat a
		// duotone image as a gray image
		case psd_color_mode_duotone:
			if(context->depth == 8)
				status = psd_combine_grayscale8_channel(context);
			else if(context->depth == 16)
				status = psd_combine_grayscale16_channel(context);
			else
				psd_assert(0);
			break;

		// 8bit
		case psd_color_mode_indexed:
			if(context->depth == 8)
				status = psd_combine_indexed8_channel(context);
			else
				psd_assert(0);
			break;
			
		// 8bit or 16bit
		case psd_color_mode_rgb:
			if(context->depth == 8)
				status = psd_combine_rgb8_channel(context);
			else if(context->depth == 16)
				status = psd_combine_rgb16_channel(context);
			else
				psd_assert(0);
			break;
			
		// 8bit or 16bit
		case psd_color_mode_cmyk:
#ifdef PSD_SUPPORT_CMYK
			if(context->depth == 8)
				status = psd_combine_cmyk8_channel(context);
			else if(context->depth == 16)
				status = psd_combine_cmyk16_channel(context);
			else
				psd_assert(0);
#endif
			break;
			
		// 8bit or 16bit
		case psd_color_mode_lab:
#ifdef PSD_SUPPORT_LAB
			if(context->depth == 8)
				status = psd_combine_lab8_channel(context);
			else if(context->depth == 16)
				status = psd_combine_lab16_channel(context);
			else
				psd_assert(0);
#endif
			break;

		case psd_color_mode_multichannel:
#ifdef PSD_SUPPORT_MULTICHANNEL
			if(context->depth == 8)
				status = psd_combine_multichannel8_channel(context);
			else
				psd_assert(0);
#endif
			break;

		default:
			psd_assert(0);
			break;
	}

	if(status != psd_status_done)
		return status;

	for(i = context->color_channels; i < context->channels; i ++)
	{
		context->alpha_channel_info[i - context->color_channels].channel_data = (psd_uchar *)psd_malloc(context->width * context->height);
		if(context->alpha_channel_info[i - context->color_channels].channel_data == NULL)
		{
			psd_freeif(image_data);
			context->temp_image_data = NULL;
			return psd_status_malloc_failed;
		}

		if(context->depth == 8)
		{
			memcpy(context->alpha_channel_info[i - context->color_channels].channel_data, 
				context->temp_image_data + context->per_channel_length * i, context->per_channel_length);
		}
		else
		{
			dst_alpha = context->alpha_channel_info[i - context->color_channels].channel_data;
			src_alpha = context->temp_image_data + context->per_channel_length * i;
			for(j = context->width * context->height; j --; )
			{
				*dst_alpha = PSD_CHAR_TO_SHORT(src_alpha) >> 8;
				dst_alpha ++;
				src_alpha += 2;
			}
		}
	}
	
	psd_freeif(image_data);
	context->temp_image_data = NULL;

	if(context->layer_count == 0 && 
		(context->load_tag == psd_load_tag_all || context->load_tag == psd_load_tag_layer))
	{
		context->layer_records = (psd_layer_record *)psd_malloc(sizeof(psd_layer_record));
		if(context->layer_records == NULL)
			return psd_status_malloc_failed;
		memset(context->layer_records, 0, sizeof(psd_layer_record));
		
		context->layer_count = 1;
		layer = context->layer_records;
		layer->layer_type = psd_layer_type_normal;
		layer->top = 0;
		layer->left = 0;
		layer->bottom = context->height;
		layer->right = context->width;
		layer->width = context->width;
		layer->height = context->height;
		layer->number_of_channels = context->color_channels;
		layer->blend_mode = psd_blend_mode_normal;
		layer->opacity = 255;
		layer->fill_opacity = 255;
		layer->clipping = psd_false;	// ??
		layer->transparency_protected = psd_false;	// ??
		layer->visible = psd_true;
		layer->obsolete = psd_false;	// ??
		layer->pixel_data_irrelevant = psd_false;	// ??
		layer->blend_clipped = psd_true;
		layer->blend_interior = psd_false;
		layer->knockout = 0;
		layer->transparency_shapes_layer = psd_true;
		layer->layer_mask_hides_effects = psd_false;
		layer->vector_mask_hides_effects = psd_false;
		layer->divider_blend_mode = psd_blend_mode_pass_through;
		layer->layer_mask_info.default_color = 255;
		layer->layer_mask_info.disabled = psd_true;

		layer->number_of_channels = context->color_channels;
		layer->channel_info = (psd_channel_info *)psd_malloc(layer->number_of_channels * sizeof(psd_channel_info));
		if(layer->channel_info == NULL)
			return psd_status_malloc_failed;
		memset(layer->channel_info, 0, layer->number_of_channels * sizeof(psd_channel_info));
		start_channel = 0;
		if(context->color_mode == psd_color_mode_rgb && context->color_channels == 4)
			start_channel = -1;
		else if(context->color_mode == psd_color_mode_cmyk && context->color_channels == 5)
			start_channel = -1;
		for(i = 0; i < layer->number_of_channels; i ++)
		{
			layer->channel_info[i].channel_id = start_channel;
			layer->channel_info[i].restricted = psd_false;
			start_channel ++;
		}

		layer->layer_blending_ranges.gray_black_src = 0;
		layer->layer_blending_ranges.gray_white_src = 65535;
		layer->layer_blending_ranges.gray_black_dst = 0;
		layer->layer_blending_ranges.gray_white_dst = 65535;
		layer->layer_blending_ranges.number_of_blending_channels = context->color_channels;
		layer->layer_blending_ranges.channel_black_src = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		layer->layer_blending_ranges.channel_white_src = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		layer->layer_blending_ranges.channel_black_dst = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		layer->layer_blending_ranges.channel_white_dst = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		if(layer->layer_blending_ranges.channel_black_src == NULL || 
			layer->layer_blending_ranges.channel_white_src == NULL ||
			layer->layer_blending_ranges.channel_black_dst == NULL ||
			layer->layer_blending_ranges.channel_white_dst == NULL)
		{
			return psd_status_malloc_failed;
		}
		for(i = 0; i < layer->layer_blending_ranges.number_of_blending_channels; i ++)
		{
			// channel source range
			layer->layer_blending_ranges.channel_black_src[i] = 0;
			layer->layer_blending_ranges.channel_white_src[i] = 65535;
			// channel destination range
			layer->layer_blending_ranges.channel_black_dst[i] = 0;
			layer->layer_blending_ranges.channel_white_dst[i] = 65535;
		}

		if(context->load_tag == psd_load_tag_all)
		{
			layer->image_data = (psd_argb_color *)psd_malloc(context->width * context->height * 4);
			if(layer->image_data == NULL)
				return psd_status_malloc_failed;
			memcpy(layer->image_data, context->merged_image_data, context->width * context->height * 4);
		}
		else if(context->load_tag == psd_load_tag_layer)
		{
			layer->image_data = context->merged_image_data;
			context->merged_image_data = NULL;
		}
	}
	
	return psd_status_done;
}

void psd_image_data_free(psd_context * context)
{
	psd_freeif(context->merged_image_data);
	psd_freeif(context->temp_image_data);
}

void psd_alpha_channel_free(psd_context * context)
{
	psd_int i;

	if(context->alpha_channel_info == NULL)
		return;

	for(i = context->color_channels; i < context->channels; i ++)
	{
		psd_freeif(context->alpha_channel_info[i - context->color_channels].channel_data);
		context->alpha_channel_info[i - context->color_channels].channel_data = NULL;
		psd_freeif(context->alpha_channel_info[i - context->color_channels].unicode_name);
		context->alpha_channel_info[i - context->color_channels].unicode_name = NULL;
	}

	psd_free(context->alpha_channel_info);
	context->alpha_channel_info = NULL;
}
