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
 * $Id: psd.c, created by Patrick in 2006.05.18, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"


enum {
	PSD_FILE_HEADER,
	PSD_COLOR_MODE_DATA,
	PSD_IMAGE_RESOURCE,
	PSD_LAYER_AND_MASK_INFORMATION,
	PSD_IMAGE_DATA,
	PSD_DONE
};

extern psd_status psd_get_file_header(psd_context * context);
extern psd_status psd_get_color_mode_data(psd_context * context);
extern psd_status psd_get_image_resource(psd_context * context);
extern psd_status psd_get_layer_and_mask(psd_context * context);
extern psd_status psd_get_image_data(psd_context * context);
extern void psd_color_mode_data_free(psd_context * context);
extern void psd_image_resource_free(psd_context * context);
extern void psd_layer_and_mask_free(psd_context * context);
extern void psd_image_data_free(psd_context * context);
extern void psd_image_blend_free(psd_context * context);

static psd_status psd_main_loop(psd_context * context);


static psd_status psd_image_load_tag(psd_context ** dst_context, psd_char * buffer, psd_load_tag load_tag, size_t size )
{
	psd_context * context;
	psd_status status;

	if(dst_context == NULL)
		return psd_status_invalid_context;
	if(buffer == NULL)
		return psd_status_invalid_file;

	context = (psd_context *)psd_malloc(sizeof(psd_context));
	if(context == NULL)
		return psd_status_malloc_failed;
	memset(context, 0, sizeof(psd_context));
    context->file_name_or_buffer = buffer;
    if (size > 0)
    {
        // buffer is a memory block
        psd_stream_init_memory(context);
        context->size = size;
        context->stream.file_length = size;
    }

    else
    {
        // buffer is a filename
        psd_stream_init_file(context);
        context->file = psd_fopen(buffer);
    	if (context->file == NULL)
    	{
    		psd_free(context);
    		return psd_status_invalid_file;
    	}
	
     	context->stream.file_length = psd_fsize(context->file);
    }
	
	context->state = PSD_FILE_HEADER;

	context->load_tag = load_tag;
	status = psd_main_loop(context);
	
	if(status != psd_status_done)
	{
		psd_image_free(context);
		context = NULL;
	}
	else
	{
		psd_stream_free(context);
	}
	
	*dst_context = context;

	return status;
}

// Load from file
psd_status psd_image_load(psd_context ** dst_context, psd_char * file_name)
{
	return psd_image_load_tag(dst_context, file_name, psd_load_tag_all, 0);
}

psd_status psd_image_load_header(psd_context ** dst_context, psd_char * file_name)
{
	return psd_image_load_tag(dst_context, file_name, psd_load_tag_header, 0);
}

psd_status psd_image_load_layer(psd_context ** dst_context, psd_char * file_name)
{
	return psd_image_load_tag(dst_context, file_name, psd_load_tag_layer ,0);
}

psd_status psd_image_load_merged(psd_context ** dst_context, psd_char * buffer)
{
	return psd_image_load_tag(dst_context, buffer, psd_load_tag_merged , 0);
}

psd_status psd_image_load_thumbnail(psd_context ** dst_context, psd_char * file_name)
{
	return psd_image_load_tag(dst_context, file_name, psd_load_tag_thumbnail ,0 );
}

psd_status psd_image_load_exif(psd_context ** dst_context, psd_char * file_name)
{
#ifdef PSD_INCLUDE_LIBXML
	return psd_image_load_tag(dst_context, file_name, psd_load_tag_exif , 0);
#else
	return psd_status_unsupport_yet;
#endif
}

//Load from memory

psd_status psd_image_load_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size)
{
    return psd_image_load_tag(dst_context, buffer, psd_load_tag_all, size);
}

psd_status psd_image_load_header_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size)
{
    return psd_image_load_tag(dst_context, buffer, psd_load_tag_header, size);
}

psd_status psd_image_load_layer_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size)
{
    return psd_image_load_tag(dst_context, buffer, psd_load_tag_layer, size);
}

psd_status psd_image_load_merged_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size)
{
    return psd_image_load_tag(dst_context, buffer, psd_load_tag_merged, size);
}

psd_status psd_image_load_thumbnail_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size)
{
    return psd_image_load_tag(dst_context, buffer, psd_load_tag_thumbnail, size);
}


psd_status psd_image_free(psd_context * context)
{
	if(context == NULL)
		return psd_status_invalid_context;
	
	psd_color_mode_data_free(context);
	psd_image_resource_free(context);
	psd_layer_and_mask_free(context);
	psd_image_data_free(context);
	
	psd_image_blend_free(context);
	psd_stream_free(context);
	
	psd_free(context);

	return psd_status_done;
}

static psd_status psd_main_loop(psd_context * context)
{
	psd_status status = psd_status_done;

	while(status == psd_status_done)
	{
		if(context->stream.file_length <= 0)
		{
			status = psd_status_fread_error;
			break;
		}
		
		switch(context->state)
		{
			case PSD_FILE_HEADER:
				status = psd_get_file_header(context);
				if(status == psd_status_done)
				{
					if (context->load_tag == psd_load_tag_header)
						context->state = PSD_DONE;
					else
						context->state = PSD_COLOR_MODE_DATA;
				}
				else if(status == psd_status_unkown_error)
					status = psd_status_file_header_error;
				break;
				
			case PSD_COLOR_MODE_DATA:
				status = psd_get_color_mode_data(context);
				if(status == psd_status_done)
					context->state = PSD_IMAGE_RESOURCE;
				else if(status == psd_status_unkown_error)
					status = psd_status_color_mode_data_error;
				break;
				
			case PSD_IMAGE_RESOURCE:
				status = psd_get_image_resource(context);
				if(status == psd_status_done)
					context->state = PSD_LAYER_AND_MASK_INFORMATION;
				else if(status == psd_status_unkown_error)
					status = psd_status_image_resource_error;
				break;
				
			case PSD_LAYER_AND_MASK_INFORMATION:
				status = psd_get_layer_and_mask(context);
				if(status == psd_status_done)
					context->state = PSD_IMAGE_DATA;
				else if(status == psd_status_unkown_error)
					status = psd_status_layer_and_mask_error;
				break;
				
			case PSD_IMAGE_DATA:
				status = psd_get_image_data(context);
				if(status == psd_status_done)
					context->state = PSD_DONE;
				else if(status == psd_status_unkown_error)
					status = psd_status_image_data_error;
				break;

			case PSD_DONE:
				return psd_status_done;

			default:
				psd_assert(0);
				return psd_status_unkown_error;
		}
	}

	return status;
}

