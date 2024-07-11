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
 * $Id: file_header.c, created by Patrick in 2006.05.18, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_color.h"
#include "psd_stream.h"
#include "psd_system.h"


typedef struct
{
	psd_char 			signature[4];			//Signature: always equal to '8BPS'. Do not try to read the file if the signature does not match this value.
	psd_uchar 			version[2];				//Version: always equal to 1. Do not try to read the file if the version does not match this value. (**PSB** version is 2.)
	psd_char 			reserved[6];			//Reserved: must be zero.
	psd_uchar 			number_of_channels[2];	//The number of channels in the image, including any alpha channels. Supported range is 1 to 56.
	psd_uchar			height_of_image[4];		//The height of the image in pixels. Supported range is 1 to 30,000. (**PSB** max of 300,000.)
	psd_uchar			width_of_image[4];		//The width of the image in pixels. Supported range is 1 to 30,000. (*PSB** max of 300,000)
	psd_uchar			depth[2];				//Depth: the number of bits per channel. Supported values are 1, 8, and 16.
	psd_uchar			color_mode[2];			//The color mode of the file. Supported values are: Bitmap = 0; Grayscale = 1; Indexed = 2; RGB = 3; CMYK = 4; Multichannel = 7; Duotone = 8; Lab = 9.
} psd_file_header_binary;


// The file header contains the basic properties of the image
// input: context, the context of psd file
// output: status of done
psd_status psd_get_file_header(psd_context * context)
{
	psd_file_header_binary	header;

	if(psd_stream_get(context, (psd_uchar *)&header, sizeof(psd_file_header_binary)) == sizeof(psd_file_header_binary))
	{
		// Signature: always equal to '8BPS'
		if(memcmp(header.signature, "8BPS", 4) != 0)
			return psd_status_file_signature_error;

		// Version: always equal to 1
		if(PSD_CHAR_TO_SHORT(header.version) != 1)
			return psd_status_file_version_error;

		// The height of the image in pixels
		context->height = PSD_CHAR_TO_INT(header.height_of_image);
		// Supported range is 1 to 30,000
		psd_assert(context->height >= 1 && context->height <= 30000);
		
		// The width of the image in pixels
		context->width = PSD_CHAR_TO_INT(header.width_of_image);
		// Supported range is 1 to 30,000
		psd_assert(context->width >= 1 && context->width <= 30000);
		
		// The number of channels in the image, including any alpha channels
		context->color_channels = context->channels = PSD_CHAR_TO_SHORT(header.number_of_channels);
		// Supported range is 1 to 56.
		psd_assert(context->channels >= 1 && context->channels <= 56);
		
		// Depth: the number of bits per channel
		context->depth = PSD_CHAR_TO_SHORT(header.depth);
		// Supported values are 1, 8, and 16.
		psd_assert(context->depth == 1 || context->depth == 8 || context->depth == 16);
		if(context->depth != 1 && context->depth != 8 && context->depth != 16)
			return psd_status_unsupport_color_depth;
		
		// The color mode of the file
		context->color_mode = PSD_CHAR_TO_SHORT(header.color_mode);
		switch(context->color_mode)
		{
			case psd_color_mode_bitmap:
			case psd_color_mode_grayscale:
			case psd_color_mode_indexed:
			case psd_color_mode_rgb:
			case psd_color_mode_duotone:
				break;
				
			case psd_color_mode_cmyk:
#ifndef PSD_SUPPORT_CMYK
				return psd_status_unknown_color_mode;
#endif
				break;

			case psd_color_mode_lab:
#ifndef PSD_SUPPORT_LAB
				return psd_status_unknown_color_mode;
#endif
				break;

			case psd_color_mode_multichannel:
#ifndef PSD_SUPPORT_MULTICHANNEL
				return psd_status_unknown_color_mode;
#endif
				break;

			default:
				psd_assert(0);
				return psd_status_unknown_color_mode;
		}
	}
	else
	{
		return psd_status_fread_error;
	}

	return psd_status_done;
}
