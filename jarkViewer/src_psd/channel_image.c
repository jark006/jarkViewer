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
 * $Id: channel_image.c, created by Patrick in 2006.05.25, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_math.h"


#define PSD_MIN_TEMP_CHANNEL_LENGTH		4096
#define PSD_MIN_TEMP_IMAGE_LENGTH		12288

#ifdef PSD_INCLUDE_ZLIB
extern psd_status psd_unzip_without_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len);
extern psd_status psd_unzip_with_prediction(psd_uchar *src_buf, psd_int src_len, 
	psd_uchar *dst_buf, psd_int dst_len, 
	psd_int row_size, psd_int color_depth);
#endif


// 1bit bitamp
static psd_status psd_combine_bitmap1_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i, j, rowstride;
	psd_uchar * black = NULL, * cur_black;
	psd_int mask;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				black = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(black != NULL);

	rowstride = (layer->width + 7) / 8;
	for(i = 0; i < layer->height; i ++)
	{
		cur_black = black + rowstride * i;
		mask = 0x80;
		for(j = 0; j < layer->width; j ++)
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
static psd_status psd_combine_grayscale8_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * gray = NULL, * alpha = NULL, gray_color;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				gray = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(gray != NULL);
	
	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
		{
			gray_color = *gray;
			*dst_color = PSD_RGB_TO_COLOR(gray_color, gray_color, gray_color);
			dst_color ++;
			gray ++;
		}
	}
	else
	{
		for(i = layer->width * layer->height; i --; )
		{
			gray_color = *gray;
			*dst_color = PSD_ARGB_TO_COLOR(*alpha, gray_color, gray_color, gray_color);
			dst_color ++;
			alpha ++;
			gray ++;
		}
	}

	return psd_status_done;
}

// 16bit grayscale
static psd_status psd_combine_grayscale16_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * gray = NULL, * alpha = NULL, gray_color;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				gray = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(gray != NULL);
	
	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
		{
			gray_color = PSD_CHAR_TO_SHORT(gray) >> 8;
			*dst_color = PSD_RGB_TO_COLOR(gray_color, gray_color, gray_color);
			dst_color ++;
			gray += 2;
		}
	}
	else
	{
		for(i = layer->width * layer->height; i --; )
		{
			gray_color = PSD_CHAR_TO_SHORT(gray) >> 8;
			*dst_color = PSD_ARGB_TO_COLOR(PSD_CHAR_TO_SHORT(alpha) >> 8, gray_color, gray_color, gray_color);
			dst_color ++;
			alpha += 2;
			gray += 2;
		}
	}

	return psd_status_done;
}

// 8bit indexed
static psd_status psd_combine_indexed8_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * index = NULL, indexed;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				index = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(index != NULL);

	for(i = layer->width * layer->height; i --; )
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
static psd_status psd_combine_rgb8_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * red = NULL, * green = NULL, * blue = NULL, * alpha = NULL;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				red = context->temp_image_data + context->max_channel_length * i;
				break;
			case 1:
				green = context->temp_image_data + context->max_channel_length * i;
				break;
			case 2:
				blue = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(red != NULL && green != NULL && blue != NULL);

	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
		{
			*dst_color = PSD_RGB_TO_COLOR(*red, *green, *blue);
			dst_color ++;
			red ++;
			green ++;
			blue ++;
		}
	}
	else
	{
		for(i = layer->width * layer->height; i --; )
		{
			*dst_color = PSD_ARGB_TO_COLOR(*alpha, *red, *green, *blue);
			dst_color ++;
			alpha ++;
			red ++;
			green ++;
			blue ++;
		}
	}

	return psd_status_done;
}

// 16bit rgb
static psd_status psd_combine_rgb16_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * red = NULL, * green = NULL, * blue = NULL, * alpha = NULL;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				red = context->temp_image_data + context->max_channel_length * i;
				break;
			case 1:
				green = context->temp_image_data + context->max_channel_length * i;
				break;
			case 2:
				blue = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(red != NULL && green != NULL && blue != NULL);

	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
		{
			*dst_color = PSD_RGB_TO_COLOR(PSD_CHAR_TO_SHORT(red) >> 8, 
				PSD_CHAR_TO_SHORT(green) >> 8, PSD_CHAR_TO_SHORT(blue) >> 8);
			dst_color ++;
			red += 2;
			green += 2;
			blue += 2;
		}
	}
	else
	{
		for(i = layer->width * layer->height; i --; )
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

	return psd_status_done;
}


#ifdef PSD_SUPPORT_CMYK

// 8bit cmyk
static psd_status psd_combine_cmyk8_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * cyan = NULL, * magenta = NULL, * yellow = NULL, * black = NULL, * alpha = NULL;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				cyan = context->temp_image_data + context->max_channel_length * i;
				break;
			case 1:
				magenta = context->temp_image_data + context->max_channel_length * i;
				break;
			case 2:
				yellow = context->temp_image_data + context->max_channel_length * i;
				break;
			case 3:
				black = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(cyan != NULL && magenta != NULL && yellow != NULL && black != NULL);

	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
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
		for(i = layer->width * layer->height; i --; )
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

	return psd_status_done;
}

// 16bit cmyk
static psd_status psd_combine_cmyk16_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * cyan = NULL, * magenta = NULL, * yellow = NULL, * black = NULL, * alpha = NULL;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				cyan = context->temp_image_data + context->max_channel_length * i;
				break;
			case 1:
				magenta = context->temp_image_data + context->max_channel_length * i;
				break;
			case 2:
				yellow = context->temp_image_data + context->max_channel_length * i;
				break;
			case 3:
				black = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(cyan != NULL && magenta != NULL && yellow != NULL && black != NULL);

	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
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
	else
	{
		for(i = layer->width * layer->height; i --; )
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

	return psd_status_done;
}

#endif // ifdef PSD_SUPPORT_CMYK

#ifdef PSD_SUPPORT_LAB

// 8bit lab
static psd_status psd_combine_lab8_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * lightness = NULL, * a = NULL, * b = NULL, * alpha = NULL;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				lightness = context->temp_image_data + context->max_channel_length * i;
				break;
			case 1:
				a = context->temp_image_data + context->max_channel_length * i;
				break;
			case 2:
				b = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(lightness != NULL && a != NULL && b != NULL);

	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
		{
			*dst_color = psd_lab_to_color(*lightness * 100 >> 8, *a - 128, *b - 128);
			dst_color ++;
			lightness ++;
			a ++;
			b ++;
		}
	}
	else
	{
		for(i = layer->width * layer->height; i --; )
		{
			*dst_color = psd_alab_to_color(*alpha, *lightness * 100 >> 8, *a - 128, *b - 128);
			dst_color ++;
			alpha ++;
			lightness ++;
			a ++;
			b ++;
		}
	}

	return psd_status_done;
}

// 16bit lab
static psd_status psd_combine_lab16_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * lightness = NULL, * a = NULL, * b = NULL, * alpha = NULL;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				lightness = context->temp_image_data + context->max_channel_length * i;
				break;
			case 1:
				a = context->temp_image_data + context->max_channel_length * i;
				break;
			case 2:
				b = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				alpha = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(lightness != NULL && a != NULL && b != NULL);

	if(alpha == NULL)
	{
		for(i = layer->width * layer->height; i --; )
		{
			*dst_color = psd_lab_to_color((PSD_CHAR_TO_SHORT(lightness) >> 8) * 100 >> 8, 
				(PSD_CHAR_TO_SHORT(a) >> 8) - 128, (PSD_CHAR_TO_SHORT(b) >> 8) - 128);
			dst_color ++;
			lightness += 2;
			a += 2;
			b += 2;
		}
	}
	else
	{
		for(i = layer->width * layer->height; i --; )
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

	return psd_status_done;
}

#endif // ifdef PSD_SUPPORT_LAB

#ifdef PSD_SUPPORT_MULTICHANNEL

static psd_status psd_combine_multichannel8_channel(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_uchar * cyan = NULL, * magenta = NULL, * yellow = NULL, * black = NULL;
	psd_argb_color * dst_color = layer->image_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		switch(layer->channel_info[i].channel_id)
		{
			case 0:
				cyan = context->temp_image_data + context->max_channel_length * i;
				break;
			case 1:
				magenta = context->temp_image_data + context->max_channel_length * i;
				break;
			case 2:
				yellow = context->temp_image_data + context->max_channel_length * i;
				break;
			case -1:
				black = context->temp_image_data + context->max_channel_length * i;
				break;
			case -2:
				break;
			default:
				psd_assert(0);
				break;
		}
	}

	psd_assert(cyan != NULL && magenta != NULL && yellow != NULL);

	if(black == NULL)
	{
		for(i = layer->width * layer->height; i --; )
		{
			*dst_color = psd_intcmyk_to_color(255 - *cyan, 255 - *magenta, 255 - *yellow, 0);
			dst_color ++;
			cyan ++;
			magenta ++;
			yellow ++;
		}
	}
	else
	{
		for(i = layer->width * layer->height; i --; )
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

#endif // ifdef PSD_SUPPORT_MULTICHANNEL


// 8bit user supplied layer mask
static psd_status psd_get_layer_user_supplied_layer_mask8(psd_context * context, psd_layer_record * layer)
{
	psd_int i;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		if(layer->channel_info[i].channel_id == -2)
		{
			layer->layer_mask_info.mask_data = (psd_color_component *)psd_malloc(layer->layer_mask_info.width * layer->layer_mask_info.height);
			if(layer->layer_mask_info.mask_data == NULL)
				return psd_status_malloc_failed;
			memcpy(layer->layer_mask_info.mask_data, context->temp_image_data + context->max_channel_length * i, 
				layer->layer_mask_info.width * layer->layer_mask_info.height);
			return psd_status_done;
		}
	}

	return psd_status_done;
}

// 16bit user supplied layer mask
static psd_status psd_get_layer_user_supplied_layer_mask16(psd_context * context, psd_layer_record * layer)
{
	psd_int per_channel_length = layer->width * layer->height;
	psd_int mask_channel_length = layer->layer_mask_info.width * layer->layer_mask_info.height;
	psd_int max_channel_length = PSD_MAX(per_channel_length, mask_channel_length);
	psd_int i, j;
	psd_uchar * src_mask;
	psd_uchar * dst_mask;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		if(layer->channel_info[i].channel_id == -2)
		{
			layer->layer_mask_info.mask_data = (psd_color_component *)psd_malloc(mask_channel_length);
			if(layer->layer_mask_info.mask_data == NULL)
				return psd_status_malloc_failed;

			src_mask = context->temp_image_data + max_channel_length * 2 * i;
			dst_mask = layer->layer_mask_info.mask_data;
			for(j = mask_channel_length; j --; )
			{
				*dst_mask = PSD_CHAR_TO_SHORT(src_mask) >> 8;
				dst_mask ++;
				src_mask += 2;
			}
			return psd_status_done;
		}
	}

	return psd_status_done;
}

// Channel image data
psd_status psd_get_layer_channel_image_data(psd_context * context, psd_layer_record * layer)
{
	psd_int i, j, k, len, length, mask_channel_length, per_channel_length, max_channel_length, pixels, mask_pixels, height;
	psd_short compression;
	psd_uchar * image_data, * count_data, * pixel_data;
	psd_int pixel_count, byte_count;
	psd_status status = psd_status_done;

	pixels = length = layer->width * layer->height;
	switch(context->depth)
	{
		case 1:			// 1bit
			length = (layer->width + 7) / 8 * layer->height;
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
	per_channel_length = length;
	switch(context->depth)
	{
		case 8:
			mask_channel_length = layer->layer_mask_info.width * layer->layer_mask_info.height;
			break;
		case 16:
			mask_channel_length = layer->layer_mask_info.width * layer->layer_mask_info.height * 2;
			break;
		default:
			mask_channel_length = 0;
			break;
	}
	mask_pixels = layer->layer_mask_info.width * layer->layer_mask_info.height;
	if (context->depth == 16)
		mask_pixels *= 2;
	max_channel_length = PSD_MAX(mask_channel_length, per_channel_length);
	if(max_channel_length <= 0)
	{
		for(i = 0; i < layer->number_of_channels; i ++)
			psd_stream_get_null(context, layer->channel_info[i].data_length);
		return psd_status_done;
	}
	
	context->per_channel_length = per_channel_length;
	context->max_channel_length = max_channel_length;
	length = max_channel_length * layer->number_of_channels;
	if(length > context->temp_image_length)
	{
		psd_freeif(context->temp_image_data);
		context->temp_image_length = PSD_MAX(context->temp_image_length * 2, length);
		context->temp_image_length = PSD_MAX(context->temp_image_length, PSD_MIN_TEMP_IMAGE_LENGTH);
		context->temp_image_data = (psd_uchar *)psd_malloc(context->temp_image_length);
		if(context->temp_image_data == NULL)
		{
			context->temp_image_length = 0;
			return psd_status_malloc_failed;
		}
	}

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		psd_assert(layer->channel_info[i].channel_id >= -2);
		
		length = layer->channel_info[i].data_length - 2;
		if(length > context->temp_channel_length)
		{
			psd_freeif(context->temp_channel_data);
			context->temp_channel_length = PSD_MAX(context->temp_channel_length * 2, length);
			context->temp_channel_length = PSD_MAX(context->temp_channel_length, PSD_MIN_TEMP_CHANNEL_LENGTH);
			context->temp_channel_data = (psd_uchar *)psd_malloc(context->temp_channel_length);
			if(context->temp_channel_data == NULL)
			{
				context->temp_channel_length = 0;
				return psd_status_malloc_failed;
			}
		}

		// Compression. 0 = Raw Data, 1 = RLE compressed, 2 = ZIP without
		// prediction, 3 = ZIP with prediction.
		compression = psd_stream_get_short(context);

		if(length <= 0)
			continue;
	
		if(psd_stream_get(context, context->temp_channel_data, layer->channel_info[i].data_length - 2)
			!= layer->channel_info[i].data_length - 2)
			return psd_status_fread_error;

		image_data = context->temp_image_data + i * max_channel_length;
		// Compression. 0 = Raw Data, 1 = RLE compressed, 2 = ZIP without
		// prediction, 3 = ZIP with prediction.		
		switch(compression)
		{
			// If the compression code is 0, the image data is just the raw image data,
			// whose size is calculated as (LayerBottom每LayerTop)*
			// (LayerRight每LayerLeft).
			case 0:
				if(layer->channel_info[i].channel_id == -2)
					memcpy(image_data, context->temp_channel_data, mask_channel_length);
				else
					memcpy(image_data, context->temp_channel_data, per_channel_length);
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
				if(layer->channel_info[i].channel_id == -2)
					height = layer->layer_mask_info.height;
				else
					height = layer->height;
				
				count_data = context->temp_channel_data;
				pixel_data = context->temp_channel_data + height * 2;
				for(j = 0; j < height; j ++)
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
				
				if(layer->channel_info[i].channel_id == -2)
					psd_assert(pixel_count == mask_pixels);
				else
					psd_assert(pixel_count == pixels);
				break;

			case 2:		// ZIP without prediction
#ifdef PSD_INCLUDE_ZLIB
				status = psd_unzip_without_prediction(context->temp_channel_data, layer->channel_info[i].data_length - 2,
					image_data, layer->channel_info[i].channel_id == -2 ? mask_pixels : pixels);
				if (status != psd_status_done)
					return status;
#else
				psd_assert(0);
#endif
				break;

			case 3:		// ZIP with prediction
#ifdef PSD_INCLUDE_ZLIB
				status = psd_unzip_with_prediction(context->temp_channel_data, layer->channel_info[i].data_length - 2,
					image_data, layer->channel_info[i].channel_id == -2 ? mask_pixels : pixels, 
					layer->channel_info[i].channel_id == -2 ? layer->layer_mask_info.width : layer->width,
					context->depth);
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
	}

	if(layer->width * layer->height > 0)
	{
		layer->image_data = (psd_argb_color *)psd_malloc(layer->width * layer->height * 4);
		if(layer->image_data == NULL)
			return psd_status_malloc_failed;

		// combine each channel to image data
		switch(context->color_mode)
		{
			// 1bit
			case psd_color_mode_bitmap:
				if(context->depth == 1)
					status = psd_combine_bitmap1_channel(context, layer);
				else
					psd_assert(0);
				break;

			// 8bit or 16bit
			case psd_color_mode_grayscale:
			// Other applications that read Photoshop files can treat a
			// duotone image as a gray image
			case psd_color_mode_duotone:
				if(context->depth == 8)
					status = psd_combine_grayscale8_channel(context, layer);
				else if(context->depth == 16)
					status = psd_combine_grayscale16_channel(context, layer);
				else
					psd_assert(0);
				break;

			// 8bit
			case psd_color_mode_indexed:
				if(context->depth == 8)
					status = psd_combine_indexed8_channel(context, layer);
				else
					psd_assert(0);
				break;
				
			// 8bit or 16bit
			case psd_color_mode_rgb:
				if(context->depth == 8)
					status = psd_combine_rgb8_channel(context, layer);
				else if(context->depth == 16)
					status = psd_combine_rgb16_channel(context, layer);
				else
					psd_assert(0);
				break;
				
			// 8bit or 16bit
			case psd_color_mode_cmyk:
#ifdef PSD_SUPPORT_CMYK
				if(context->depth == 8)
					status = psd_combine_cmyk8_channel(context, layer);
				else if(context->depth == 16)
					status = psd_combine_cmyk16_channel(context, layer);
				else
					psd_assert(0);
#endif
				break;
				
			// 8bit or 16bit
			case psd_color_mode_lab:
#ifdef PSD_SUPPORT_LAB
				if(context->depth == 8)
					status = psd_combine_lab8_channel(context, layer);
				else if(context->depth == 16)
					status = psd_combine_lab16_channel(context, layer);
				else
					psd_assert(0);
#endif
				break;

			case psd_color_mode_multichannel:
#ifdef PSD_SUPPORT_MULTICHANNEL
				if(context->depth == 8)
					status = psd_combine_multichannel8_channel(context, layer);
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
	}

	// get transparency mask and user supplied layer mask
	if(layer->layer_mask_info.width * layer->layer_mask_info.height > 0)
	{
		if(context->depth == 8)
			status = psd_get_layer_user_supplied_layer_mask8(context, layer);
		else if(context->depth == 16)
			status = psd_get_layer_user_supplied_layer_mask16(context, layer);
		if(status != psd_status_done)
			return status;
	}

	return psd_status_done;
}

