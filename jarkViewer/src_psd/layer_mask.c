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
 * $Id: layer_mask.c, created by Patrick in 2006.05.19, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"


extern psd_status psd_get_layer_levels(psd_context * context, psd_layer_record * layer, psd_int data_length);
extern void psd_layer_levels_free(psd_uint info_data);
extern psd_status psd_get_layer_curves(psd_context * context, psd_layer_record * layer, psd_int data_length);
extern void psd_layer_curves_free(psd_uint info_data);
extern psd_status psd_get_layer_brightness_contrast(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_color_balance(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_hue_saturation(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_selective_color(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_threshold(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_invert(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_posterize(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_channel_mixer(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_photo_filter(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_gradient_map(psd_context * context, psd_layer_record * layer);
extern void psd_layer_gradient_map_free(psd_uint info_data);
extern psd_status psd_get_layer_effects(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_effects2(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_solid_color(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_gradient_fill(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_pattern_fill(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_channel_image_data(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_layer_type_tool(psd_context * context, psd_layer_record * layer);
extern psd_status psd_get_pattern(psd_context * context);
extern psd_status psd_get_layer_vector_mask(psd_context * context, psd_layer_record * layer, psd_int size);
extern void psd_layer_type_tool_free(psd_uint info_data);
extern void psd_layer_vector_mask_free(psd_layer_record * layer);
extern void psd_layer_effects_free(psd_uint layer_info);
extern void psd_pattern_free(psd_context * context);
static void psd_layer_free(psd_layer_record * layer);


// Layer name source setting (Photoshop 6.0)
static psd_status psd_get_layer_name_id(psd_context * context, psd_layer_record * layer)
{
	// ID for the layer name
	layer->layer_name_id = psd_stream_get_int(context);

	return psd_status_done;
}

// Unicode layer name (Photoshop 5.0)
static psd_status psd_get_layer_unicode_name(psd_context * context, psd_layer_record * layer)
{
	// Unicode string (4 bytes length + string).
	layer->unicode_name_length = psd_stream_get_int(context);
	layer->unicode_name = (psd_ushort *)psd_malloc(2 * layer->unicode_name_length);
	if(layer->unicode_name == NULL)
		return psd_status_malloc_failed;
	memset(layer->unicode_name, 0, 2 * layer->unicode_name_length);
	psd_stream_get(context, (psd_uchar *)layer->unicode_name, 2 * layer->unicode_name_length);

	return psd_status_done;
}

// Layer ID (Photoshop 5.0)
static psd_status psd_get_layer_id(psd_context * context, psd_layer_record * layer)
{
	// ID
	layer->layer_id = psd_stream_get_int(context);

	return psd_status_done;
}

// Blend clipping elements (Photoshop 6.0)
static psd_status psd_get_layer_blend_clipped(psd_context * context, psd_layer_record * layer)
{
	// Blend clipped elements: boolean
	layer->blend_clipped = psd_stream_get_bool(context);

	// padding
	psd_stream_get_null(context, 3);

	return psd_status_done;
}

// Blend interior elements (Photoshop 6.0)
static psd_status psd_get_layer_blend_interior(psd_context * context, psd_layer_record * layer)
{
	// Blend interior elements: boolean
	layer->blend_interior = psd_stream_get_bool(context);

	// padding
	psd_stream_get_null(context, 3);

	return psd_status_done;
}

// Knockout setting (Photoshop 6.0)
static psd_status psd_get_layer_knockout(psd_context * context, psd_layer_record * layer)
{
	// Knockout: boolean
	layer->knockout = psd_stream_get_bool(context);

	// padding
	psd_stream_get_null(context, 3);

	return psd_status_done;
}

// Protected setting (Photoshop 6.0)
static psd_status psd_get_layer_protected(psd_context * context, psd_layer_record * layer)
{
	psd_uint tag;

	// Protection flags: bits 0 - 2 are used for Photoshop 6.0. Transparency,
	// composite and position respectively.
	tag = psd_stream_get_int(context);
	layer->transparency = tag & 0x01;
	layer->composite = (tag & (0x01 << 1)) > 0;
	layer->position_respectively = (tag & (0x01 << 2)) > 0;

	return psd_status_done;
}

// Sheet color setting (Photoshop 6.0)
static psd_status psd_get_layer_sheet_color(psd_context * context, psd_layer_record * layer)
{
	psd_int i;
	psd_ushort color_component[4];
	
	// Color. Only the first color setting is used for Photoshop 6.0; 	
	for(i = 0; i < 4; i ++)
		color_component[i] = psd_stream_get_char(context);
	psd_color_space_to_argb(&layer->sheet_color, psd_color_space_rgb, color_component);
	// the rest are zeros
	psd_stream_get_null(context, 4);

	return psd_status_done;
}

// Reference point (Photoshop 6.0)
static psd_status psd_get_layer_reference_point(psd_context * context, psd_layer_record * layer)
{
	// 2 double values for the reference point
	layer->reference_point_x = (psd_int)psd_stream_get_double(context);
	layer->reference_point_y = (psd_int)psd_stream_get_double(context);

	return psd_status_done;
}

// Layer version (Photoshop 7.0)
static psd_status psd_get_layer_version(psd_context * context, psd_layer_record * layer)
{
	// A 32-bit number representing the version of Photoshop needed to read
	// and interpret the layer without data loss. 70 = 7.0, 80 = 8.0, etc.
	// NOTE: The minimum value is 70, because just having the field present in
	// 6.0 triggers a warning. For the future, Photoshop 7 checks to see
	// whether this number is larger than the current version -- i.e., 70 --
	// and if so, warns that it is ignoring some data.
	layer->layer_version = psd_stream_get_int(context);

	return psd_status_done;
}

// Transparency shapes layer (Photoshop 7.0)
static psd_status psd_get_layer_transparency_shapes_layer(psd_context * context, psd_layer_record * layer)
{
	// 1: the transparency of the layer is used in determining the shape of the
	// effects. This is the default for behavior like previous versions.
	// 0: treated in the same way as fill opacity including modulating blend
	// modes, rather than acting as strict transparency.
	// Using this feature is useful for achieving effects that otherwise would
	// require complex use of clipping groups.
	layer->transparency_shapes_layer = psd_stream_get_bool(context);

	// Padding
	psd_stream_get_null(context, 3);

	return psd_status_done;
}

// Layer mask as global mask (Photoshop 7.0)
static psd_status psd_get_layer_layer_mask_hides_effects(psd_context * context, psd_layer_record * layer)
{
	// 1: the layer mask is used in a final crossfade masking the layer and
	// effects rather than being used to shape the layer and its effects.
	// This behavior was previously tied to the link status flag for the layer mask.
	// (An unlinked mask acted like a flag value of 1, a linked mask like 0). For
	// old files that lack this key, the link status is used in order to preserve
	// compositing results.
	layer->layer_mask_hides_effects = psd_stream_get_bool(context);

	// Padding
	psd_stream_get_null(context, 3);

	return psd_status_done;
}

// Vector mask as global mask (Photoshop 7.0)
static psd_status psd_get_layer_vector_mask_hides_effects(psd_context * context, psd_layer_record * layer)
{
	// 1: the vector mask is used in a final crossfade masking the vector and
	// effects rather than being used to shape the vector and its effects.
	// This behavior was previously tied to the link status flag for the vector mask.
	// (An unlinked mask acted like a flag value of 1, a linked mask like 0). For
	// old files that lack this key, the link status is used in order to preserve
	// compositing results.
	layer->vector_mask_hides_effects = psd_stream_get_bool(context);

	// Padding
	psd_stream_get_null(context, 3);

	return psd_status_done;
}

// fill opacity
static psd_status psd_get_layer_fill_opacity(psd_context * context, psd_layer_record * layer)
{
	layer->fill_opacity = psd_stream_get_char(context);

	// Padding
	psd_stream_get_null(context, 3);

	return psd_status_done;
}

// Section divider setting (Photoshop 6.0)
static psd_status psd_get_layer_section_divider(psd_context * context, psd_layer_record * layer, psd_int size)
{
	// Type. 4 possible values, 0 = any other type of layer, 1 = open ※folder§, 2 =
	// closed ※folder§, 3 = bounding section divider, hidden in the UI
	layer->divider_type = psd_stream_get_int(context);
	switch(layer->divider_type)
	{
		case 1:
		case 2:
			layer->layer_type = psd_layer_type_folder;
			break;
		case 3:
			layer->layer_type = psd_layer_type_hidden;
			break;
	}

	// Following is only present if length = 12
	if(size == 12)
	{
		// Signature: '8BIM'
		if(psd_stream_get_int(context) != '8BIM')
			return psd_status_divider_signature_error;

		// blend mode
		layer->divider_blend_mode = psd_stream_get_blend_mode(context);
	}

	return psd_status_done;
}

// Channel blending restrictions setting (Photoshop 6.0)
static psd_status psd_get_layer_channel_blending_restrictions(psd_context * context, psd_layer_record * layer, psd_int size)
{
	psd_int i, j, channel_number;

	// Following is repeated length / 4 times.
	for(i = 0; i < size / 4; i ++)
	{
		// Channel number that is restricted
		channel_number = psd_stream_get_int(context);
		for(j = 0; j < layer->number_of_channels; j ++)
		{
			if(layer->channel_info[j].channel_id == channel_number)
			{
				layer->channel_info[j].restricted = psd_true;
				break;
			}
		}
	}

	return psd_status_done;
}

// shows the high-level organization of the layer information.
static psd_status psd_get_layer_info(psd_context * context)
{
	psd_int length, extra_length, i, j, size;
	psd_int prev_stream_pos, prev_layer_stream_pos, extra_stream_pos;
	psd_bool skip_first_alpha = psd_false;
	psd_layer_record * layer, * group_layer;
	psd_uchar flags;
	psd_uint tag;
	psd_status status = psd_status_done;

	// Length of the layers info section. (**PSB** length is 8 bytes.)
	length = psd_stream_get_int(context);
	// rounded up to a multiple of 2
	if(length & 0x01)
		length ++;
	if(length <= 0)
		return psd_status_done;

	prev_layer_stream_pos = context->stream.current_pos;

	// Layer count. 
	context->layer_count = psd_stream_get_short(context);
	// If it is a negative number, its absolute value is the number of
	// layers and the first alpha channel contains the transparency data for the
	// merged result.
	if(context->layer_count < 0)
	{
		skip_first_alpha = psd_true;
		context->layer_count = -context->layer_count;
	}

	psd_assert(context->layer_count > 0);

	context->layer_records = (psd_layer_record *)psd_malloc(context->layer_count * sizeof(psd_layer_record));
	if(context->layer_records == NULL)
		return psd_status_malloc_failed;
	memset(context->layer_records, 0, context->layer_count * sizeof(psd_layer_record));

	for(i = 0, layer = context->layer_records; i < context->layer_count; i ++, layer ++)
	{
		// INFORMATION ABOUT EACH LAYER
		// value as default
		layer->layer_type = psd_layer_type_normal;
		layer->blend_mode = psd_blend_mode_normal;
		layer->opacity = 255;
		layer->fill_opacity = 255;
		layer->blend_clipped = psd_true;
		layer->blend_interior = psd_false;
		layer->knockout = 0;
		layer->transparency_shapes_layer = psd_true;
		layer->layer_mask_hides_effects = psd_false;
		layer->vector_mask_hides_effects = psd_false;
		layer->divider_blend_mode = psd_blend_mode_pass_through;
		layer->layer_mask_info.default_color = 255;
		layer->layer_mask_info.disabled = psd_true;
		
		// Rectangle containing the contents of the layer. Specified as top, left,
		// bottom, right coordinates
		layer->top = psd_stream_get_int(context);
		layer->left = psd_stream_get_int(context);
		layer->bottom = psd_stream_get_int(context);
		layer->right = psd_stream_get_int(context);

		layer->width = layer->right - layer->left;
		layer->height = layer->bottom - layer->top;
		// the size of layer size is 0.
		//psd_assert(layer->width > 0 && layer->height > 0);

		// Number of channels in the layer
		layer->number_of_channels = psd_stream_get_short(context);
		psd_assert(layer->number_of_channels > 0);
		
		layer->channel_info = (psd_channel_info *)psd_malloc(layer->number_of_channels * sizeof(psd_channel_info));
		if(layer->channel_info == NULL)
		{
			psd_layer_free(layer);
			return psd_status_malloc_failed;
		}

		// Channel information. Six bytes per channel, consisting of:
		// 2 bytes for Channel ID: 0 = red, 1 = green, etc.;
		// 每1 = transparency mask; 每2 = user supplied layer mask
		// 4 bytes for length of corresponding channel data. (**PSB** 8 bytes for
		// length of corresponding channel data.)
		for(j = 0; j < layer->number_of_channels; j ++)
		{
			layer->channel_info[j].channel_id = psd_stream_get_short(context);
			layer->channel_info[j].data_length = psd_stream_get_int(context);
			layer->channel_info[j].restricted = psd_false;
		}

		// Blend mode signature: '8BIM'
		tag = psd_stream_get_int(context);
		if(tag != '8BIM')
		{
			psd_layer_free(layer);
			return psd_status_blend_mode_signature_error;
		}

		// Blend mode key
		layer->blend_mode = psd_stream_get_blend_mode(context);

		// Opacity. 0 = transparent ... 255 = opaque
		layer->opacity = psd_stream_get_char(context);

		// Clipping: 0 = base, 1 = non每base
		layer->clipping = psd_stream_get_bool(context);

		// Flags
		flags = psd_stream_get_char(context);
		// bit 0 = transparency protected
		layer->transparency_protected = flags & 0x01;
		// bit 1 = visible
		layer->visible = (flags & (0x01 << 1)) > 0;
		layer->visible = psd_true - layer->visible;
		// bit 2 = obsolete
		layer->obsolete = (flags & (0x01 << 2)) > 0;
		// bit 3 = 1 for Photoshop 5.0 and later, tells if bit 4 has useful information
		if((flags & (0x01 << 3)) > 0)
		{
			// bit 4 = pixel data irrelevant to appearance of document
			layer->pixel_data_irrelevant = (flags & (0x01 << 4)) > 0;
		}

		// Filler (zero)
		flags = psd_stream_get_char(context);
		psd_assert(flags == 0);

		// Length of the extra data field ( = the total length of the next five fields).
		extra_length = psd_stream_get_int(context);
		extra_stream_pos = context->stream.current_pos;
		psd_assert(extra_length > 0);

		// LAYER MASK / ADJUSTMENT LAYER DATA
		// Size of the data: 36, 20, or 0. If zero, the following fields are not present
		size = psd_stream_get_int(context);
		psd_assert(size == 36 || size == 20 || size == 0);
		if(size > 0)
		{
			// Rectangle enclosing layer mask: Top, left, bottom, right
			layer->layer_mask_info.top = psd_stream_get_int(context);
			layer->layer_mask_info.left = psd_stream_get_int(context);
			layer->layer_mask_info.bottom = psd_stream_get_int(context);
			layer->layer_mask_info.right = psd_stream_get_int(context);
			layer->layer_mask_info.width = layer->layer_mask_info.right - layer->layer_mask_info.left;
			layer->layer_mask_info.height = layer->layer_mask_info.bottom - layer->layer_mask_info.top;

			// Default color. 0 or 255
			layer->layer_mask_info.default_color = psd_stream_get_char(context);
			psd_assert(layer->layer_mask_info.default_color == 0 ||
				layer->layer_mask_info.default_color == 255);

			// Flags
			flags = psd_stream_get_char(context);
			// bit 0 = position relative to layer
			layer->layer_mask_info.relative = flags & 0x01;
			// bit 1 = layer mask disabled
			layer->layer_mask_info.disabled = (flags & (0x01 << 1)) > 0;
			// bit 2 = invert layer mask when blending
			layer->layer_mask_info.invert = (flags & (0x01 << 2)) > 0;

			if(size == 20)
			{
				// Padding. Only present if size = 20. Otherwise the following is present
				psd_stream_get_short(context);
			}
			else
			{
				// Real Flags. Same as Flags information above.
				flags = psd_stream_get_char(context);
				// bit 0 = position relative to layer
				layer->layer_mask_info.relative = flags & 0x01;
				// bit 1 = layer mask disabled
				layer->layer_mask_info.disabled = (flags & (0x01 << 1)) > 0;
				// bit 2 = invert layer mask when blending
				layer->layer_mask_info.invert = (flags & (0x01 << 2)) > 0;

				// Real user mask background. 0 or 255. Same as Flags information above.
				layer->layer_mask_info.default_color = psd_stream_get_char(context);
				psd_assert(layer->layer_mask_info.default_color == 0 ||
					layer->layer_mask_info.default_color == 255);

				// Rectangle enclosing layer mask: Top, left, bottom, right.
				layer->layer_mask_info.top = psd_stream_get_int(context);
				layer->layer_mask_info.left = psd_stream_get_int(context);
				layer->layer_mask_info.bottom = psd_stream_get_int(context);
				layer->layer_mask_info.right = psd_stream_get_int(context);
				layer->layer_mask_info.width = layer->layer_mask_info.right - layer->layer_mask_info.left;
				layer->layer_mask_info.height = layer->layer_mask_info.bottom - layer->layer_mask_info.top;
			}
		}

		// LAYER BLENDING RANGES DATA
		// Length of layer blending ranges data
		size = psd_stream_get_int(context);

		// Composite gray blend source. Contains 2 black values followed by 2
		// white values. Present but irrelevant for Lab & Grayscale.
		layer->layer_blending_ranges.gray_black_src = psd_stream_get_short(context);
		layer->layer_blending_ranges.gray_white_src = psd_stream_get_short(context);
		// Composite gray blend destination range
		layer->layer_blending_ranges.gray_black_dst = psd_stream_get_short(context);
		layer->layer_blending_ranges.gray_white_dst = psd_stream_get_short(context);

		layer->layer_blending_ranges.number_of_blending_channels = (size - 8) / 8;
		if (layer->layer_blending_ranges.number_of_blending_channels <= 0)
		{
			psd_layer_free(layer);
			return psd_status_invalid_blending_channels;
		}
		
		layer->layer_blending_ranges.channel_black_src = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		layer->layer_blending_ranges.channel_white_src = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		layer->layer_blending_ranges.channel_black_dst = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		layer->layer_blending_ranges.channel_white_dst = (psd_ushort *)psd_malloc(layer->layer_blending_ranges.number_of_blending_channels * 2);
		if(layer->layer_blending_ranges.channel_black_src == NULL || 
			layer->layer_blending_ranges.channel_white_src == NULL ||
			layer->layer_blending_ranges.channel_black_dst == NULL ||
			layer->layer_blending_ranges.channel_white_dst == NULL)
		{
			psd_layer_free(layer);
			return psd_status_malloc_failed;
		}
		
		for(j = 0; j < layer->layer_blending_ranges.number_of_blending_channels; j ++)
		{
			// channel source range
			layer->layer_blending_ranges.channel_black_src[j] = psd_stream_get_short(context);
			layer->layer_blending_ranges.channel_white_src[j] = psd_stream_get_short(context);
			// channel destination range
			layer->layer_blending_ranges.channel_black_dst[j] = psd_stream_get_short(context);
			layer->layer_blending_ranges.channel_white_dst[j] = psd_stream_get_short(context);
		}

		// Layer name: Pascal string, padded to a multiple of 4 bytes.
		size = psd_stream_get_char(context);
		size = ((size + 1 + 3) & ~0x03) - 1;
		psd_stream_get(context, layer->layer_name, size);

		while(context->stream.current_pos - extra_stream_pos < extra_length)
		{
			// ADDITIONAL LAYER INFORMATION
			// Signature
			tag = psd_stream_get_int(context);
			if(tag != '8BIM')
				return psd_status_layer_information_signature_error;

			// Key: a 4-character code
			tag = psd_stream_get_int(context);
			// Length data below, rounded up to an even byte count.
			// (**PSB**, the following keys have a length count of 8 bytes: LMsk, Lr16,
			// Layr, Mt16, Mtrn, Alph.
			size = psd_stream_get_int(context);
			size = (size + 1) & ~0x01;
			prev_stream_pos = context->stream.current_pos;

			// Adjustment layer
			// Adjustment layers can have one of the following keys
			status = psd_status_done;
			switch(tag)
			{
				case 'levl':
					status = psd_get_layer_levels(context, layer, size);
					break;
				case 'curv':
					status = psd_get_layer_curves(context, layer, size);
					break;
				case 'brit':
					status = psd_get_layer_brightness_contrast(context, layer);
					break;
				case 'blnc':
					status = psd_get_layer_color_balance(context, layer);
					break;
				//case 'hue ':
					// Old Hue/saturation, Photoshop 4.0
					//break;
				case 'hue2':
					status = psd_get_layer_hue_saturation(context, layer);
					break;
				case 'selc':
					status = psd_get_layer_selective_color(context, layer);
					break;
				case 'thrs':
					status = psd_get_layer_threshold(context, layer);
					break;
				case 'nvrt':
					status = psd_get_layer_invert(context, layer);
					break;
				case 'post':
					status = psd_get_layer_posterize(context, layer);
					break;
				case 'mixr':
					status = psd_get_layer_channel_mixer(context, layer);
					break;
				case 'grdm':
					status = psd_get_layer_gradient_map(context, layer);
					break;
				case 'phfl':
					status = psd_get_layer_photo_filter(context, layer);
					break;
				case 'lrFX':
					status = psd_get_layer_effects(context, layer);
					break;
				case 'lfx2':
					status = psd_get_layer_effects2(context,layer);
					break;
				case 'tySh':
					status = psd_get_layer_type_tool(context, layer);
					break;
				//case 'TySh':
					// Type tool object setting (Photoshop 6.0)
					//break;
				case 'SoCo':
					status = psd_get_layer_solid_color(context, layer);
					break;
				case 'GdFl':
					status = psd_get_layer_gradient_fill(context, layer);
					break;
				case 'PtFl':
					status = psd_get_layer_pattern_fill(context, layer);
					break;
				case 'luni':
					status = psd_get_layer_unicode_name(context, layer);
					break;
				case 'lnsr':
					status = psd_get_layer_name_id(context, layer);
					break;
				case 'lyid':
					status = psd_get_layer_id(context, layer);
					break;
				case 'clbl':
					status = psd_get_layer_blend_clipped(context, layer);
					break;
				case 'infx':
					status = psd_get_layer_blend_interior(context, layer);
					break;
				case 'knko':
					status = psd_get_layer_knockout(context, layer);
					break;
				case 'lspf':
					status = psd_get_layer_protected(context, layer);
					break;
				case 'lclr':
					status = psd_get_layer_sheet_color(context, layer);
					break;
				case 'fxrp':
					status = psd_get_layer_reference_point(context, layer);
					break;
				case 'lyvr':
					status = psd_get_layer_version(context, layer);
					break;
				case 'tsly':
					status = psd_get_layer_transparency_shapes_layer(context, layer);
					break;
				case 'lmgm':
					status = psd_get_layer_layer_mask_hides_effects(context, layer);
					break;
				case 'vmgm':
					status = psd_get_layer_vector_mask_hides_effects(context, layer);
					break;
				case 'iOpa':
					status = psd_get_layer_fill_opacity(context, layer);
					break;
				case 'lsct':
					status = psd_get_layer_section_divider(context, layer, size);
					break;
				case 'brst':
					status = psd_get_layer_channel_blending_restrictions(context, layer, size);
					break;
#ifdef PSD_GET_PATH_RESOURCE
				case 'vmsk':
					status = psd_get_layer_vector_mask(context, layer, size);
					break;
#endif
				default:
					psd_stream_get_null(context, size);
					break;
			}

			if(status != psd_status_done)
			{
				psd_layer_free(layer);
				return status;
			}

			// Filler
			psd_stream_get_null(context, prev_stream_pos + size - context->stream.current_pos);

			psd_assert(layer->layer_info_count < psd_layer_info_type_count);
		}

		psd_assert(context->stream.current_pos - extra_stream_pos == extra_length);
	}
	
	for(i = 0, layer = context->layer_records; i < context->layer_count; i ++, layer ++)
	{
		// Channel image data. Contains one or more image data records for each layer.
		status = psd_get_layer_channel_image_data(context, layer);
		if(status != psd_status_done)
		{
			psd_layer_free(layer);
			return status;
		}
	}

	// Filler: zeros
	psd_stream_get_null(context, prev_layer_stream_pos + length - context->stream.current_pos);

	// group layer
	for(i = context->layer_count - 1, group_layer = NULL; i >= 0; i --)
	{
		layer = &context->layer_records[i];
		switch(layer->layer_type)
		{
			case psd_layer_type_normal:
				layer->group_layer = group_layer;
				break;
			case psd_layer_type_folder:
				group_layer = layer;
				break;
			case psd_layer_type_hidden:
				group_layer = NULL;
				break;
		}
	}

	return psd_status_done;
}

// Global layer mask info
static psd_status psd_get_mask_info(psd_context * context)
{
	psd_int length;
	psd_int prev_stream_pos;

	// Length of global layer mask info section.
	length = psd_stream_get_int(context);
	if(length == 0)
		return psd_status_done;

	prev_stream_pos = context->stream.current_pos;

	// Overlay color space (undocumented).
	context->global_layer_mask.color = psd_stream_get_space_color(context);

	// Opacity. 0 = transparent, 100 = opaque.
	context->global_layer_mask.opacity = psd_stream_get_short(context);

	// Kind. 0 = Color selected〞i.e. inverted; 1 = Color protected;128 = use
	// value stored per layer. This value is preferred. The others are for
	// backward compatibility with beta versions.
	context->global_layer_mask.kind = psd_stream_get_char(context);

	// Filler: zeros
	psd_stream_get_null(context, prev_stream_pos + length - context->stream.current_pos);

	return psd_status_done;
}

// The fourth section of a Photoshop file contains information about layers and masks.
// This section of the document describes the formats of layer and mask records.
psd_status psd_get_layer_and_mask(psd_context * context)
{
	psd_int length, prev_stream_pos, extra_stream_pos, size;
	psd_status status;
	psd_uint tag;

	// Length of the layer and mask information section. (**PSB** length is 8 bytes.)
	length = psd_stream_get_int(context);
	if(length <= 0)
		return psd_status_done;
	
	prev_stream_pos = context->stream.current_pos;

	if(context->load_tag == psd_load_tag_merged || context->load_tag == psd_load_tag_thumbnail || 
		context->load_tag == psd_load_tag_exif)
	{
		psd_stream_get_null(context, length);
		return psd_status_done;
	}

	// Layer info
	status = psd_get_layer_info(context);
	
	psd_freeif(context->temp_image_data);
	context->temp_image_data = NULL;
	context->temp_image_length = 0;
	psd_freeif(context->temp_channel_data);
	context->temp_channel_data = NULL;
	context->temp_channel_length = 0;
	
	if(status != psd_status_done)
		return status;

	// Global layer mask info
	status = psd_get_mask_info(context);

	while(prev_stream_pos + length - context->stream.current_pos > 12)
	{
		tag = psd_stream_get_int(context);
		if(tag == '8BIM')
		{
			tag = psd_stream_get_int(context);
			if (tag == 'Lr16')
			{
				status = psd_get_layer_info(context);
				continue;
			}
			
			size = psd_stream_get_int(context);
			
			switch(tag)
			{
				case 'Patt':
				case 'Pat2':
					while(size >= 4)
					{
						extra_stream_pos = context->stream.current_pos;
						status = psd_get_pattern(context);
						size -= context->stream.current_pos - extra_stream_pos;
					}
					if(size > 0)
						psd_stream_get_null(context, size);
					break;
				default:
					if(size > 0)
						psd_stream_get_null(context, size);
					break;
			}
		}
		else
		{
			break;
		}
	}
	
	// Filler: zeros
	psd_stream_get_null(context, prev_stream_pos + length - context->stream.current_pos);

	return status;
}

static void psd_layer_free(psd_layer_record * layer)
{
	psd_int i;
	
	for(i = 0; i < layer->layer_info_count; i ++)
	{
		if(layer->layer_info_data[i] != 0)
		{
			switch(layer->layer_info_type[i])
			{
				case psd_layer_info_type_levels:
					psd_layer_levels_free(layer->layer_info_data[i]);
					break;
				case psd_layer_info_type_curves:
					psd_layer_curves_free(layer->layer_info_data[i]);
					break;
				case psd_layer_info_type_gradient_map:
					psd_layer_gradient_map_free(layer->layer_info_data[i]);
					break;
				case psd_layer_info_type_type_tool:
					psd_layer_type_tool_free(layer->layer_info_data[i]);
					break;
				case psd_layer_info_type_effects:
				case psd_layer_info_type_effects2:
					psd_layer_effects_free(layer->layer_info_data[i]);
					break;
				default:
					psd_freeif((void *)layer->layer_info_data[i]);
					break;
			}
			layer->layer_info_data[i] = 0;
		}
	}
	layer->layer_info_count = 0;

	psd_freeif(layer->channel_info);
	layer->channel_info = NULL;

	psd_freeif(layer->unicode_name);
	layer->unicode_name = NULL;

	psd_freeif(layer->image_data);
	layer->image_data = NULL;
	psd_freeif(layer->layer_mask_info.mask_data);
	layer->layer_mask_info.mask_data = NULL;

	psd_freeif(layer->layer_blending_ranges.channel_black_src);
	layer->layer_blending_ranges.channel_black_src = NULL;
	psd_freeif(layer->layer_blending_ranges.channel_white_src);
	layer->layer_blending_ranges.channel_white_src = NULL;
	psd_freeif(layer->layer_blending_ranges.channel_black_dst);
	layer->layer_blending_ranges.channel_black_dst = NULL;
	psd_freeif(layer->layer_blending_ranges.channel_white_dst);
	layer->layer_blending_ranges.channel_white_dst = NULL;

#ifdef PSD_GET_PATH_RESOURCE
	psd_layer_vector_mask_free(layer);
#endif
}

void psd_layer_and_mask_free(psd_context * context)
{
	psd_int i;
	
	for(i = 0; i < context->layer_count; i ++)
		psd_layer_free(&context->layer_records[i]);
	
	psd_freeif(context->layer_records);
	psd_freeif(context->temp_channel_data);
	psd_pattern_free(context);
}

