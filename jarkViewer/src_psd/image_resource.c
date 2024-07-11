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
 * $Id: image_resource.c, created by Patrick in 2006.05.18, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_fixed.h"

#ifdef PSD_INCLUDDE_LIBEXIF
#include <libexif/exif-data.h>
static const psd_uchar ExifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
#endif

#ifdef PSD_INCLUDE_LIBXML
#include <libxml/parser.h>
#endif


extern psd_status psd_thumbnail_decode_jpeg(psd_argb_color ** dst_image, psd_int compress_len, psd_context * context);
extern psd_status psd_thumbnail_decode_raw(psd_argb_color ** dst_image, psd_int image_len, psd_context * context);
extern void psd_alpha_channel_free(psd_context * context);
extern psd_status psd_get_path(psd_context * context, psd_int length);
extern void psd_path_free(psd_context * context);


psd_status psd_get_image_resource(psd_context * context)
{
	psd_int length, i, size;
	psd_ushort ID;
	psd_uint tag;
	psd_uchar sizeofname;
	psd_int sizeofdata, prev_stream_pos;
	psd_uchar * buffer;
	//psd_status status;

	// Length of image resource section
	length = psd_stream_get_int(context);
	if(length <= 0)
		return psd_status_done;

	// default
	context->global_angle = 30;
	context->global_altitude = 30;

	while(length > 0)
	{
		// Signature: '8BIM'
		tag = psd_stream_get_int(context);
		if(tag == '8BIM')
		{
			length -= 4;
			// Unique identifier for the resource
			ID = psd_stream_get_short(context);
			length -= 2;
			// Name: Pascal string, padded to make the size even (a null name consists of two bytes of 0)
			sizeofname = psd_stream_get_char(context);
			if((sizeofname & 0x01) == 0)
				sizeofname ++;
			psd_stream_get_null(context, sizeofname);
			length -= sizeofname + 1;
			// Actual size of resource data that follows
			sizeofdata = psd_stream_get_int(context);
			length -= 4;
			// resource data must be even
			if(sizeofdata & 0x01)
				sizeofdata ++;
			length -= sizeofdata;

			switch(context->load_tag)
			{
				case psd_load_tag_thumbnail:
					if(ID != 1033 && ID != 1036)
					{
						psd_stream_get_null(context, sizeofdata);
						continue;
					}
					break;
				case psd_load_tag_merged:
					// alpha channels information
					if(ID != 1006 && ID != 1045 && ID != 1053)
					{
						psd_stream_get_null(context, sizeofdata);
						continue;
					}
					break;
				case psd_load_tag_exif:
					if(ID != 1058 && ID != 1059)
					{
						psd_stream_get_null(context, sizeofdata);
						continue;
					}
					break;
			}

			prev_stream_pos = context->stream.current_pos;

			if(sizeofdata > 0)
			{
				switch(ID)
				{
					// ResolutionInfo structure
					case 1005:
						// Horizontal resolution in pixels per inch.
						context->resolution_info.hres = psd_stream_get_int(context) / 65536.0f;
						// 1=display horitzontal resolution in pixels per inch; 2=display horitzontal resolution in pixels per cm.
						context->resolution_info.hres_unit = psd_stream_get_short(context);
						// Display width as 1=inches; 2=cm; 3=points; 4=picas; 5=columns.
						context->resolution_info.width_unit = psd_stream_get_short(context);
						// Vertial resolution in pixels per inch.
						context->resolution_info.vres = psd_stream_get_int(context) / 65536.0f;
						// 1=display vertical resolution in pixels per inch; 2=display vertical resolution in pixels per cm.
						context->resolution_info.vres_unit = psd_stream_get_short(context);
						// Display height as 1=inches; 2=cm; 3=points; 4=picas; 5=columns.
						context->resolution_info.height_unit = psd_stream_get_short(context);
						context->fill_resolution_info = psd_true;
						break;

					// Names of the alpha channels as a series of Pascal strings.
					case 1006:
						buffer = (psd_uchar *)psd_malloc(sizeofdata);
						if(buffer == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, buffer, sizeofdata);
						if(context->alpha_channels == 0)
						{
							size = 0;
							// maybe odd
							while(size + 1 < sizeofdata)
							{
								size += *(buffer + size) + 1;
								context->alpha_channels ++;
							}
							context->color_channels = context->channels - context->alpha_channels;
							context->alpha_channel_info = (psd_alpha_channel_info *)psd_malloc(context->alpha_channels * sizeof(psd_alpha_channel_info));
							if(context->alpha_channel_info == NULL)
							{
								psd_free(buffer);
								return psd_status_malloc_failed;
							}
							memset(context->alpha_channel_info, 0, context->alpha_channels * sizeof(psd_alpha_channel_info));
						}

						size = 0;
						for(i = 0; i < context->alpha_channels; i ++)
						{
							memcpy(context->alpha_channel_info[i].name, buffer + size + 1, *(buffer + size));
							size += *(buffer + size) + 1;
						}
						psd_free(buffer);
						context->fill_alpha_channel_info = psd_true;
						break;

					// DisplayInfo structure
					case 1007:
						context->display_info.color = psd_stream_get_space_color(context);
						// 0..100
						context->display_info.opacity = psd_stream_get_short(context);
						psd_assert(context->display_info.opacity >= 0 && context->display_info.opacity <= 100);
						// selected = 0, protected = 1
						context->display_info.kind = psd_stream_get_char(context);
						// maybe be 2 when color mode is multichannel
						//psd_assert(context->display_info.kind == 0 || context->display_info.kind == 1);
						// padding
						psd_stream_get_char(context);
						context->fill_display_info = psd_true;
						break;

					// The caption as a Pascal string.
					case 1008:
						size = psd_stream_get_char(context);
						psd_stream_get(context, context->caption, size);
						break;

					// Layer state information
					// 2 bytes containing the index of target layer (0 = bottom layer).
					case 1024:
						context->target_layer_index = psd_stream_get_short(context);
						break;

					// Layers group information
					// 2 bytes per layer containing a group ID for the dragging groups. Layers in
					// a group have the same group ID.
					case 1026:
						context->layer_group_count = sizeofdata / 2;
						context->layer_group_id = (psd_ushort *)psd_malloc(context->layer_group_count * 2);
						if(context->layer_group_id == NULL)
							return psd_status_malloc_failed;
						for(i = 0; i < context->layer_group_count; i ++)
							context->layer_group_id[i] = psd_stream_get_short(context);
						context->fill_layer_group = psd_true;
						break;

					// (Photoshop 4.0) Thumbnail resource for Photoshop 4.0 only
					case 1033:
					// (Photoshop 5.0) Thumbnail resource (supersedes resource 1033)
					case 1036:
						if(context->load_tag == psd_load_tag_layer)
						{
							psd_stream_get_null(context, sizeofdata);
							continue;
						}

						// 1 = kJpegRGB . Also supports kRawRGB (0).
						context->thumbnail_resource.format = psd_stream_get_int(context);
						psd_assert(context->thumbnail_resource.format == 0 || context->thumbnail_resource.format == 1);
						// Width of thumbnail in pixels.
						context->thumbnail_resource.width = psd_stream_get_int(context);
						// Height of thumbnail in pixels.
						context->thumbnail_resource.height = psd_stream_get_int(context);
						// Padded row bytes = (width * bits per pixel + 31) / 32 * 4.
						context->thumbnail_resource.width_bytes = psd_stream_get_int(context);
						// Total size = widthbytes * height * planes
						context->thumbnail_resource.total_size = psd_stream_get_int(context);
						// Used for consistency check.
						context->thumbnail_resource.size_after_compression = psd_stream_get_int(context);
						context->thumbnail_resource.bits_per_pixel = psd_stream_get_short(context);
						// Bits per pixel. = 24
						psd_assert(context->thumbnail_resource.bits_per_pixel == 24);
						context->thumbnail_resource.number_of_planes = psd_stream_get_short(context);
						// Number of planes. = 1
						psd_assert(context->thumbnail_resource.number_of_planes == 1);
#ifdef PSD_INCLUDE_LIBJPEG
						if(context->thumbnail_resource.format == 0)
						{
							status = psd_thumbnail_decode_raw(&context->thumbnail_resource.thumbnail_data, 
								context->thumbnail_resource.size_after_compression, context);
						}
						else
						{
							status = psd_thumbnail_decode_jpeg(&context->thumbnail_resource.thumbnail_data, 
								context->thumbnail_resource.size_after_compression, context);
						}
						if(status != psd_status_done)
							return status;
#else
						context->thumbnail_resource.jfif_data = (psd_uchar *)psd_malloc(sizeofdata - 28);
						if(context->thumbnail_resource.jfif_data == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, context->thumbnail_resource.jfif_data, sizeofdata - 28);
#endif
						context->fill_thumbnail_resource = psd_true;
						break;

					// (Photoshop 4.0) Copyright flag
					// Boolean indicating whether image is copyrighted. Can be set via
					// Property suite or by user in File Info...
					case 1034:
						context->copyright_flag = (psd_bool)psd_stream_get_short(context);
						psd_assert(context->copyright_flag == 0 || context->copyright_flag == 1);
						break;

					// (Photoshop 5.0) Global Angle
					// 4 bytes that contain an integer between 0 and 359, which is the global
					// lighting angle for effects layer. If not present, assumed to be 30.
					case 1037:
						context->global_angle = psd_stream_get_int(context);
						break;

					// (Photoshop 5.0) Effects visible
					// 1-byte global flag to show/hide all the effects layer. Only present when
					// they are hidden.
					case 1042:
						context->effects_visible = (psd_bool)psd_stream_get_short(context);
						psd_assert(context->effects_visible == 0 || context->effects_visible == 1);
						break;

					// (Photoshop 5.0) Unicode Alpha Names
					// Unicode string (4 bytes length followed by string).
					case 1045:
						buffer = (psd_uchar *)psd_malloc(sizeofdata);
						if(buffer == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, buffer, sizeofdata);
						if(context->alpha_channels == 0)
						{
							size = 0;
							while(size < sizeofdata)
							{
								size += PSD_CHAR_TO_INT(buffer + size) * 2 + 4;
								context->alpha_channels ++;
							}
							context->color_channels = context->channels - context->alpha_channels;
							context->alpha_channel_info = (psd_alpha_channel_info *)psd_malloc(context->alpha_channels * sizeof(psd_alpha_channel_info));
							if(context->alpha_channel_info == NULL)
							{
								psd_free(buffer);
								return psd_status_malloc_failed;
							}
							memset(context->alpha_channel_info, 0, context->alpha_channels * sizeof(psd_alpha_channel_info));
						}

						size = 0;
						for(i = 0; i < context->alpha_channels; i ++)
						{
							context->alpha_channel_info[i].unicode_name_length = PSD_CHAR_TO_INT(buffer + size);
							context->alpha_channel_info[i].unicode_name = (psd_ushort *)psd_malloc(2 * context->alpha_channel_info[i].unicode_name_length);
							if(context->alpha_channel_info[i].unicode_name == NULL)
							{
								psd_free(buffer);
								return psd_status_malloc_failed;
							}
							memcpy(context->alpha_channel_info[i].unicode_name, buffer + size + 4, 2 * context->alpha_channel_info[i].unicode_name_length);
							size += 2 * context->alpha_channel_info[i].unicode_name_length + 4;
						}
						psd_free(buffer);
						context->fill_alpha_channel_info = psd_true;
						break;

					// (Photoshop 6.0) Indexed Color Table Count
					// 2 bytes for the number of colors in table that are actually defined
					case 1046:
						context->indexed_color_table_count = psd_stream_get_short(context);
						break;

					// (Photoshop 6.0) Transparency Index.
					// 2 bytes for the index of transparent color, if any.
					case 1047:
						context->transparency_index = psd_stream_get_short(context);
						break;

					// (Photoshop 6.0) Global Altitude
					// 4 byte entry for altitude
					case 1049:
						context->global_altitude = psd_stream_get_int(context);
						break;

					// (Photoshop 6.0) Alpha Identifiers
					// 4 bytes of length, followed by 4 bytes each for every alpha identifier.
					case 1053:
						if(context->alpha_channels == 0)
						{
							context->alpha_channels = sizeofdata / 4;
							context->color_channels = context->channels - context->alpha_channels;
							context->alpha_channel_info = (psd_alpha_channel_info *)psd_malloc(context->alpha_channels * sizeof(psd_alpha_channel_info));
							if(context->alpha_channel_info == NULL)
								return psd_status_malloc_failed;
							memset(context->alpha_channel_info, 0, context->alpha_channels * sizeof(psd_alpha_channel_info));
						}

						for(i = 0; i < context->alpha_channels; i ++)
						{
							context->alpha_channel_info[i].identifier = psd_stream_get_int(context);
						}
						context->fill_alpha_channel_info = psd_true;
						break;

					// (Photoshop 6.0) Version Info
					// 4 bytes version, 1 byte hasRealMergedData, Unicode string: writer
					// name, Unicode string: reader name, 4 bytes file version.
					case 1057:
						context->version_info.version = psd_stream_get_int(context);
						context->version_info.has_real_merged_data = psd_stream_get_bool(context);
						context->version_info.writer_name_length = psd_stream_get_int(context);
						context->version_info.writer_name = (psd_ushort *)psd_malloc(2 * context->version_info.writer_name_length);
						if(context->version_info.writer_name == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, (psd_uchar *)context->version_info.writer_name, 2 * context->version_info.writer_name_length);
						context->version_info.reader_name_length = psd_stream_get_int(context);
						context->version_info.reader_name = (psd_ushort *)psd_malloc(2 * context->version_info.reader_name_length);
						if(context->version_info.reader_name == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, (psd_uchar *)context->version_info.reader_name, 2 * context->version_info.reader_name_length);
						context->version_info.file_version = psd_stream_get_int(context);
						context->fill_version_info = psd_true;
						break;

// if you don't need the following image resource,
// you can undef this macro in psd_config.h to reduce the parsing time
/////////////////////////////////////////////////////////////////////////////////////////////
#ifdef PSD_GET_ALL_IMAGE_RESOURCE

					// Border information
					case 1009:
						// a fixed number (2 bytes real, 2 bytes fraction) for the border width
						context->border_info.border_width = psd_fixed_16_16_tofloat((psd_fixed_16_16)psd_stream_get_int(context));
						// 2 bytes for border units (1 = inches, 2 = cm, 3 = points, 4 = picas, 5 = columns).
						context->border_info.border_units = (psd_units)psd_stream_get_short(context);
						psd_assert(context->border_info.border_units >= psd_units_inches && 
							context->border_info.border_units <= psd_units_columns);
						context->fill_border_info = psd_true;
						break;

					// Print flags
					// A series of one-byte boolean values (see Page Setup dialog): labels, crop
					// marks, color bars, registration marks, negative, flip, interpolate, caption,
					// print flags.
					case 1011:
						context->print_flags.labels = psd_stream_get_bool(context);
						context->print_flags.crop_marks = psd_stream_get_bool(context);
						context->print_flags.color_bars = psd_stream_get_bool(context);
						context->print_flags.registration_marks = psd_stream_get_bool(context);
						context->print_flags.negative = psd_stream_get_bool(context);
						context->print_flags.flip = psd_stream_get_bool(context);
						context->print_flags.interpolate = psd_stream_get_bool(context);
						context->print_flags.caption = psd_stream_get_bool(context);
						context->print_flags.print_flags = psd_stream_get_bool(context);
						context->fill_print_flags = psd_true;
						break;

					// (Photoshop 4.0) Grid and guides information
					case 1032:
						// Version ( = 1)
						psd_assert(psd_stream_get_int(context) == 1);
						// Future implementation of document-specific grids (4 bytes horizontal, 4 bytes vertical).
						context->grid_guides.horz_grid = psd_stream_get_int(context);
						context->grid_guides.vert_grid = psd_stream_get_int(context);
						// Number of guide resource blocks (can be 0).
						context->grid_guides.guide_count = psd_stream_get_int(context);
						if(context->grid_guides.guide_count > 0)
						{
							//Location of guide in document coordinates. Since the guide is either vertical or horizontal, this only has to be one component of the coordinate.
							context->grid_guides.guide_coordinate = (psd_int *)psd_malloc(context->grid_guides.guide_count * 4);
							// Direction of guide. VHSelect is a system type of psd_uchar where 0 = vertical, 1 = horizontal.
							context->grid_guides.guide_direction = (psd_uchar *)psd_malloc(context->grid_guides.guide_count);
							if(context->grid_guides.guide_coordinate == NULL ||
								context->grid_guides.guide_direction == NULL)
							{
								psd_free(context->grid_guides.guide_coordinate);
								context->grid_guides.guide_coordinate = NULL;
								psd_free(context->grid_guides.guide_direction);
								context->grid_guides.guide_direction = NULL;
								return psd_status_malloc_failed;
							}
							for(i = 0; i < context->grid_guides.guide_count; i ++)
							{
								context->grid_guides.guide_coordinate[i] = psd_stream_get_int(context);
								context->grid_guides.guide_direction[i] = psd_stream_get_char(context);
							}
						}
						context->fill_grid_and_guides_info = psd_true;
						break;

					// (Photoshop 5.0) Color samplers resource
					case 1038:
						// Version ( = 1)
						psd_assert(psd_stream_get_int(context) == 1);
						// Number of color samplers to follow.
						context->color_samplers.number_of_color_samplers = psd_stream_get_int(context);
						if (context->color_samplers.number_of_color_samplers > 0)
						{
							context->color_samplers.resource = (psd_color_samplers_resource *)
								psd_malloc(sizeof(psd_color_samplers_resource) * context->color_samplers.number_of_color_samplers);
							if (context->color_samplers.resource == NULL)
								return psd_status_malloc_failed;
							for (i = 0; i < context->color_samplers.number_of_color_samplers; i ++)
							{
								// The vertical and horizontal position of the point (4 bytes each).
								context->color_samplers.resource[i].vertical_position = psd_stream_get_int(context);
								context->color_samplers.resource[i].horizontal_position = psd_stream_get_int(context);
								// Color Space
								context->color_samplers.resource[i].color_space = psd_stream_get_short(context);
							}
						}
						context->fill_color_samplers = psd_true;
						break;
						
					// (Photoshop 6.0) Slices
					case 1050:
						// Version ( = 6)
						psd_assert(psd_stream_get_int(context) == 6);
						// Bounding rectangle for all of the slices: top, left, bottom, right of all the slices
						context->slices_resource.bounding_top = psd_stream_get_int(context);
						context->slices_resource.bounding_left = psd_stream_get_int(context);
						context->slices_resource.bounding_bottom = psd_stream_get_int(context);
						context->slices_resource.bounding_right = psd_stream_get_int(context);
						// Name of group of slices: Unicode string
						size = psd_stream_get_int(context) * 2;
						psd_stream_get_null(context, size);
						// Number of slices to follow
						context->slices_resource.number_of_slices = psd_stream_get_int(context);
						context->slices_resource.slices_resource_block = (psd_slices_resource_block *)psd_malloc(context->slices_resource.number_of_slices * sizeof(psd_slices_resource_block));
						if(context->slices_resource.slices_resource_block == NULL)
							return psd_status_malloc_failed;
						memset(context->slices_resource.slices_resource_block, 0, context->slices_resource.number_of_slices * sizeof(psd_slices_resource_block));
						for(i = 0; i < context->slices_resource.number_of_slices; i ++)
						{
							context->slices_resource.slices_resource_block[i].id = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].group_id = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].origin = psd_stream_get_int(context);
							// NOTE: Only present if Origin = 1
							if(context->slices_resource.slices_resource_block[i].origin == 1)
								context->slices_resource.slices_resource_block[i].associated_layer_id = psd_stream_get_int(context);
							// Name: Unicode string
							size = psd_stream_get_int(context) * 2;
							psd_stream_get_null(context, size);
							context->slices_resource.slices_resource_block[i].type = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].left = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].top = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].right = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].bottom = psd_stream_get_int(context);
							// URL: Unicode string
							size = psd_stream_get_int(context) * 2;
							psd_stream_get_null(context, size);
							// Target: Unicode string
							size = psd_stream_get_int(context) * 2;
							psd_stream_get_null(context, size);
							// Message: Unicode string
							size = psd_stream_get_int(context) * 2;
							psd_stream_get_null(context, size);
							// Alt Tag: Unicode string
							size = psd_stream_get_int(context) * 2;
							psd_stream_get_null(context, size);
							context->slices_resource.slices_resource_block[i].cell_text_is_html = psd_stream_get_char(context);
							// Cell text: Unicode string
							size = psd_stream_get_int(context) * 2;
							psd_stream_get_null(context, size);
							context->slices_resource.slices_resource_block[i].horizontal_alignment = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].veritcal_alignment = psd_stream_get_int(context);
							context->slices_resource.slices_resource_block[i].color = psd_argb_to_color(psd_stream_get_char(context), 
								psd_stream_get_char(context), psd_stream_get_char(context), psd_stream_get_char(context));
						}
						context->fill_slices_resource = psd_true;
						break;

					// (Photoshop 6.0) URL List
					case 1054:
						// 4 byte count of URLs
						context->url_list.number_of_urls = psd_stream_get_int(context);
						if (context->url_list.number_of_urls > 0)
						{
							context->url_list.items = (psd_url_list_item *)psd_malloc(sizeof(psd_url_list_item) *
								context->url_list.number_of_urls);
							if (context->url_list.items == NULL)
								return psd_status_malloc_failed;
							memset(context->url_list.items, 0, sizeof(psd_url_list_item) * context->url_list.number_of_urls);
						}
						for (i = 0; i < context->url_list.number_of_urls; i ++)
						{
							// followed by 4 byte long
							context->url_list.items[i].tag = psd_stream_get_int(context);
							// 4 byte ID
							context->url_list.items[i].ID = psd_stream_get_int(context);
							// Unicode string for each count.
							context->url_list.items[i].name_length = psd_stream_get_int(context);
							context->url_list.items[i].name = (psd_ushort *)psd_malloc(2 * context->url_list.items[i].name_length);
							if (context->url_list.items[i].name == NULL)
								return psd_status_malloc_failed;
							psd_stream_get(context, (psd_uchar *)context->url_list.items[i].name, 
								2 * context->url_list.items[i].name_length);
						}
						context->fill_url_list = psd_true;
						break;

					// (Photoshop 7.0) EXIF data 1
					case 1058:
					// (Photoshop 7.0) EXIF data 3
					// http://www.pima.net/standards/it10/PIMA15740/exif.htm
					case 1059:
						// avoid to get the exif data for twice
						psd_assert(context->fill_exif_data == psd_false);
#	ifdef PSD_INCLUDDE_LIBEXIF
						buffer = (psd_uchar *)psd_malloc(sizeofdata + sizeof(ExifHeader));
						if (buffer == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, buffer + sizeof(ExifHeader), sizeofdata);
						memcpy(buffer, ExifHeader, sizeof(ExifHeader));
						context->exif_data = (psd_uchar *)exif_data_new_from_data(buffer, sizeofdata + sizeof(ExifHeader));
						psd_free(buffer);
						context->fill_exif_data = psd_true;
#	else // ifdef PSD_INCLUDDE_LIBEXIF
						context->exif_data = (psd_uchar *)psd_malloc(sizeofdata);
						if (context->exif_data == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, context->exif_data, sizeofdata);
						context->exif_data_length = sizeofdata;
						context->fill_exif_data = psd_true;
#	endif // ifdef PSD_INCLUDDE_LIBEXIF
						break;
						
					// (Photoshop 7.0) XMP metadata
					// File info as XML description
					// http://Partners.adobe.com/asn/developer/xmp/main.html
					case 1060:
#	ifdef PSD_INCLUDE_LIBXML
						buffer = (psd_uchar *)psd_malloc(sizeofdata);
						if (buffer == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, buffer, sizeofdata);
						context->XMP_metadata = (psd_uchar *)xmlParseMemory(buffer, sizeofdata);
						psd_free(buffer);
						context->fill_XMP_metadata = psd_true;
#	else // ifdef PSD_INCLUDE_LIBXML
						context->XMP_metadata = (psd_uchar *)psd_malloc(sizeofdata);
						if (context->XMP_metadata == NULL)
							return psd_status_malloc_failed;
						psd_stream_get(context, context->XMP_metadata, sizeofdata);
						context->XMP_metadata_length = sizeofdata;
						context->fill_XMP_metadata = psd_true;
#	endif // ifdef PSD_INCLUDE_LIBXML
						break;

					// (Photoshop 7.0) Print scale
					case 1062:
						// 2 bytes style (0 = centered, 1 = size to fit, 2 = user defined).
						context->print_scale.style = psd_stream_get_short(context);
						psd_assert(context->print_scale.style >= psd_print_centered &&
							context->print_scale.style <= psd_print_user_defined);
						// 4 bytes x location (floating point).
						context->print_scale.x_location = psd_stream_get_float(context);
						// 4 bytes y location (floating point).
						context->print_scale.y_location = psd_stream_get_float(context);
						// 4 bytes scale (floating point)
						context->print_scale.scale = psd_stream_get_float(context);
						context->fill_print_scale = psd_true;
						break;

					// (Photoshop CS) Pixel Aspect Ratio
					case 1064:
						// 4 bytes (version = 1)
						psd_assert(psd_stream_get_int(context) == 1);
						// 8 bytes double, x / y of a pixel
						context->pixel_aspect_ratio = psd_stream_get_double(context);
						break;

					// Print flags information
					case 10000:
						// 2 bytes version ( = 1)
						psd_assert(psd_stream_get_short(context) == 1);
						// 1 byte center crop marks
						context->print_flags_info.center_crop = psd_stream_get_bool(context);
						// 1 byte ( = 0)
						psd_assert(psd_stream_get_char(context) == 0);
						// 4 bytes bleed width value
						context->print_flags_info.value = psd_stream_get_int(context);
						// 2 bytes bleed width scale
						context->print_flags_info.scale = psd_stream_get_short(context);
						context->fill_print_flags_info = psd_true;
						break;
						
#endif // ifdef PSD_GET_ALL_IMAGE_RESOURCE
/////////////////////////////////////////////////////////////////////////////////////////////

					default:
#ifdef PSD_GET_PATH_RESOURCE
						// Photoshop stores its paths as resources of type 8BIM, with IDs in the range 2000
						// through 2998.
						if(ID >= 2000 && ID <= 2998)
						{
							psd_get_path(context, sizeofdata);
						}
						// If the file contains a resource of type 8BIM with an ID of 2999, then this resource
						// contains a Pascal每style string containing the name of the clipping path to use with this
						// image when saving it as an EPS file???
						else if(ID == 2999)
						{
							// we don't find any files includes the name of the clipping path.
							psd_assert(0);
						}
						else
#endif // ifdef PSD_GET_PATH_RESOURCE
						{
							psd_stream_get_null(context, sizeofdata);
						}
						break;
				}
				
				// Filler
				psd_stream_get_null(context, prev_stream_pos + sizeofdata - context->stream.current_pos);
			}
		}
		else
		{
			return psd_status_resource_signature_error;
		}
	}

	return psd_status_done;
}

void psd_image_resource_free(psd_context * context)
{
	psd_int i;
	
	psd_alpha_channel_free(context);
	
	psd_freeif(context->layer_group_id);
	psd_freeif(context->thumbnail_resource.jfif_data);
	psd_freeif(context->thumbnail_resource.thumbnail_data);
	psd_freeif(context->version_info.writer_name);
	psd_freeif(context->version_info.reader_name);
	psd_freeif(context->grid_guides.guide_coordinate);
	psd_freeif(context->grid_guides.guide_direction);
	psd_freeif(context->slices_resource.slices_resource_block);
	psd_freeif(context->color_samplers.resource);

	for (i = 0; i < context->url_list.number_of_urls; i ++)
		psd_freeif(context->url_list.items[i].name);
	psd_freeif(context->url_list.items);

#ifdef PSD_INCLUDDE_LIBEXIF
	exif_data_free((ExifData *)context->exif_data);
#else
	psd_freeif(context->exif_data);
#endif

#ifdef PSD_INCLUDE_LIBXML
	xmlFreeDoc((xmlDocPtr)context->XMP_metadata);
#else
	psd_freeif(context->XMP_metadata);
#endif

#ifdef PSD_GET_PATH_RESOURCE
	psd_path_free(context);
#endif
}

