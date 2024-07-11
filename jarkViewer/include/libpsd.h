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
 * $Id: libpsd.h, created by Patrick in 2006.05.18, libpsd@graphest.com Exp $
 */

#ifndef __LIB_PSD_H__
#define __LIB_PSD_H__

#include "psd_types.h"


#ifdef __cplusplus
extern "C" {
#endif


// global status
typedef enum {
	psd_status_done								= 0,
	psd_status_invalid_context					= -1,
	psd_status_invalid_file						= -2,
	psd_status_unkown_error						= -3,
	psd_status_malloc_failed					= -4,
	psd_status_fread_error						= -5,
	psd_status_unsupport_yet					= -6,
	psd_status_file_header_error				= -10,
	psd_status_color_mode_data_error			= -11,
	psd_status_image_resource_error				= -12,
	psd_status_layer_and_mask_error 			= -13,
	psd_status_image_data_error					= -14,
	psd_status_unsupport_color_space			= -15,
	//psd_status_donot_support_multichannel		= -16,
	//psd_status_donot_support_duotone			= -17,
	psd_status_unsupport_color_depth			= -18,
	psd_status_unknown_color_mode				= -19,
	psd_status_unsupport_image_compression		= -20,
	psd_status_invalid_layer					= -21,
	psd_status_invalid_adjustment_layer			= -22,
	psd_status_invalid_layer_effects			= -23,
	psd_status_invalid_bitmap					= -50,
	psd_status_bitmap_dismatch_size				= -51,
	psd_status_invalid_layer_mask_info			= -52,
	psd_status_invalid_range					= -53,
	psd_status_invalid_gradient_color			= -54,
	psd_status_invalid_rect						= -55,
	psd_status_invalid_blending_channels		= -56,
	psd_status_blend_empty_rect					= -100,
	psd_status_unzip_error						= -101,
	psd_status_file_signature_error				= -200,
	psd_status_file_version_error				= -201,
	psd_status_resource_signature_error			= -400,
	psd_status_thumbnail_decode_error			= -401,
	psd_status_blend_mode_signature_error		= -500,
	psd_status_unsupport_blend_mode				= -501,
	psd_status_additional_layer_signature_error = -502,
	psd_status_levels_unsupport_version			= -503,
	psd_status_unsupport_layer_type				= -504,
	psd_status_extra_levels_key_error			= -505,
	psd_status_levels_dismatch_data_length		= -506,
	psd_status_extra_levels_unsupport_version	= -507,
	psd_status_curves_unsupport_version			= -508,
	psd_status_extra_curves_key_error			= -509,
	psd_status_extra_curves_unsupport_version	= -510,
	psd_status_hue_saturation_unsupport_version	= -511,
	psd_status_selective_color_unsupport_version= -512,
	psd_status_channel_mixer_unsupport_version	= -513,
	psd_status_photo_filter_unsupport_version	= -514,
	psd_status_gradient_map_unsupport_version	= -515,
	psd_status_effects_unsupport_version		= -516,
	psd_status_effects_signature_error			= -517,
	psd_status_unsupport_effects_type			= -518,
	psd_status_common_state_unsupport_version	= -519,
	psd_status_shadow_unsupport_version			= -520,
	psd_status_outer_glow_unsupport_version		= -521,
	psd_status_inner_glow_unsupport_version		= -522,
	psd_status_bevel_unsupport_version			= -523,
	psd_status_solid_fill_unsupport_version		= -524,
	psd_status_solid_color_unsupport_version	= -525,
	psd_status_gradient_fill_unsupport_version	= -526,
	psd_status_layer_information_signature_error= -528,
	psd_status_divider_signature_error			= -529,
	psd_status_pattern_fill_unsupport_version	= -530,
	psd_status_pattern_unsupport_version		= -531,
} psd_status;

typedef enum {
	psd_load_tag_all,
	psd_load_tag_header,
	psd_load_tag_layer,
	psd_load_tag_merged,
	psd_load_tag_thumbnail,
	psd_load_tag_exif
} psd_load_tag;

// color mode
typedef enum {
	psd_color_mode_bitmap 		= 0,
	psd_color_mode_grayscale 	= 1,
	psd_color_mode_indexed		= 2,
	psd_color_mode_rgb			= 3,
	psd_color_mode_cmyk			= 4,
	psd_color_mode_multichannel	= 7,
	psd_color_mode_duotone		= 8,
	psd_color_mode_lab			= 9
} psd_color_mode;

// color space
typedef enum {
	psd_color_space_dummy		= -1,
	psd_color_space_rgb,
	psd_color_space_hsb,
	psd_color_space_cmyk,
	psd_color_space_pantone,
	psd_color_space_focoltone,
	psd_color_space_trumatch,
	psd_color_space_toyo,
	psd_color_space_lab,
	psd_color_space_gray,
	psd_color_space_wide_cmyk,
	psd_color_space_hks,
	psd_color_space_dic,
	psd_color_space_total_ink,
	psd_color_space_monitor_rgb,
	psd_color_space_duotone,
	psd_color_space_opacity
} psd_color_space;

typedef enum {
	psd_layer_type_normal,
	psd_layer_type_hidden,
	psd_layer_type_folder,
	psd_layer_type_solid_color,
	psd_layer_type_gradient_fill,
	psd_layer_type_pattern_fill,
	psd_layer_type_levels,
	psd_layer_type_curves,
	psd_layer_type_brightness_contrast,
	psd_layer_type_color_balance,
	psd_layer_type_hue_saturation,
	psd_layer_type_selective_color,
	psd_layer_type_threshold,
	psd_layer_type_invert,
	psd_layer_type_posterize,
	psd_layer_type_channel_mixer,
	psd_layer_type_gradient_map,
	psd_layer_type_photo_filter,
} psd_layer_type;

// blend mode key
typedef enum {
	psd_blend_mode_normal,			// 'norm' = normal
	psd_blend_mode_dissolve,		// 'diss' = dissolve
	psd_blend_mode_darken,			// 'dark' = darken
	psd_blend_mode_multiply,		// 'mul ' = multiply
	psd_blend_mode_color_burn,		// 'idiv' = color burn
	psd_blend_mode_linear_burn,		// 'lbrn' = linear burn
	psd_blend_mode_lighten,			// 'lite' = lighten
	psd_blend_mode_screen,			// 'scrn' = screen
	psd_blend_mode_color_dodge,		// 'div ' = color dodge
	psd_blend_mode_linear_dodge,	// 'lddg' = linear dodge
	psd_blend_mode_overlay,			// 'over' = overlay
	psd_blend_mode_soft_light,		// 'sLit' = soft light
	psd_blend_mode_hard_light,		// 'hLit' = hard light
	psd_blend_mode_vivid_light,		// 'vLit' = vivid light
	psd_blend_mode_linear_light,	// 'lLit' = linear light
	psd_blend_mode_pin_light,		// 'pLit' = pin light
	psd_blend_mode_hard_mix,		// 'hMix' = hard mix
	psd_blend_mode_difference,		// 'diff' = difference
	psd_blend_mode_exclusion,		// 'smud' = exclusion
	psd_blend_mode_hue,				// 'hue ' = hue
	psd_blend_mode_saturation,		// 'sat ' = saturation
	psd_blend_mode_color,			// 'colr' = color
	psd_blend_mode_luminosity,		// 'lum ' = luminosity
	psd_blend_mode_pass_through,			// 'pass' = pass
} psd_blend_mode;

// There are several types of layer information that have been added in Photoshop 4.0
// and later. These exist at the end of the layer records structure
typedef enum {
	psd_layer_info_type_normal,
	psd_layer_info_type_levels,				// 'levl' = Levels
	psd_layer_info_type_curves,				// 'curv' = Curves
	psd_layer_info_type_brightness_contrast,// 'brit' = Brightness/contrast
	psd_layer_info_type_color_balance,		// 'blnc' = Color balance
	//psd_layer_info_type_old_hue_saturation,// 'hue ' = Old Hue/saturation, Photoshop 4.0
	psd_layer_info_type_new_hue_saturation,	// 'hue2' = New Hue/saturation, Photoshop 5.0
	psd_layer_info_type_selective_color,	// 'selc' = Selective color
	psd_layer_info_type_threshold,			// 'thrs' = Threshold
	psd_layer_info_type_invert,				// 'nvrt' = Invert
	psd_layer_info_type_posterize,			// 'post' = Posterize
	psd_layer_info_type_channel_mixer,		// Key is 'mixr'.
	psd_layer_info_type_gradient_map,		// Key is 'grdm'.
	psd_layer_info_type_photo_filter,		// Key is 'phfl'.
	psd_layer_info_type_type_tool,			// 'tySh'. Type Tool Info (Photoshop 5.0 and 5.5 only)
	psd_layer_info_type_type_tool_object,	// 'TySh'. Type tool object setting (Photoshop 6.0)
	psd_layer_info_type_effects,			// Effects Layer (Photoshop 5.0). The key for the effects layer is 'lrFX'.
	psd_layer_info_type_effects2,			// 'lfx2', Photoshop 6.0
	psd_layer_info_type_solid_color,		// Key is 'SoCo'
	psd_layer_info_type_gradient_fill,		// Key is 'GdFl'
	psd_layer_info_type_pattern_fill,		// Key is 'PtFl'
} psd_layer_info_type;
#define psd_layer_info_type_count			(psd_layer_info_type_pattern_fill + 1)

typedef enum {
	psd_gradient_style_linear,				// 'Lnr '
	psd_gradient_style_radial,				// 'Rdl '
	psd_gradient_style_angle,				// 'Angl'
	psd_gradient_style_reflected,			// 'Rflc'
	psd_gradient_style_diamond				// 'Dmnd'
} psd_gradient_style;

typedef enum {
	psd_color_stop_type_foreground_color,	// 'FrgC'
	psd_color_stop_type_background_Color,	// 'BckC'
	psd_color_stop_type_user_stop			// 'UsrS'
} psd_color_stop_type;

typedef enum {
	psd_layer_effects_type_drop_shadow,
	psd_layer_effects_type_inner_shadow,
	psd_layer_effects_type_outer_glow,
	psd_layer_effects_type_inner_glow,
	psd_layer_effects_type_satin,
	psd_layer_effects_type_color_overlay,
	psd_layer_effects_type_gradient_overlay,
	psd_layer_effects_type_pattern_overlay,
	psd_layer_effects_type_stroke,
	psd_layer_effects_type_bevel_emboss,
} psd_layer_effects_type;
#define psd_layer_effects_type_count		(psd_layer_effects_type_bevel_emboss + 1)

typedef enum {
	psd_layer_effects_bevel_emboss_outer_shadow = psd_layer_effects_type_bevel_emboss,
	psd_layer_effects_bevel_emboss_outer_light,
	psd_layer_effects_bevel_emboss_inner_shadow,
	psd_layer_effects_bevel_emboss_inner_light,
	psd_layer_effects_bevel_emboss_texture
} psd_layer_effects_bevel_emboss_type;

#define psd_layer_effects_image_count		(psd_layer_effects_bevel_emboss_texture + 1)

typedef enum {
	psd_technique_softer,
	psd_technique_precise,
	psd_technique_slope_limit,
} psd_technique_type;

typedef enum {
	psd_stroke_outside,
	psd_stroke_inside,
	psd_stroke_center
} psd_stroke_position;

typedef enum {
	psd_fill_solid_color,
	psd_fill_gradient,
	psd_fill_pattern,
} psd_fill_type;

typedef enum {
	psd_glow_center,
	psd_glow_edge,
} psd_glow_source;

typedef enum {
	psd_bevel_outer_bevel,
	psd_bevel_inner_bevel,
	psd_bevel_emboss,
	psd_bevel_pillow_emboss,
	psd_bevel_stroke_emboss,
} psd_bevel_style;

typedef enum {
	psd_direction_up,
	psd_direction_down
} psd_direction;

typedef enum {
	psd_units_inches 	= 1,
	psd_units_cm 		= 2,
	psd_units_points	= 3,
	psd_units_picas		= 4,
	psd_units_columns	= 5,
} psd_units;

typedef enum {
	psd_print_centered		= 0,
	psd_print_size_to_fit 	= 1,
	psd_print_user_defined	= 2,
} psd_print_style;

typedef psd_int(*psd_stream_get_func_ptr)(void * context, psd_uchar * buffer, psd_int length);
typedef psd_int(*psd_stream_get_null_func_ptr)(void * context, psd_int length);
typedef void(*psd_stream_free_func_ptr)(void * context);

typedef struct _psd_stream
{
	psd_uchar *					buffer;
	psd_int						read_in_length;
	psd_int						read_out_length;
	psd_int						file_length;
	psd_int						current_pos;

    psd_stream_get_func_ptr         stream_get;
    psd_stream_get_null_func_ptr    stream_get_null;
    psd_stream_free_func_ptr        stream_free;

} psd_stream;


/*********************************************************************************/
/*********************************************************************************/
// image resources section

// This structure contains information about the resolution of an image. It is
// written as an image resource. See the Document file formats chapter for more
// details.
typedef struct _psd_resolution_info
{
	psd_float					hres;			// Horizontal resolution in pixels per inch.
	psd_short					hres_unit;		// 1=display horitzontal resolution in pixels per inch; 2=display horitzontal resolution in pixels per cm.
	psd_short					width_unit;		// Display width as 1=inches; 2=cm; 3=points; 4=picas; 5=columns.
	psd_float					vres;			// Vertial resolution in pixels per inch.
	psd_short					vres_unit;		// 1=display vertical resolution in pixels per inch; 2=display vertical resolution in pixels per cm.
	psd_short					height_unit;	// Display height as 1=inches; 2=cm; 3=points; 4=picas; 5=columns.
} psd_resolution_info;

// This structure contains display information about each channel. It is written
// as an image resource.
typedef struct _psd_display_info
{
	psd_argb_color				color;
	psd_short 					opacity; 		// 0..100
	psd_uchar 					kind; 			// selected = 0, protected = 1
} psd_display_info;

typedef struct _psd_alpha_channel_info
{
	psd_char					name[256];
	psd_int						unicode_name_length;
	psd_ushort *				unicode_name;
	psd_int						identifier;
	psd_uchar *					channel_data;
} psd_alpha_channel_info;

// Grid and guides information
// Photoshop stores grid and guides information for an image in an image resource
// block. Each of these resource blocks consists of an initial 16-byte grid and guide
// header, which is always present, followed by 5-byte blocks of specific guide
// information for guide direction and location, which are present if there are guides
typedef struct _psd_grid_guides
{
	psd_int						horz_grid;		// Future implementation of document-specific grids (4 bytes horizontal, 4 bytes vertical).
	psd_int						vert_grid;
	psd_int						guide_count;	// Number of guide resource blocks (can be 0).
	psd_int *					guide_coordinate;//Location of guide in document coordinates. Since the guide is either vertical or horizontal, this only has to be one component of the coordinate.
	psd_uchar *					guide_direction;// Direction of guide. VHSelect is a system type of psd_uchar where 0 = vertical, 1 = horizontal.
} psd_grid_guides;

// Thumbnail resource format
// Adobe Photoshop (version 5.0 and later) stores thumbnail information for preview
// display in an image resource block that consists of an initial 28-byte header, followed
// by a JFIF thumbnail in RGB (red, green, blue) order for both Macintosh and Windows.
typedef struct _psd_thumbnail_resource
{
	psd_int						format;			// 1 = kJpegRGB . Also supports kRawRGB (0).
	psd_int						width;			// Width of thumbnail in pixels.
	psd_int						height;			// Height of thumbnail in pixels.
	psd_int						width_bytes;	// Padded row bytes = (width * bits per pixel + 31) / 32 * 4.
	psd_int						total_size;		// Total size = widthbytes * height * planes
	psd_int						size_after_compression;// Used for consistency check.
	psd_short					bits_per_pixel;	// 24
	psd_short					number_of_planes;// 1
	psd_uchar *					jfif_data;
	psd_argb_color *			thumbnail_data;
} psd_thumbnail_resource;

// (Photoshop 6.0) Version Info
// 4 bytes version, 1 byte hasRealMergedData, Unicode string: writer
// name, Unicode string: reader name, 4 bytes file version.
typedef struct _psd_version_info
{
	psd_uint					version;
	psd_bool					has_real_merged_data;
	psd_int						writer_name_length;
	psd_ushort *				writer_name;
	psd_int						reader_name_length;
	psd_ushort *				reader_name;
	psd_uint					file_version;
} psd_version_info;

// Slices resource block
typedef struct _psd_slices_resource_block
{
	psd_int						id;
	psd_int						group_id;
	psd_int						origin;
	psd_int						associated_layer_id;// NOTE: Only present if Origin = 1
	psd_int						type;
	psd_int						left;
	psd_int						top;
	psd_int						right;
	psd_int						bottom;
	psd_bool					cell_text_is_html;
	psd_int						horizontal_alignment;
	psd_int						veritcal_alignment;
	psd_argb_color				color;
} psd_slices_resource_block;

// Slices resource format
// Adobe Photoshop 6.0 and later stores slices information for an image in an image
// resource block. .
typedef struct _psd_slices_resource
{
	psd_int						bounding_top;	// Bounding rectangle for all of the slices: top, left, bottom, right of all the slices
	psd_int						bounding_left;
	psd_int						bounding_bottom;
	psd_int						bounding_right;
	psd_int						number_of_slices;// Number of slices to follow. See Slices resource block in the next table.
	psd_slices_resource_block * slices_resource_block;
} psd_slices_resource;

// Border information
// Contains , and 
typedef struct _psd_border_info {
	psd_float					border_width;	// fixed number (2 bytes real, 2 bytes fraction) for the border width
	psd_units					border_units;	// border units (1 = inches, 2 = cm, 3 = points, 4 = picas, 5 = columns).
} psd_border_info;

// Print flags
// A series of one-byte boolean values (see Page Setup dialog)
typedef struct _psd_print_flags {
	psd_bool					labels;
	psd_bool					crop_marks;
	psd_bool					color_bars;
	psd_bool					registration_marks;
	psd_bool					negative;
	psd_bool					flip;
	psd_bool					interpolate;
	psd_bool					caption;
	psd_bool					print_flags;
} psd_print_flags;

// (Photoshop 7.0) Print scale
typedef struct _psd_print_scale {
	psd_print_style				style;			// style (0 = centered, 1 = size to fit, 2 = user defined).
	psd_float					x_location;		// x location (floating point).
	psd_float					y_location;		// y location (floating point).
	psd_float					scale;			// scale (floating point)
} psd_print_scale;

// Print flags information
typedef struct _psd_print_flags_info {
	psd_bool					center_crop;	// center crop marks
	psd_int						value;			// bleed width value
	psd_short					scale;			// bleed width scale
} psd_print_flags_info;


// (Photoshop 6.0) URL List
typedef struct _psd_url_list_item
{
	psd_uint					tag;
	psd_uint					ID;
	psd_int						name_length;
	psd_ushort *				name;
} psd_url_list_item;

typedef struct _psd_url_list
{
	psd_int						number_of_urls;	// count of URLs
	psd_url_list_item *			items;
} psd_url_list;


// Color samplers resource format
typedef struct _psd_color_samplers_resource
{
	psd_int						vertical_position;		// The vertical position of the point
	psd_int						horizontal_position;	// The horizontal position of the point
	psd_color_space				color_space;
} psd_color_samplers_resource;

typedef struct _psd_color_samplers
{
	psd_int						number_of_color_samplers;	// Number of color samplers to follow.
	psd_color_samplers_resource *resource;
} psd_color_samplers;


// Path resource format
typedef struct _psd_bezier_point
{
	psd_bool					linked;
	psd_float					preceding_control_vertical;
	psd_float					preceding_control_horizontal;
	psd_float					anchor_point_vertical;
	psd_float					anchor_point_horizontal;
	psd_float					leaving_control_vertical;
	psd_float					leaving_control_horizontal;
} psd_bezier_point;

typedef struct _psd_subpath
{
	psd_bool					closed;
	psd_int						number_of_points;
	psd_bezier_point *			bezier_points;
} psd_subpath;

typedef struct _psd_path
{
	psd_int						number_of_subpaths;
	psd_subpath *				subpaths;
	psd_float					clipboard_top;
	psd_float					clipboard_left;
	psd_float					clipboard_bottom;
	psd_float					clipboard_right;
	psd_float					resolution;
	psd_bool					initial_fill;
} psd_path;


// end of image resources section
/*********************************************************************************/
/*********************************************************************************/


/*********************************************************************************/
/*********************************************************************************/
// layer and mask information section

// Channel information, Six bytes per channel
typedef struct _psd_channel_info
{
	psd_short					channel_id;		// 2 bytes for Channel ID: 0 = red, 1 = green, etc.; 每1 = transparency mask; 每2 = user supplied layer mask
	psd_int						data_length;	// 4 bytes for length of corresponding channel data. (**PSB** 8 bytes for length of corresponding channel data.)
	psd_bool					restricted;
} psd_channel_info;

// Layer mask data. Can be 40 bytes, 24 bytes, or 4 bytes if no layer mask.
typedef struct _psd_layer_mask_info
{
	psd_int						top;			// Rectangle enclosing layer mask: Top, left, bottom, right
	psd_int						left;
	psd_int						bottom;
	psd_int						right;
	psd_int						width;
	psd_int						height;
	psd_color_component			default_color;	// 0 or 255
	psd_bool					relative;		// position relative to layer
	psd_bool					disabled;		// layer mask disabled
	psd_bool					invert;			// invert layer mask when blending
	psd_color_component *		mask_data;
} psd_layer_mask_info;

// Layer blending ranges
typedef struct _psd_layer_blending_ranges
{
	psd_ushort					gray_black_src;	// Composite gray blend source. Contains 2 black values followed by 2 white values. Present but irrelevant for Lab & Grayscale.
	psd_ushort					gray_white_src;
	psd_ushort					gray_black_dst;	// Composite gray blend destination range
	psd_ushort					gray_white_dst;
	psd_int						number_of_blending_channels;
	psd_ushort *				channel_black_src;// channel source range
	psd_ushort *				channel_white_src;
	psd_ushort *				channel_black_dst;// First channel destination range
	psd_ushort *				channel_white_dst;
} psd_layer_blending_ranges;

// Vector mask setting (Photoshop 6.0)
typedef struct _psd_layer_vector_mask
{
	psd_path *					path;
	psd_bool					invert;				// Flags. bit 1 = invert, bit 2 = not link, bit 3 = disable
	psd_bool					not_link;
	psd_bool					disable;
} psd_layer_vector_mask;

// Information about each layer
typedef struct _psd_layer_record		psd_layer_record;
struct _psd_layer_record
{
	psd_layer_type				layer_type;
	psd_int						top;				// Rectangle containing the contents of the layer. Specified as top, left, bottom, right coordinates
	psd_int						left;
	psd_int						bottom;
	psd_int						right;
	psd_int						width;
	psd_int						height;
	psd_short					number_of_channels;	// Number of channels in the layer, including any alpha channels. Supported range is 1 to 56.
	psd_channel_info *			channel_info;		//Channel information
	psd_blend_mode				blend_mode;			// Blend mode key
	psd_uchar					opacity;			// 0 = transparent ... 255 = opaque
	psd_bool					clipping;			// 0 = base, 1 = non每base
	psd_bool					transparency_protected;
	psd_bool					visible;
	psd_bool					obsolete;
	psd_bool					pixel_data_irrelevant;// bit 3 = 1 for Photoshop 5.0 and later, tells if bit 4 has useful information; bit 4 = pixel data irrelevant to appearance of document
	psd_layer_mask_info			layer_mask_info;			// Layer mask data
	psd_layer_blending_ranges 	layer_blending_ranges;
	psd_uchar					layer_name[256];	// Pascal string, padded to a multiple of 4 bytes.
	psd_layer_vector_mask		vector_mask;
	
	psd_argb_color *			image_data;
	
	psd_int						layer_info_count;
	psd_layer_info_type			layer_info_type[psd_layer_info_type_count];
	psd_uint					layer_info_data[psd_layer_info_type_count];
	psd_bool					adjustment_valid;
	
	psd_uchar 					fill_opacity;
	psd_int						unicode_name_length;
	psd_ushort *				unicode_name;
	psd_uint					layer_name_id;
	psd_int						layer_id;
	psd_int						layer_version;
	psd_bool					blend_clipped;
	psd_bool					blend_interior;
	psd_bool					knockout;
	psd_bool					transparency;
	psd_bool					composite;
	psd_bool					position_respectively;
	psd_argb_color				sheet_color;
	psd_int						reference_point_x;
	psd_int						reference_point_y;
	psd_bool					transparency_shapes_layer;
	psd_bool					layer_mask_hides_effects;
	psd_bool					vector_mask_hides_effects;
	psd_int						divider_type;
	psd_blend_mode				divider_blend_mode;

	psd_layer_record *			group_layer;
};

// Global layer mask info
typedef struct _psd_global_layer_mask
{
	psd_argb_color				color;
	psd_short					opacity;
	psd_uchar					kind;
} psd_global_layer_mask;

// end of layer and mask information section
/*********************************************************************************/
/*********************************************************************************/


/*********************************************************************************/
/*********************************************************************************/
// additional layer information

// LEVELS
// Level record
typedef struct _psd_layer_level_record
{
	psd_ushort					input_floor;		// (0...253)
	psd_ushort					input_ceiling;		// (2...255)
	psd_ushort					output_floor;		// 255). Matched to input floor.
	psd_ushort					output_ceiling;		// (0...255)
	psd_float					gamma;				// Short integer from 10...999 representing 0.1...9.99. Applied to all image data.
} psd_layer_level_record;

// Levels settings files are loaded and saved in the Levels dialog.
typedef struct _psd_layer_levels
{
	psd_layer_level_record		record[29];			// 29 sets of level records, each level containing 5 psd_short integers
	// Photoshop CS (8.0) Additional information
	// At the end of the Version 2 file is the following information:
	psd_ushort					extra_level_count;	// Count of total level record structures. Subtract the legacy number of level record structures, 29, to determine how many are remaining in the file for reading.
	psd_layer_level_record *	extra_record;		// Additianol level records according to count
	psd_uchar					lookup_table[3][256];
} psd_layer_levels;


// CURVES
// The following is the data for each curve specified by count above
typedef struct _psd_layer_curves_data
{
	psd_ushort					channel_index;		// Before each curve is a channel index.
	psd_ushort					point_count;		// Count of points in the curve (psd_short integer from 2...19)
	psd_ushort					output_value[19];	// All coordinates have range 0 to 255
	psd_ushort					input_value[19];
} psd_layer_curves_data;

// Curves file format
typedef struct _psd_layer_curves
{
	psd_ushort					curve_count;		// Count of curves in the file.
	psd_layer_curves_data * 	curve;
	psd_uchar					lookup_table[3][256];
} psd_layer_curves;


// BRIGHTNESS AND CONTRAST
typedef struct _psd_layer_brightness_contrast
{
	psd_short					brightness;
	psd_short					contrast;
	psd_short					mean_value;			// for brightness and contrast
	psd_char					Lab_color;
	psd_uchar					lookup_table[256];
} psd_layer_brightness_contrast;


// COLOR BALANCE
typedef struct _psd_layer_color_balance
{
	psd_short					cyan_red[3];		// (-100...100). shadows, midtones, highlights
	psd_short					magenta_green[3];
	psd_short					yellow_blue[3];
	psd_bool					preserve_luminosity;
	psd_uchar 					lookup_table[3][256];
} psd_layer_color_balance;


// HUE/SATURATION
// Hue/Saturation settings files are loaded and saved in Photoshop＊s Hue/Saturation dialog
typedef struct _psd_layer_hue_saturation
{
	psd_uchar					hue_or_colorization;	// 0 = Use settings for hue-adjustment; 1 = Use settings for colorization.
	psd_short					colorization_hue;		// Photoshop 5.0: The actual values are stored for the new version. Hue is - 180...180, Saturation is 0...100, and Lightness is -100...100.
	psd_short					colorization_saturation;// Photoshop 4.0: Three psd_short integers Hue, Saturation, and Lightness from 每100...100.
	psd_short					colorization_lightness;	// The user interface represents hue as 每180...180, saturation as 0...100, and Lightness as -100...1000, as the traditional HSB color wheel, with red = 0.
	psd_short					master_hue;				// Master hue, saturation and lightness values.
	psd_short					master_saturation;
	psd_short					master_lightness;
	psd_short					range_values[6][4];		// For RGB and CMYK, those values apply to each of the six hextants in the HSB color wheel: those image pixels nearest to red, yellow, green, cyan, blue, or magenta. These numbers appear in the user interface from 每60...60, however the slider will reflect each of the possible 201 values from 每100...100.
	psd_short					setting_values[6][3];	// For Lab, the first four of the six values are applied to image pixels in the four Lab color quadrants, yellow, green, blue, and magenta. The other two values are ignored ( = 0). The values appear in the user interface from 每90 to 90.
	psd_uchar					lookup_table[6][360];
} psd_layer_hue_saturation;


// SELECTIVE  COLOR
// Selective Color settings files are loaded and saved in Photoshop＊s Selective Color dialog.
typedef struct _psd_layer_selective_color
{
	psd_ushort					correction_method;		// 0 = Apply color correction in relative mode; 1 = Apply color correction in absolute mode.
	psd_short					cyan_correction[10];	// Amount of cyan correction. Short integer from 每100...100.
	psd_short					magenta_correction[10];	// Amount of magenta correction. Short integer from 每100...100.
	psd_short					yellow_correction[10];	// Amount of yellow correction. Short integer from 每100...100.
	psd_short					black_correction[10]; 	// Amount of black correction. Short integer from 每100...100.
} psd_layer_selective_color;


// THRESHOLD
typedef struct _psd_layer_threshold
{
	psd_ushort					level;					// (1...255)
} psd_layer_threshold;


// INVERT
// no parameter


// POSTERIZE
typedef struct _psd_layer_posterize
{
	psd_ushort					levels;					// (2...255)
	psd_uchar					lookup_table[256];
} psd_layer_posterize;


// CHANNEL MIXER
typedef struct _psd_layer_channel_mixer
{
	psd_bool					monochrome;
	psd_short					red_cyan[4];			// RGB or CMYK color plus constant for the mixer settings. 4 * 2 bytes of color with 2 bytes of constant.
	psd_short					green_magenta[4];		// (-200...200)
	psd_short					blue_yellow[4];
	psd_short					black[4];
	psd_short					constant[4];
} psd_layer_channel_mixer;


// GRADIENT MAP
// Each color stop
typedef struct _psd_gradient_color_stop
{
	psd_int						location;				// Location of color stop
	psd_int						midpoint;				// Midpoint of color stop
	psd_argb_color				actual_color;
	psd_color_stop_type			color_stop_type;
} psd_gradient_color_stop;

// Each transparency stop
typedef struct _psd_gradient_transparency_stop
{
	psd_int						location;				// Location of transparency stop
	psd_int						midpoint;				// Midpoint of transparency stop
	psd_short					opacity;				// Opacity of transparency stop
} psd_gradient_transparency_stop;

// Gradient settings (Photoshop 6.0)
typedef struct _psd_layer_gradient_map
{
	psd_bool					reverse;				// Is gradient reverse
	psd_bool					dithered;				// Is gradient dithered
	psd_int						name_length;
	psd_ushort *				name;					// Name of the gradient: Unicode string, padded
	psd_short					number_color_stops;		// Number of color stops to follow
	psd_gradient_color_stop * 	color_stop;
	psd_short					number_transparency_stops;// Number of transparency stops to follow
	psd_gradient_transparency_stop * transparency_stop;
	psd_short					expansion_count;		// Expansion count ( = 2 for Photoshop 6.0)
	psd_short					interpolation;			// Interpolation if length above is non-zero
	psd_short					length;					// Length (= 32 for Photoshop 6.0)
	psd_short					mode;					// Mode for this gradient
	psd_int						random_number_seed;		// Random number seed
	psd_short					showing_transparency_flag;// Flag for showing transparency
	psd_short					using_vector_color_flag;// Flag for using vector color
	psd_int						roughness_factor;		// Roughness factor
	psd_argb_color				min_color;
	psd_argb_color				max_color;
	psd_argb_color				lookup_table[256];
} psd_layer_gradient_map;


// PHOTO FILTER
typedef struct _psd_layer_photo_filter
{
	psd_int						x_color;		// 4 bytes each for XYZ color
	psd_int						y_color;
	psd_int						z_color;
	psd_int						density;		// (1...100)
	psd_bool					preserve_luminosity;
} psd_layer_photo_filter;


// EFFECTS

typedef struct _psd_gradient_color
{
	psd_int						smoothness;
	psd_int						name_length;
	psd_ushort *				name;					// Name of the gradient: Unicode string, padded
	psd_short					number_color_stops;		// Number of color stops to follow
	psd_gradient_color_stop * 	color_stop;
	psd_short					number_transparency_stops;// Number of transparency stops to follow
	psd_gradient_transparency_stop *transparency_stop;
} psd_gradient_color;

typedef struct _psd_pattern
{
	psd_color_mode				color_mode;			// The image mode of the file.
	psd_short					height;				// Point: vertical, 2 bytes and horizontal, 2 bytes
	psd_short					width;
	psd_int						name_length;		// Name: Unicode string
	psd_ushort * 				name;
	psd_uchar					unique_id[256];		// Unique ID for this pattern: Pascal string
	psd_int						version;
	psd_short					top;				// Rectangle: top, left, bottom, right
	psd_short					left;
	psd_short					bottom;
	psd_short					right;
	psd_int						max_channel;		// Max channels
	psd_int						channel_number;
	psd_argb_color *			image_data;
} psd_pattern;

// Effects layer, drop shadow and inner shadow info
typedef struct _psd_layer_effects_drop_shadow
{
	psd_bool					effect_enable;		// Effect enabled
	
	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_argb_color				color;
	psd_argb_color				native_color;
	psd_uchar					opacity;			// Opacity as a percent
	psd_int						angle;				// Angle in degrees
	psd_bool					use_global_light;	// Use this angle in all of the layer effects
	psd_int						distance;			// Distance in pixels
	psd_int						spread;				// Intensity as a percent
	psd_int						size;				// Blur value in pixels

	psd_uchar					contour_lookup_table[256];
	psd_bool					anti_aliased;
	psd_int						noise;
	psd_bool					knocks_out;
} psd_layer_effects_drop_shadow;

typedef struct _psd_layer_effects_inner_shadow
{
	psd_bool					effect_enable;		// Effect enabled
	
	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_argb_color				color;
	psd_argb_color				native_color;
	psd_uchar					opacity;			// Opacity as a percent
	psd_int						angle;				// Angle in degrees
	psd_bool					use_global_light;	// Use this angle in all of the layer effects
	psd_int						distance;			// Distance in pixels
	psd_int						choke;				// Intensity as a percent
	psd_int						size;				// Blur value in pixels

	psd_uchar					contour_lookup_table[256];
	psd_bool					anti_aliased;
	psd_int						noise;
} psd_layer_effects_inner_shadow;

// Effects layer, outer glow info
typedef struct _psd_layer_effects_outer_glow
{
	psd_bool					effect_enable;		// Effect enabled
	
	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_uchar					opacity;			// Opacity as a percent
	psd_int						noise;
	psd_fill_type				fill_type;
	psd_argb_color				color;
	psd_argb_color				native_color;
	psd_gradient_color			gradient_color;

	psd_technique_type			technique;
	psd_int						spread;
	psd_int						size;
	
	psd_uchar					contour_lookup_table[256];
	psd_bool					anti_aliased;
	psd_int						range;
	psd_int						jitter;
} psd_layer_effects_outer_glow;

// Effects layer, inner glow info
typedef struct _psd_layer_effects_inner_glow
{
	psd_bool					effect_enable;		// Effect enabled
	
	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_uchar					opacity;			// Opacity as a percent
	psd_int						noise;
	psd_fill_type				fill_type;
	psd_argb_color				color;
	psd_argb_color				native_color;
	psd_gradient_color			gradient_color;

	psd_technique_type			technique;
	psd_glow_source				source;
	psd_int						choke;
	psd_int						size;
	
	psd_uchar					contour_lookup_table[256];
	psd_bool					anti_aliased;
	psd_int						range;
	psd_int						jitter;
} psd_layer_effects_inner_glow;

typedef struct _psd_pattern_info
{
	psd_int						name_length;
	psd_ushort *				name;
	psd_uchar					identifier[256];
} psd_pattern_info;

// Effects layer, bevel info
typedef struct _psd_layer_effects_bevel_emboss
{
	psd_bool					effect_enable;				// Effect enabled

	psd_bevel_style				style;						// Bevel style
	psd_technique_type			technique;
	psd_int						depth;
	psd_direction				direction;					// Up or down
	psd_int						size;						// Strength. Depth in pixels
	psd_int						soften;						// Blur value in pixels.
	
	psd_int						angle;						// Angle in degrees
	psd_bool					use_global_light;			// Use this angle in all of the layer effects
	psd_int						altitude;
	psd_uchar					gloss_contour_lookup_table[256];
	psd_bool					gloss_anti_aliased;
	psd_blend_mode				highlight_blend_mode;		// Highlight blend mode: 4 bytes for signature and 4 bytes for the key
	psd_argb_color				highlight_color;
	psd_argb_color				real_highlight_color;
	psd_uchar					highlight_opacity;			// Hightlight opacity as a percent
	psd_blend_mode				shadow_blend_mode;			// Shadow blend mode: 4 bytes for signature and 4 bytes for the key
	psd_argb_color				shadow_color;
	psd_argb_color				real_shadow_color;
	psd_uchar					shadow_opacity;				// Shadow opacity as a percent

	psd_bool					contour_enable;
	psd_uchar					contour_lookup_table[256];
	psd_bool					contour_anti_aliased;
	psd_int						contour_range;

	psd_bool					texture_enable;
	psd_pattern_info			texture_pattern_info;
	psd_int						texture_scale;
	psd_int						texture_depth;
	psd_bool					texture_invert;
	psd_bool					texture_link;
	psd_int						texture_horz_phase;
	psd_int						texture_vert_phase;
} psd_layer_effects_bevel_emboss;

typedef struct _psd_layer_effects_satin
{
	psd_bool					effect_enable;				// Effect enabled

	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_argb_color				color;
	psd_uchar					opacity;			// Opacity as a percent
	psd_int						angle;				// Angle in degrees
	psd_int						distance;
	psd_int						size;
	psd_uchar					contour_lookup_table[256];
	psd_bool					anti_aliased;
	psd_bool					invert;
} psd_layer_effects_satin;

// Effects layer, solid fill (added in Photoshop 7.0)
typedef struct _psd_layer_effects_color_overlay
{
	psd_bool					effect_enable;				// Effect enabled

	psd_blend_mode				blend_mode;			// Key for blend mode
	psd_argb_color				color;
	psd_uchar					opacity;			// Opacity as a percent
	psd_argb_color				native_color;
} psd_layer_effects_color_overlay;

typedef struct _psd_layer_effects_gradient_overlay
{
	psd_bool					effect_enable;				// Effect enabled

	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_uchar					opacity;			// Opacity as a percent
	psd_gradient_color			gradient_color;
	psd_bool					reverse;
	psd_gradient_style			style;
	psd_bool					align_width_layer;
	psd_int						angle;				// Angle in degrees
	psd_int						scale;
	psd_int						horz_offset;
	psd_int						vert_offset;
} psd_layer_effects_gradient_overlay;

typedef struct _psd_layer_effects_pattern_overlay
{
	psd_bool					effect_enable;				// Effect enabled

	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_argb_color				color;
	psd_uchar					opacity;			// Opacity as a percent
	psd_int						scale;
	psd_bool					link_with_layer;
	psd_pattern_info			pattern_info;
	psd_int						horz_phase;
	psd_int						vert_phase;
} psd_layer_effects_pattern_overlay;

typedef struct _psd_layer_effects_stroke
{
	psd_bool					effect_enable;				// Effect enabled

	psd_int						size;
	psd_stroke_position			position;
	psd_blend_mode				blend_mode;			// Blend mode: 4 bytes for signature and 4 bytes for key
	psd_uchar					opacity;			// Opacity as a percent
	psd_fill_type				fill_type;
	
	psd_argb_color				fill_color;

	psd_gradient_color			gradient_color;
	psd_bool					gradient_reverse;
	psd_gradient_style			gradient_style;
	psd_bool					gradient_align;
	psd_int						gradient_angle;
	psd_int						gradient_scale;
	psd_int						gradient_horz_offset;
	psd_int						gradient_vert_offset;

	psd_pattern_info			pattern_info;
	psd_int						pattern_scale;
	psd_bool					pattern_link;
	psd_int						pattern_horz_phase;
	psd_int						pattern_vert_phase;
} psd_layer_effects_stroke;

// Effects Layer info
typedef struct _psd_layer_effects
{
	psd_short							effects_count;	// Effects count: may be 6 (for the 6 effects in Photoshop 5 and 6) or 7 (for Photoshop 7.0)
	psd_bool							visible;		// common state info, visible: always true
	psd_layer_effects_drop_shadow 		drop_shadow;
	psd_layer_effects_inner_shadow 		inner_shadow;
	psd_layer_effects_outer_glow 		outer_glow;
	psd_layer_effects_inner_glow		inner_glow;
	psd_layer_effects_bevel_emboss		bevel_emboss;
	psd_layer_effects_satin				satin;
	psd_layer_effects_color_overlay		color_overlay;
	psd_layer_effects_gradient_overlay 	gradient_overlay;
	psd_layer_effects_pattern_overlay 	pattern_overlay;
	psd_layer_effects_stroke			stroke;

	psd_bool							fill[psd_layer_effects_type_count];
	psd_bool							valid[psd_layer_effects_type_count];
	psd_blend_mode						blend_mode[psd_layer_effects_image_count];
	psd_uchar							opacity[psd_layer_effects_image_count];
	psd_argb_color *					image_data[psd_layer_effects_image_count];
	psd_int								left[psd_layer_effects_image_count];
	psd_int								top[psd_layer_effects_image_count];
	psd_int								right[psd_layer_effects_image_count];
	psd_int								bottom[psd_layer_effects_image_count];
	psd_int								width[psd_layer_effects_image_count];
	psd_int								height[psd_layer_effects_image_count];
} psd_layer_effects;


// SOLID COLOR
typedef struct _psd_layer_solid_color
{
	psd_uint					id;
	psd_argb_color				fill_color;
} psd_layer_solid_color;


// GRADIENT FILL
typedef struct _psd_layer_gradient_fill
{
	psd_uint					id;
	psd_double					angle;
	psd_gradient_style			style;
	psd_int						scale;
	psd_bool					reverse;				// Is gradient reverse
	psd_bool					dithered;				// Is gradient dithered
	psd_bool					align_with_layer;
	psd_gradient_color			gradient_color;
} psd_layer_gradient_fill;


// PATTERN FILL
typedef struct _psd_layer_pattern_fill
{
	psd_uint					id;
	psd_pattern_info			pattern_info;
	psd_int						scale;
} psd_layer_pattern_fill;


// TYPE TOOL INFO (PHOTOSHOP 5.0 AND 5.5 ONLY)
typedef struct _psd_layer_type_face
{
	psd_short					mark;					// Mark value
	psd_int						font_type;				// Font type data
	psd_char					font_name[256];			// Pascal string of font name
	psd_char					font_family_name[256];	// Pascal string of font family name
	psd_char					font_style_name[256];	// Pascal string of font style name
	psd_short					script;					// Script value
	psd_int						number_axes_vector;		// Number of design axes vector to follow
	psd_int	*					vector;					// Design vector value
} psd_layer_type_face;

typedef struct _psd_layer_type_style
{
	psd_short					mark;					// Mark value
	psd_short					face_mark;				// Face mark value
	psd_int						size;					// Size value
	psd_int						tracking;				// Tracking value
	psd_int						kerning;				// Kerning value
	psd_int						leading;				// Leading value
	psd_int						base_shift;				// Base shift value
	psd_bool					auto_kern;				// Auto kern on/off
	psd_bool					rotate;					// Rotate up/down
} psd_layer_type_style;

typedef struct _psd_layer_type_line
{
	psd_int						char_count;				// Character count value
	psd_short					orientation;			// Orientation value
	psd_short					alignment;				// Alignment value
	psd_short					actual_char;			// Actual character as a double byte character
	psd_short					style;					// Style value
} psd_layer_type_line;

typedef struct _psd_layer_type_tool
{
	psd_double					transform_info[6];		// 6 * 8 double precision numbers for the transform information
	psd_short					faces_count;			// Count of faces
	psd_layer_type_face *		face;
	psd_short					styles_count;			// Count of styles
	psd_layer_type_style *		style;
	psd_short					type;					// Type value
	psd_int						scaling_factor;			// Scaling factor value
	psd_int						sharacter_count;		// Sharacter count value
	psd_int						horz_place;				// Horizontal placement
	psd_int						vert_place;				// Vertical placement
	psd_int						select_start;			// Select start value
	psd_int						select_end;				// Select end value
	psd_short					lines_count;			// Line count
	psd_layer_type_line *		line;
	psd_argb_color				color;
	psd_bool					anti_alias;				// Anti alias on/off
} psd_layer_type_tool;


// end of additional layer information
/*********************************************************************************/
/*********************************************************************************/

typedef struct _psd_context
{
    psd_char *					file_name_or_buffer;
	size_t						size;
   	void *						file;
	psd_stream					stream;
	psd_uint					state;
	psd_load_tag				load_tag;
	
	psd_int						width;
	psd_int						height;
	psd_ushort					channels;
	psd_ushort					depth;
	psd_color_mode				color_mode;

	psd_int						color_map_length;
	psd_argb_color *			color_map;

	psd_uchar					caption[256];

	psd_bool					fill_resolution_info;
	psd_resolution_info			resolution_info;
	
	psd_bool					fill_display_info;
	psd_display_info			display_info;

	psd_bool					fill_alpha_channel_info;
	psd_short					alpha_channels;
	psd_short					color_channels;
	psd_alpha_channel_info *	alpha_channel_info;

	psd_short					target_layer_index;

	psd_bool					fill_layer_group;
	psd_int						layer_group_count;
	psd_ushort *				layer_group_id;

	psd_bool					fill_thumbnail_resource;
	psd_thumbnail_resource		thumbnail_resource;

	psd_bool					fill_version_info;
	psd_version_info			version_info;

	psd_bool					copyright_flag;
	psd_int						global_angle;
	psd_int						global_altitude;
	psd_bool					effects_visible;
	psd_short					indexed_color_table_count;
	psd_short					transparency_index;

	psd_bool					fill_border_info;
	psd_border_info				border_info;

	psd_bool					fill_print_flags;
	psd_print_flags				print_flags;

	psd_bool					fill_grid_and_guides_info;
	psd_grid_guides 			grid_guides;

	psd_bool					fill_color_samplers;
	psd_color_samplers			color_samplers;

	psd_bool					fill_slices_resource;
	psd_slices_resource			slices_resource;

	psd_bool					fill_url_list;
	psd_url_list				url_list;
	
	psd_bool					fill_exif_data;
	psd_uchar *					exif_data;
	psd_int						exif_data_length;

	psd_bool					fill_XMP_metadata;
	psd_uchar *					XMP_metadata;
	psd_int						XMP_metadata_length;

	psd_bool					fill_print_scale;
	psd_print_scale				print_scale;

	psd_double					pixel_aspect_ratio;

	psd_bool					fill_print_flags_info;
	psd_print_flags_info		print_flags_info;

	psd_short					layer_count;
	psd_layer_record *			layer_records;

	psd_global_layer_mask		global_layer_mask;

	psd_int						malloc_pattern;
	psd_int						pattern_count;
	psd_pattern *				patterns;

	psd_int						malloc_path;
	psd_int						path_count;
	psd_path *					paths;

	psd_argb_color *			merged_image_data;
	psd_argb_color *			blending_image_data;

	// temporary data
	psd_uchar *					rand_data;
	psd_uchar *					temp_image_data;
	psd_int						temp_image_length;
	psd_uchar *					temp_channel_data;
	psd_int						temp_channel_length;
	psd_int						per_channel_length;
	psd_int						max_channel_length;
} psd_context;


psd_status psd_image_load(psd_context ** dst_context, psd_char * file_name);
psd_status psd_image_load_header(psd_context ** dst_context, psd_char * file_name);
psd_status psd_image_load_layer(psd_context ** dst_context, psd_char * file_name);
psd_status psd_image_load_merged(psd_context ** dst_context, psd_char * file_name);
psd_status psd_image_load_thumbnail(psd_context ** dst_context, psd_char * file_name);

psd_status psd_image_load_from_memory(psd_context ** dst_context, psd_char * file_name, size_t size);
psd_status psd_image_load_header_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size);
psd_status psd_image_load_layer_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size);
psd_status psd_image_load_merged_from_memory(psd_context ** dst_context, psd_char *buffer, size_t size);
psd_status psd_image_load_thumbnail_from_memory(psd_context ** dst_context, psd_char * buffer, size_t size);

psd_status psd_image_load_exif(psd_context ** dst_context, psd_char * file_name);
psd_status psd_image_free(psd_context * context);
psd_status psd_adjustment_layer_update(psd_layer_record * layer);
psd_status psd_layer_effects_update(psd_layer_record * layer, psd_layer_effects_type type);
psd_status psd_image_blend(psd_context * context, psd_int left, psd_int top, psd_int width, psd_int height);


#ifdef __cplusplus
}
#endif

#endif //ifndef __LIB_PSD_H__
