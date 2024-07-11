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
 * $Id: blend.c, created by Patrick in 2005.06.01, libpsd@graphest.com Exp $
 */

#include <stdlib.h>
#include <math.h>
#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_color.h"
#include "psd_rect.h"
#include "psd_blend.h"
#include "psd_math.h"


extern psd_bool psd_adjustment_layer_blend(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_effects_blend_background(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);
extern psd_bool psd_layer_effects_blend_foreground(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect);


#ifdef PSD_SUPPORT_LAYER_BLEND

static psd_status psd_build_rand_data(psd_context * context)
{
	psd_int i;
	psd_uchar * rand_data;

	context->rand_data = (psd_uchar *)psd_malloc(context->width * context->height);
	if(context->rand_data == NULL)
		return psd_status_malloc_failed;

	srand(0);
	rand_data = context->rand_data;
	for(i = context->width * context->height; i --; )
	{
		*rand_data = rand() & 0xFF;
		rand_data ++;
	}

	return psd_status_done;
}

static void psd_image_clear_rect(psd_context * context, psd_rect * dst_rect)
{
	psd_int i, width;
	psd_argb_color * dst_color;

	dst_color = context->blending_image_data + dst_rect->top * context->width + dst_rect->left;
	width = psd_rect_width(dst_rect);
	for(i = dst_rect->top; i < dst_rect->bottom; i ++)
	{
		psd_color_memset(dst_color, psd_color_clear, width);
		dst_color += context->width;
	}
}

psd_bool psd_layer_check_restricted(psd_context * context, psd_layer_record * layer)
{
	psd_int i;

	if(context->color_mode != psd_color_mode_rgb)
		return psd_false;

	if(layer->knockout != 0)
		return psd_true;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		if(layer->channel_info[i].restricted == psd_true)
			return psd_true;
	}

	if(layer->layer_blending_ranges.gray_black_src != 0 ||
		layer->layer_blending_ranges.gray_white_src != 65535 ||
		layer->layer_blending_ranges.gray_black_dst != 0 ||
		layer->layer_blending_ranges.gray_white_dst != 65535)
		return psd_true;

	for(i = 0; i < layer->layer_blending_ranges.number_of_blending_channels; i ++)
	{
		if(layer->layer_blending_ranges.channel_black_src[i] != 0 ||
			layer->layer_blending_ranges.channel_white_src[i] != 65535 ||
			layer->layer_blending_ranges.channel_black_dst[i] != 0 ||
			layer->layer_blending_ranges.channel_white_dst[i] != 65535)
			return psd_true;
	}
	
	return psd_false;
}

void psd_layer_blend_normal(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_argb_color * dst_data, * src_data, dst_color, src_color;
	psd_int i, j, width, height;
	psd_int opacity, mix_alpha;
	psd_int dst_red, dst_green, dst_blue, dst_alpha, src_alpha;
	psd_color_component * mask_data = NULL, mask_color, default_mask_color;
	psd_int mask_left, mask_right, dst_left;
	psd_layer_mask_info * layer_mask_info = &layer->layer_mask_info;

	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	opacity = layer->opacity * layer->fill_opacity / 255;
	if(layer->group_layer != NULL)
		opacity = opacity * layer->group_layer->opacity / 255;
	
	if(layer_mask_info->disabled == psd_false)
		default_mask_color = layer_mask_info->default_color;
	else
		default_mask_color = 255;
	mask_left = layer_mask_info->left;
	mask_right = layer_mask_info->right;
	dst_left = dst_rect->left;

	if(opacity == 255)
	{
		for(i = 0; i < height; i ++)
		{
			dst_data = context->blending_image_data + (i + dst_rect->top) * context->width + 
				dst_rect->left;
			src_data = layer->image_data + (i + dst_rect->top - layer->top) * layer->width + 
				(dst_rect->left - layer->left);
			
			if(layer_mask_info->disabled == psd_false && layer_mask_info->mask_data != NULL &&
				i + dst_rect->top >= layer_mask_info->top && i + dst_rect->top < layer_mask_info->bottom)
			{
				mask_data = layer_mask_info->mask_data + (i + dst_rect->top - layer_mask_info->top) * layer_mask_info->width + 
					PSD_MAX(0, dst_rect->left - layer_mask_info->left);
			}
			else
			{
				mask_data = NULL;
			}
			
			for(j = 0; j < width; j ++, dst_data ++, src_data ++)
			{
				src_color = *src_data;
				src_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
				
				if(mask_data != NULL && j + dst_left >= mask_left && j + dst_left < mask_right)
				{
					mask_color = *mask_data;
					mask_data ++;
					switch(mask_color)
					{
						case 0:
							continue;
						case 255:
							break;
						default:
							src_alpha = src_alpha * mask_color >> 8;
							break;
					}
				}
				else if(default_mask_color != 255)
				{
					switch(default_mask_color)
					{
						case 0:
							continue;
						case 255:
							break;
						default:
							src_alpha = src_alpha * default_mask_color >> 8;
							break;
					}
				}
				
				switch(src_alpha)
				{
					case 0:
						break;
					case 255:
						*dst_data = src_color;
						break;
					default:
						dst_color = *dst_data;
						dst_alpha = PSD_GET_ALPHA_COMPONENT(dst_color);
						dst_red = PSD_GET_RED_COMPONENT(dst_color);
						dst_green = PSD_GET_GREEN_COMPONENT(dst_color);
						dst_blue = PSD_GET_BLUE_COMPONENT(dst_color);
						switch(dst_alpha)
						{
							case 0:
								*dst_data = src_color;
								break;
							case 255:
								dst_red = ((dst_red << 8) + (PSD_GET_RED_COMPONENT(src_color) - dst_red) * src_alpha) >> 8;
								dst_green = ((dst_green << 8) + (PSD_GET_GREEN_COMPONENT(src_color) - dst_green) * src_alpha) >> 8;
								dst_blue = ((dst_blue << 8) + (PSD_GET_BLUE_COMPONENT(src_color) - dst_blue) * src_alpha) >> 8;
								*dst_data = PSD_RGB_TO_COLOR(dst_red, dst_green, dst_blue);
								break;
							default:
								mix_alpha = (src_alpha << 8) / (src_alpha + ((256 - src_alpha) * dst_alpha >> 8));
								dst_alpha = dst_alpha + ((256 - dst_alpha) * src_alpha >> 8);
								dst_red = ((dst_red << 8) + (PSD_GET_RED_COMPONENT(src_color) - dst_red) * mix_alpha) >> 8;
								dst_green = ((dst_green << 8) + (PSD_GET_GREEN_COMPONENT(src_color) - dst_green) * mix_alpha) >> 8;
								dst_blue = ((dst_blue << 8) + (PSD_GET_BLUE_COMPONENT(src_color) - dst_blue) * mix_alpha) >> 8;
								*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
								break;
						}
						break;
				}
			}
		}
	}
	else
	{
		for(i = 0; i < height; i ++)
		{
			dst_data = context->blending_image_data + (i + dst_rect->top) * context->width + 
				dst_rect->left;
			src_data = layer->image_data + (i + dst_rect->top - layer->top) * layer->width + 
				(dst_rect->left - layer->left);
			
			if(layer_mask_info->disabled == psd_false && layer_mask_info->mask_data != NULL &&
				i + dst_rect->top >= layer_mask_info->top && i + dst_rect->top < layer_mask_info->bottom)
			{
				mask_data = layer_mask_info->mask_data + (i + dst_rect->top - layer_mask_info->top) * layer_mask_info->width + 
					PSD_MAX(0, dst_rect->left - layer_mask_info->left);
			}
			else
			{
				mask_data = NULL;
			}
			
			for(j = 0; j < width; j ++, dst_data ++, src_data ++)
			{
				src_color = *src_data;
				src_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
				
				if(mask_data != NULL && j + dst_left >= mask_left && j + dst_left < mask_right)
				{
					mask_color = *mask_data;
					mask_data ++;
					switch(mask_color)
					{
						case 0:
							continue;
						case 255:
							break;
						default:
							src_alpha = src_alpha * mask_color >> 8;
							break;
					}
				}
				else if(default_mask_color != 255)
				{
					switch(default_mask_color)
					{
						case 0:
							continue;
						case 255:
							break;
						default:
							src_alpha = src_alpha * default_mask_color >> 8;
							break;
					}
				}
				
				switch(src_alpha)
				{
					case 0:
						break;
					default:
						src_alpha = src_alpha * opacity >> 8;
						dst_color = *dst_data;
						dst_alpha = PSD_GET_ALPHA_COMPONENT(dst_color);
						dst_red = PSD_GET_RED_COMPONENT(dst_color);
						dst_green = PSD_GET_GREEN_COMPONENT(dst_color);
						dst_blue = PSD_GET_BLUE_COMPONENT(dst_color);
						switch(dst_alpha)
						{
							case 0:
								*dst_data = (src_alpha << 24) | (src_color & 0x00FFFFFF);
								break;
							case 255:
								dst_red = ((dst_red << 8) + (PSD_GET_RED_COMPONENT(src_color) - dst_red) * src_alpha) >> 8;
								dst_green = ((dst_green << 8) + (PSD_GET_GREEN_COMPONENT(src_color) - dst_green) * src_alpha) >> 8;
								dst_blue = ((dst_blue << 8) + (PSD_GET_BLUE_COMPONENT(src_color) - dst_blue) * src_alpha) >> 8;
								*dst_data = PSD_RGB_TO_COLOR(dst_red, dst_green, dst_blue);
								break;
							default:
								mix_alpha = (src_alpha << 8) / (src_alpha + ((256 - src_alpha) * dst_alpha >> 8));
								dst_alpha = dst_alpha + ((256 - dst_alpha) * src_alpha >> 8);
								dst_red = ((dst_red << 8) + (PSD_GET_RED_COMPONENT(src_color) - dst_red) * mix_alpha) >> 8;
								dst_green = ((dst_green << 8) + (PSD_GET_GREEN_COMPONENT(src_color) - dst_green) * mix_alpha) >> 8;
								dst_blue = ((dst_blue << 8) + (PSD_GET_BLUE_COMPONENT(src_color) - dst_blue) * mix_alpha) >> 8;
								*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
								break;
						}
						break;
				}
			}
		}
	}
}

void psd_layer_blend(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_argb_color * dst_data, * src_data, dst_color, src_color;
	psd_int i, j, width, height;
	psd_int opacity, mix_alpha;
	psd_int dst_red, dst_green, dst_blue, dst_alpha, src_red, src_green, src_blue, src_alpha;
	psd_int dst_hue, dst_saturation, dst_brightness, src_hue, src_saturation, src_brightness;
	psd_blend_mode blend_mode;
	psd_color_component * mask_data = NULL, mask_color, default_mask_color;
	psd_int mask_left, mask_right, dst_left;
	psd_layer_mask_info * layer_mask_info = &layer->layer_mask_info;
	psd_uchar * rand_data;

	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	opacity = layer->opacity * layer->fill_opacity / 255;
	if(layer->group_layer != NULL)
		opacity = opacity * layer->group_layer->opacity / 255;
	if(layer->group_layer != NULL && layer->group_layer->divider_blend_mode != psd_blend_mode_pass_through)
		blend_mode = layer->group_layer->divider_blend_mode;
	else
		blend_mode = layer->blend_mode;

	if(layer_mask_info->disabled == psd_false)
		default_mask_color = layer_mask_info->default_color;
	else
		default_mask_color = 255;
	mask_left = layer_mask_info->left;
	mask_right = layer_mask_info->right;
	dst_left = dst_rect->left;
	
	if(blend_mode == psd_blend_mode_dissolve)
	{
		for(i = 0; i < height; i ++)
		{
			dst_data = context->blending_image_data + (i + dst_rect->top) * context->width + 
				dst_rect->left;
			src_data = layer->image_data + (i + dst_rect->top - layer->top) * layer->width + 
				(dst_rect->left - layer->left);
			rand_data = context->rand_data + (i + dst_rect->top) * context->width + dst_rect->left;
			
			if(layer_mask_info->disabled == psd_false && layer_mask_info->mask_data != NULL &&
				i + dst_rect->top >= layer_mask_info->top && i + dst_rect->top < layer_mask_info->bottom)
			{
				mask_data = layer_mask_info->mask_data + (i + dst_rect->top - layer_mask_info->top) * layer_mask_info->width + 
					PSD_MAX(0, dst_rect->left - layer_mask_info->left);
			}
			else
			{
				mask_data = NULL;
			}

			for(j = 0; j < width; j ++, dst_data ++, src_data ++, rand_data ++)
			{
				src_color = *src_data;
				src_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
				
				if(mask_data != NULL && j + dst_left >= mask_left && j + dst_left < mask_right)
				{
					mask_color = *mask_data;
					mask_data ++;
					switch(mask_color)
					{
						case 0:
							continue;
						case 255:
							break;
						default:
							src_alpha = src_alpha * mask_color >> 8;
							break;
					}
				}
				else if(default_mask_color != 255)
				{
					switch(default_mask_color)
					{
						case 0:
							continue;
						case 255:
							break;
						default:
							src_alpha = src_alpha * default_mask_color >> 8;
							break;
					}
				}

				switch(src_alpha)
				{
					case 0:
						break;
					case 255:
						*dst_data = (src_color & 0x00FFFFFF) | 0xFF000000;
						break;
					default:
						dst_alpha = PSD_GET_ALPHA_COMPONENT(*dst_data);
						mix_alpha = (src_alpha << 8) / (src_alpha + ((256 - src_alpha) * dst_alpha >> 8));
						if(*rand_data <= mix_alpha)
							*dst_data = (src_color & 0x00FFFFFF) | 0xFF000000;
						break;
				}
			}
		}

		return;
	}

	for(i = 0; i < height; i ++)
	{
		dst_data = context->blending_image_data + (i + dst_rect->top) * context->width + 
			dst_rect->left;
		src_data = layer->image_data + (i + dst_rect->top - layer->top) * layer->width + 
			(dst_rect->left - layer->left);
		
		if(layer_mask_info->disabled == psd_false && layer_mask_info->mask_data != NULL &&
			i + dst_rect->top >= layer_mask_info->top && i + dst_rect->top < layer_mask_info->bottom)
		{
			mask_data = layer_mask_info->mask_data + (i + dst_rect->top - layer_mask_info->top) * layer_mask_info->width + 
				PSD_MAX(0, dst_rect->left - layer_mask_info->left);
		}
		else
		{
			mask_data = NULL;
		}

		for(j = 0; j < width; j ++, dst_data ++, src_data ++)
		{
			src_color = *src_data;
			src_alpha = PSD_GET_ALPHA_COMPONENT(src_color);
			
			if(mask_data != NULL && j + dst_left >= mask_left && j + dst_left < mask_right)
			{
				mask_color = *mask_data;
				mask_data ++;
				switch(mask_color)
				{
					case 0:
						continue;
					case 255:
						break;
					default:
						src_alpha = src_alpha * mask_color >> 8;
						break;
				}
			}
			else if(default_mask_color != 255)
			{
				switch(default_mask_color)
				{
					case 0:
						continue;
					case 255:
						break;
					default:
						src_alpha = src_alpha * default_mask_color >> 8;
						break;
				}
			}
			
			switch(src_alpha)
			{
				case 0:
					continue;
				default:
					src_alpha = src_alpha * opacity >> 8;
					dst_color = *dst_data;
					dst_alpha = PSD_GET_ALPHA_COMPONENT(dst_color);
					switch(dst_alpha)
					{
						case 0:
							*dst_data = (src_alpha << 24) | (src_color & 0x00FFFFFF);
							continue;
						case 255:
							mix_alpha = src_alpha;
							break;
						default:
							mix_alpha = (src_alpha << 8) / (src_alpha + ((256 - src_alpha) * dst_alpha >> 8));
							dst_alpha = dst_alpha + ((256 - dst_alpha) * src_alpha >> 8);
							break;
					}
					break;
			}
			
			src_red = PSD_GET_RED_COMPONENT(src_color);
			src_green = PSD_GET_GREEN_COMPONENT(src_color);
			src_blue = PSD_GET_BLUE_COMPONENT(src_color);
			dst_red = PSD_GET_RED_COMPONENT(dst_color);
			dst_green = PSD_GET_GREEN_COMPONENT(dst_color);
			dst_blue = PSD_GET_BLUE_COMPONENT(dst_color);

			switch(blend_mode)
			{
				case psd_blend_mode_normal:
					PSD_BLEND_NORMAL(dst_red, src_red, mix_alpha);
					PSD_BLEND_NORMAL(dst_green, src_green, mix_alpha);
					PSD_BLEND_NORMAL(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_darken:
					PSD_BLEND_DARKEN(dst_red, src_red, mix_alpha);
					PSD_BLEND_DARKEN(dst_green, src_green, mix_alpha);
					PSD_BLEND_DARKEN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_multiply:
					PSD_BLEND_MULTIPLY(dst_red, src_red, mix_alpha);
					PSD_BLEND_MULTIPLY(dst_green, src_green, mix_alpha);
					PSD_BLEND_MULTIPLY(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_color_burn:
					PSD_BLEND_COLOR_BURN(dst_red, src_red, mix_alpha);
					PSD_BLEND_COLOR_BURN(dst_green, src_green, mix_alpha);
					PSD_BLEND_COLOR_BURN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_linear_burn:
					PSD_BLEND_LINEEAR_BURN(dst_red, src_red, mix_alpha);
					PSD_BLEND_LINEEAR_BURN(dst_green, src_green, mix_alpha);
					PSD_BLEND_LINEEAR_BURN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_lighten:
					PSD_BLEND_LIGHTEN(dst_red, src_red, mix_alpha);
					PSD_BLEND_LIGHTEN(dst_green, src_green, mix_alpha);
					PSD_BLEND_LIGHTEN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_screen:
					PSD_BLEND_SCREEN(dst_red, src_red, mix_alpha);
					PSD_BLEND_SCREEN(dst_green, src_green, mix_alpha);
					PSD_BLEND_SCREEN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_color_dodge:
					PSD_BLEND_COLOR_DODGE(dst_red, src_red, mix_alpha);
					PSD_BLEND_COLOR_DODGE(dst_green, src_green, mix_alpha);
					PSD_BLEND_COLOR_DODGE(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_linear_dodge:
					PSD_BLEND_LINEAR_DODGE(dst_red, src_red, mix_alpha);
					PSD_BLEND_LINEAR_DODGE(dst_green, src_green, mix_alpha);
					PSD_BLEND_LINEAR_DODGE(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_overlay:
					PSD_BLEND_OVERLAY(dst_red, src_red, mix_alpha);
					PSD_BLEND_OVERLAY(dst_green, src_green, mix_alpha);
					PSD_BLEND_OVERLAY(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_soft_light:
					PSD_BLEND_SOFTLIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_SOFTLIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_SOFTLIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_hard_light:
					PSD_BLEND_HARDLIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_HARDLIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_HARDLIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_vivid_light:	// ?
					PSD_BLEND_VIVID_LIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_VIVID_LIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_VIVID_LIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_linear_light:	// ?
					PSD_BLEND_LINEAR_LIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_LINEAR_LIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_LINEAR_LIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_pin_light:
					PSD_BLEND_PIN_LIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_PIN_LIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_PIN_LIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_hard_mix:
					PSD_BLEND_HARD_MIX(dst_red, src_red, mix_alpha);
					PSD_BLEND_HARD_MIX(dst_green, src_green, mix_alpha);
					PSD_BLEND_HARD_MIX(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_difference:
					PSD_BLEND_DIFFERENCE(dst_red, src_red, mix_alpha);
					PSD_BLEND_DIFFERENCE(dst_green, src_green, mix_alpha);
					PSD_BLEND_DIFFERENCE(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_exclusion:
					PSD_BLEND_EXCLUSION(dst_red, src_red, mix_alpha);
					PSD_BLEND_EXCLUSION(dst_green, src_green, mix_alpha);
					PSD_BLEND_EXCLUSION(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_hue:
				case psd_blend_mode_saturation:
				case psd_blend_mode_color:
				case psd_blend_mode_luminosity:
					psd_rgb_to_inthsb(src_red, src_green, src_blue, &src_hue, &src_saturation, &src_brightness);
					psd_rgb_to_inthsb(dst_red, dst_green, dst_blue, &dst_hue, &dst_saturation, &dst_brightness);
					switch(blend_mode)
					{
						case psd_blend_mode_hue:
							dst_hue = src_hue;
							break;
						case psd_blend_mode_saturation:
							dst_saturation = src_saturation;
							break;
						case psd_blend_mode_color:
							dst_hue = src_hue;
							dst_saturation = src_saturation;
							break;
						case psd_blend_mode_luminosity:
							dst_brightness = src_brightness;
							break;
					}
					psd_inthsb_to_rgb(dst_hue, dst_saturation, dst_brightness, &src_red, &src_green, &src_blue);
					dst_red = PSD_BLEND_CHANNEL(dst_red, src_red, mix_alpha);
					dst_green = PSD_BLEND_CHANNEL(dst_green, src_green, mix_alpha);
					dst_blue = PSD_BLEND_CHANNEL(dst_blue, src_blue, mix_alpha);
					break;
			}
			
			*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
		}
	}
}

void psd_layer_blend_normal_restricted(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_bool gray_restricted = psd_false, red_restricted = psd_false, green_restricted = psd_false, blue_restricted = psd_false;
	psd_int min_src_gray = 0, max_src_gray = 255, min_src_red = 0, max_src_red = 255, 
		min_src_green = 0, max_src_green = 255, min_src_blue = 0, max_src_blue = 255;
	psd_int min_dst_gray = 0, max_dst_gray = 255, min_dst_red = 0, max_dst_red = 255, 
		min_dst_green = 0, max_dst_green = 255, min_dst_blue = 0, max_dst_blue = 255;
	psd_argb_color * dst_data, * src_data, dst_color, src_color;
	psd_int i, j, width, height;
	psd_int opacity;
	psd_int src_red, src_green, src_blue, src_alpha, 
		dst_red, dst_green, dst_blue, dst_alpha, gray;
	psd_int flr1, flr2;
	psd_color_component * mask_data = NULL, mask_color, default_mask_color;
	psd_int mask_left, mask_right, dst_left;
	psd_layer_mask_info * layer_mask_info = &layer->layer_mask_info;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		if(layer->channel_info[i].restricted == psd_true)
		{
			switch(layer->channel_info[i].channel_id)
			{
				case 0:
					red_restricted = psd_true;
					break;
				case 1:
					green_restricted = psd_true;
					break;
				case 2:
					blue_restricted = psd_true;
					break;
			}
		}
	}
	if(red_restricted == psd_true && green_restricted == psd_true && blue_restricted == psd_true)
		gray_restricted = psd_true;

	if(layer->layer_blending_ranges.gray_black_src == layer->layer_blending_ranges.gray_white_src)
		gray_restricted = psd_true;

	if(gray_restricted == psd_true)
		return;

	min_src_gray = layer->layer_blending_ranges.gray_black_src >> 8;
	max_src_gray = layer->layer_blending_ranges.gray_white_src >> 8;
	min_dst_gray = layer->layer_blending_ranges.gray_black_dst >> 8;
	max_dst_gray = layer->layer_blending_ranges.gray_white_dst >> 8;

	for(i = 0; i < layer->layer_blending_ranges.number_of_blending_channels; i ++)
	{
		switch(i)
		{
			case 0:
				min_src_red = layer->layer_blending_ranges.channel_black_src[i] >> 8;
				max_src_red = layer->layer_blending_ranges.channel_white_src[i] >> 8;
				min_dst_red = layer->layer_blending_ranges.channel_black_dst[i] >> 8;
				max_dst_red = layer->layer_blending_ranges.channel_white_dst[i] >> 8;
				break;
			case 1:
				min_src_green = layer->layer_blending_ranges.channel_black_src[i] >> 8;
				max_src_green = layer->layer_blending_ranges.channel_white_src[i] >> 8;
				min_dst_green = layer->layer_blending_ranges.channel_black_dst[i] >> 8;
				max_dst_green = layer->layer_blending_ranges.channel_white_dst[i] >> 8;
				break;
			case 2:
				min_src_blue = layer->layer_blending_ranges.channel_black_src[i] >> 8;
				max_src_blue = layer->layer_blending_ranges.channel_white_src[i] >> 8;
				min_dst_blue = layer->layer_blending_ranges.channel_black_dst[i] >> 8;
				max_dst_blue = layer->layer_blending_ranges.channel_white_dst[i] >> 8;
				break;
		}
	}

	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	opacity = layer->opacity * layer->fill_opacity / 255;
	if(layer->group_layer != NULL)
		opacity = opacity * layer->group_layer->opacity / 255;

	if(layer_mask_info->disabled == psd_false)
		default_mask_color = layer_mask_info->default_color;
	else
		default_mask_color = 255;
	mask_left = layer_mask_info->left;
	mask_right = layer_mask_info->right;
	dst_left = dst_rect->left;

	for(i = 0; i < height; i ++)
	{
		dst_data = context->blending_image_data + (i + dst_rect->top) * context->width + 
			dst_rect->left;
		src_data = layer->image_data + (i + dst_rect->top - layer->top) * layer->width + 
			(dst_rect->left - layer->left);
		
		if(layer_mask_info->disabled == psd_false && layer_mask_info->mask_data != NULL &&
			i + dst_rect->top >= layer_mask_info->top && i + dst_rect->top < layer_mask_info->bottom)
		{
			mask_data = layer_mask_info->mask_data + (i + dst_rect->top - layer_mask_info->top) * layer_mask_info->width + 
				PSD_MAX(0, dst_rect->left - layer_mask_info->left);
		}
		else
		{
			mask_data = NULL;
		}
		
		for(j = 0; j < width; j ++, dst_data ++, src_data ++)
		{
			src_color = *src_data;
			src_alpha = PSD_GET_ALPHA_COMPONENT(src_color);

			if(mask_data != NULL && j + dst_left >= mask_left && j + dst_left < mask_right)
			{
				mask_color = *mask_data;
				mask_data ++;
				switch(mask_color)
				{
					case 0:
						continue;
					case 255:
						break;
					default:
						src_alpha = src_alpha * mask_color >> 8;
						break;
				}
			}
			else if(default_mask_color != 255)
			{
				switch(default_mask_color)
				{
					case 0:
						continue;
					case 255:
						break;
					default:
						src_alpha = src_alpha * default_mask_color >> 8;
						break;
				}
			}
			
			switch(src_alpha)
			{
				case 0:
					break;
				default:
					src_alpha = src_alpha * opacity >> 8;
					src_red = PSD_GET_RED_COMPONENT(src_color);
					src_green = PSD_GET_GREEN_COMPONENT(src_color);
					src_blue = PSD_GET_BLUE_COMPONENT(src_color);
					dst_color = *dst_data;
					dst_alpha = PSD_GET_ALPHA_COMPONENT(dst_color);
					dst_red = PSD_GET_RED_COMPONENT(dst_color);
					dst_green = PSD_GET_GREEN_COMPONENT(dst_color);
					dst_blue = PSD_GET_BLUE_COMPONENT(dst_color);
					
					if(max_dst_gray - min_dst_gray != 255)
					{
						gray = (dst_red + dst_green + dst_blue) / 3;
						if(gray < min_dst_gray || gray > max_dst_gray)
							continue;
					}
					if(max_src_gray - min_src_gray != 255)
					{
						gray = (src_red + src_green + src_blue) / 3;
						if(gray < min_src_gray || gray > max_src_gray)
							continue;
					}
					
					if(dst_red < min_dst_red || dst_red > max_dst_red ||
						src_red < min_src_red || src_red > max_src_red)
						continue;
					if(dst_green < min_dst_green || dst_green > max_dst_green ||
						src_green < min_src_green || src_green > max_src_green)
						continue;
					if(dst_blue < min_dst_blue || dst_blue > max_dst_blue ||
						src_blue < min_src_blue || src_blue > max_src_blue)
						continue;

					if(src_alpha == 255)
					{
						dst_alpha = 255;
						if(red_restricted == psd_false)
							dst_red = src_red;
						if(green_restricted == psd_false)
							dst_green = src_green;
						if(blue_restricted == psd_false)
							dst_blue = src_blue;
					}
					else if(dst_alpha == 255)
					{
						dst_alpha = 255;
						if(red_restricted == psd_false)
							dst_red = ((dst_red << 8) + (PSD_GET_RED_COMPONENT(src_color) - dst_red) * src_alpha) >> 8;
						if(green_restricted == psd_false)
							dst_green = ((dst_green << 8) + (PSD_GET_GREEN_COMPONENT(src_color) - dst_green) * src_alpha) >> 8;
						if(blue_restricted == psd_false)
							dst_blue = ((dst_blue << 8) + (PSD_GET_BLUE_COMPONENT(src_color) - dst_blue) * src_alpha) >> 8;
					}
					else
					{
						flr1 = (src_alpha << 8) / (src_alpha + ((256 - src_alpha) * dst_alpha >> 8));
						flr2 = 0x100 - flr1;
						dst_alpha = dst_alpha + ((256 - dst_alpha) * src_alpha >> 8);
						if(red_restricted == psd_false)
							dst_red = (dst_red * flr2 + PSD_GET_RED_COMPONENT(src_color) * flr1) >> 8;
						if(green_restricted == psd_false)
							dst_green = (dst_green * flr2 + PSD_GET_GREEN_COMPONENT(src_color) * flr1) >> 8;
						if(blue_restricted == psd_false)
							dst_blue = (dst_blue * flr2 + PSD_GET_BLUE_COMPONENT(src_color) * flr1) >> 8;
					}
					*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
					break;
			}
		}
	}
}

void psd_layer_blend_restricted(psd_context * context, psd_layer_record * layer, psd_rect * dst_rect)
{
	psd_bool gray_restricted = psd_false, red_restricted = psd_false, green_restricted = psd_false, blue_restricted = psd_false;
	psd_int min_src_gray = 0, max_src_gray = 255, min_src_red = 0, max_src_red = 255, 
		min_src_green = 0, max_src_green = 255, min_src_blue = 0, max_src_blue = 255;
	psd_int min_dst_gray = 0, max_dst_gray = 255, min_dst_red = 0, max_dst_red = 255, 
		min_dst_green = 0, max_dst_green = 255, min_dst_blue = 0, max_dst_blue = 255;
	psd_argb_color * dst_data, * src_data, dst_color, src_color;
	psd_int i, j, width, height;
	psd_int opacity, mix_alpha;
	psd_int src_red, src_green, src_blue, src_alpha, 
		dst_red, dst_green, dst_blue, dst_alpha, gray;
	psd_int dst_hue, dst_saturation, dst_brightness, src_hue, src_saturation, src_brightness;
	psd_blend_mode blend_mode;
	psd_color_component * mask_data = NULL, mask_color, default_mask_color;
	psd_int mask_left, mask_right, dst_left;
	psd_layer_mask_info * layer_mask_info = &layer->layer_mask_info;
	psd_uchar * rand_data;

	for(i = 0; i < layer->number_of_channels; i ++)
	{
		if(layer->channel_info[i].restricted == psd_true)
		{
			switch(layer->channel_info[i].channel_id)
			{
				case 0:
					red_restricted = psd_true;
					break;
				case 1:
					green_restricted = psd_true;
					break;
				case 2:
					blue_restricted = psd_true;
					break;
			}
		}
	}
	if(red_restricted == psd_true && green_restricted == psd_true && blue_restricted == psd_true)
		gray_restricted = psd_true;

	if(layer->layer_blending_ranges.gray_black_src == layer->layer_blending_ranges.gray_white_src)
		gray_restricted = psd_true;

	if(gray_restricted == psd_true)
		return;

	min_src_gray = layer->layer_blending_ranges.gray_black_src >> 8;
	max_src_gray = layer->layer_blending_ranges.gray_white_src >> 8;
	min_dst_gray = layer->layer_blending_ranges.gray_black_dst >> 8;
	max_dst_gray = layer->layer_blending_ranges.gray_white_dst >> 8;

	for(i = 0; i < layer->layer_blending_ranges.number_of_blending_channels; i ++)
	{
		switch(i)
		{
			case 0:
				min_src_red = layer->layer_blending_ranges.channel_black_src[i] >> 8;
				max_src_red = layer->layer_blending_ranges.channel_white_src[i] >> 8;
				min_dst_red = layer->layer_blending_ranges.channel_black_dst[i] >> 8;
				max_dst_red = layer->layer_blending_ranges.channel_white_dst[i] >> 8;
				break;
			case 1:
				min_src_green = layer->layer_blending_ranges.channel_black_src[i] >> 8;
				max_src_green = layer->layer_blending_ranges.channel_white_src[i] >> 8;
				min_dst_green = layer->layer_blending_ranges.channel_black_dst[i] >> 8;
				max_dst_green = layer->layer_blending_ranges.channel_white_dst[i] >> 8;
				break;
			case 2:
				min_src_blue = layer->layer_blending_ranges.channel_black_src[i] >> 8;
				max_src_blue = layer->layer_blending_ranges.channel_white_src[i] >> 8;
				min_dst_blue = layer->layer_blending_ranges.channel_black_dst[i] >> 8;
				max_dst_blue = layer->layer_blending_ranges.channel_white_dst[i] >> 8;
				break;
		}
	}

	width = psd_rect_width(dst_rect);
	height = psd_rect_height(dst_rect);
	opacity = layer->opacity * layer->fill_opacity / 255;
	if(layer->group_layer != NULL)
		opacity = opacity * layer->group_layer->opacity / 255;
	if(layer->group_layer != NULL && layer->group_layer->divider_blend_mode != psd_blend_mode_pass_through)
		blend_mode = layer->group_layer->divider_blend_mode;
	else
		blend_mode = layer->blend_mode;

	if(layer_mask_info->disabled == psd_false)
		default_mask_color = layer_mask_info->default_color;
	else
		default_mask_color = 255;
	mask_left = layer_mask_info->left;
	mask_right = layer_mask_info->right;
	dst_left = dst_rect->left;

	for(i = 0; i < height; i ++)
	{
		dst_data = context->blending_image_data + (i + dst_rect->top) * context->width + 
			dst_rect->left;
		src_data = layer->image_data + (i + dst_rect->top - layer->top) * layer->width + 
			(dst_rect->left - layer->left);
		rand_data = context->rand_data + (i + dst_rect->top) * context->width + dst_rect->left;
		
		if(layer_mask_info->disabled == psd_false && layer_mask_info->mask_data != NULL &&
			i + dst_rect->top >= layer_mask_info->top && i + dst_rect->top < layer_mask_info->bottom)
		{
			mask_data = layer_mask_info->mask_data + (i + dst_rect->top - layer_mask_info->top) * layer_mask_info->width + 
				PSD_MAX(0, dst_rect->left - layer_mask_info->left);
		}
		else
		{
			mask_data = NULL;
		}

		for(j = 0; j < width; j ++, dst_data ++, src_data ++, rand_data ++)
		{
			src_color = *src_data;
			src_alpha = PSD_GET_ALPHA_COMPONENT(src_color);

			if(mask_data != NULL && j + dst_left >= mask_left && j + dst_left < mask_right)
			{
				mask_color = *mask_data;
				mask_data ++;
				switch(mask_color)
				{
					case 0:
						continue;
					case 255:
						break;
					default:
						src_alpha = src_alpha * mask_color >> 8;
						break;
				}
			}
			else if(default_mask_color != 255)
			{
				switch(default_mask_color)
				{
					case 0:
						continue;
					case 255:
						break;
					default:
						src_alpha = src_alpha * default_mask_color >> 8;
						break;
				}
			}

			switch(src_alpha)
			{
				case 0:
					continue;
				default:
					src_alpha = src_alpha * opacity >> 8;
					dst_color = *dst_data;
					dst_alpha = PSD_GET_ALPHA_COMPONENT(dst_color);
					switch(dst_alpha)
					{
						case 0:
							*dst_data = (src_alpha << 24) | (src_color & 0x00FFFFFF);
							continue;
						case 255:
							mix_alpha = src_alpha;
							break;
						default:
							mix_alpha = (src_alpha << 8) / (src_alpha + ((256 - src_alpha) * dst_alpha >> 8));
							dst_alpha = dst_alpha + ((256 - dst_alpha) * src_alpha >> 8);
							break;
					}
					break;
			}
			
			src_red = PSD_GET_RED_COMPONENT(src_color);
			src_green = PSD_GET_GREEN_COMPONENT(src_color);
			src_blue = PSD_GET_BLUE_COMPONENT(src_color);
			dst_color = *dst_data;
			dst_alpha = PSD_GET_ALPHA_COMPONENT(dst_color);
			dst_red = PSD_GET_RED_COMPONENT(dst_color);
			dst_green = PSD_GET_GREEN_COMPONENT(dst_color);
			dst_blue = PSD_GET_BLUE_COMPONENT(dst_color);
			
			if(max_dst_gray - min_dst_gray != 255)
			{
				gray = (dst_red + dst_green + dst_blue) / 3;
				if(gray < min_dst_gray || gray > max_dst_gray)
					continue;
			}
			if(max_src_gray - min_src_gray != 255)
			{
				gray = (src_red + src_green + src_blue) / 3;
				if(gray < min_src_gray || gray > max_src_gray)
					continue;
			}
			
			if(dst_red < min_dst_red || dst_red > max_dst_red ||
				src_red < min_src_red || src_red > max_src_red)
				continue;
			if(dst_green < min_dst_green || dst_green > max_dst_green ||
				src_green < min_src_green || src_green > max_src_green)
				continue;
			if(dst_blue < min_dst_blue || dst_blue > max_dst_blue ||
				src_blue < min_src_blue || src_blue > max_src_blue)
				continue;

			switch(blend_mode)
			{
				case psd_blend_mode_normal:
					PSD_BLEND_NORMAL(dst_red, src_red, mix_alpha);
					PSD_BLEND_NORMAL(dst_green, src_green, mix_alpha);
					PSD_BLEND_NORMAL(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_dissolve:
					switch(mix_alpha)
					{
						case 0:
							break;
						case 255:
							*dst_data = (src_color & 0x00FFFFFF) | 0xFF000000;
							break;
						default:
							if(*rand_data <= mix_alpha)
								*dst_data = (src_color & 0x00FFFFFF) | 0xFF000000;
							break;
					}
					continue;
				case psd_blend_mode_darken:
					PSD_BLEND_DARKEN(dst_red, src_red, mix_alpha);
					PSD_BLEND_DARKEN(dst_green, src_green, mix_alpha);
					PSD_BLEND_DARKEN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_multiply:
					PSD_BLEND_MULTIPLY(dst_red, src_red, mix_alpha);
					PSD_BLEND_MULTIPLY(dst_green, src_green, mix_alpha);
					PSD_BLEND_MULTIPLY(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_color_burn:
					PSD_BLEND_COLOR_BURN(dst_red, src_red, mix_alpha);
					PSD_BLEND_COLOR_BURN(dst_green, src_green, mix_alpha);
					PSD_BLEND_COLOR_BURN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_linear_burn:
					PSD_BLEND_LINEEAR_BURN(dst_red, src_red, mix_alpha);
					PSD_BLEND_LINEEAR_BURN(dst_green, src_green, mix_alpha);
					PSD_BLEND_LINEEAR_BURN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_lighten:
					PSD_BLEND_LIGHTEN(dst_red, src_red, mix_alpha);
					PSD_BLEND_LIGHTEN(dst_green, src_green, mix_alpha);
					PSD_BLEND_LIGHTEN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_screen:
					PSD_BLEND_SCREEN(dst_red, src_red, mix_alpha);
					PSD_BLEND_SCREEN(dst_green, src_green, mix_alpha);
					PSD_BLEND_SCREEN(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_color_dodge:
					PSD_BLEND_COLOR_DODGE(dst_red, src_red, mix_alpha);
					PSD_BLEND_COLOR_DODGE(dst_green, src_green, mix_alpha);
					PSD_BLEND_COLOR_DODGE(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_linear_dodge:
					PSD_BLEND_LINEAR_DODGE(dst_red, src_red, mix_alpha);
					PSD_BLEND_LINEAR_DODGE(dst_green, src_green, mix_alpha);
					PSD_BLEND_LINEAR_DODGE(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_overlay:
					PSD_BLEND_OVERLAY(dst_red, src_red, mix_alpha);
					PSD_BLEND_OVERLAY(dst_green, src_green, mix_alpha);
					PSD_BLEND_OVERLAY(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_soft_light:
					PSD_BLEND_SOFTLIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_SOFTLIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_SOFTLIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_hard_light:
					PSD_BLEND_HARDLIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_HARDLIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_HARDLIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_vivid_light:	// ?
					PSD_BLEND_VIVID_LIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_VIVID_LIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_VIVID_LIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_linear_light:	// ?
					PSD_BLEND_LINEAR_LIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_LINEAR_LIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_LINEAR_LIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_pin_light:
					PSD_BLEND_PIN_LIGHT(dst_red, src_red, mix_alpha);
					PSD_BLEND_PIN_LIGHT(dst_green, src_green, mix_alpha);
					PSD_BLEND_PIN_LIGHT(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_hard_mix:
					PSD_BLEND_HARD_MIX(dst_red, src_red, mix_alpha);
					PSD_BLEND_HARD_MIX(dst_green, src_green, mix_alpha);
					PSD_BLEND_HARD_MIX(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_difference:
					PSD_BLEND_DIFFERENCE(dst_red, src_red, mix_alpha);
					PSD_BLEND_DIFFERENCE(dst_green, src_green, mix_alpha);
					PSD_BLEND_DIFFERENCE(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_exclusion:
					PSD_BLEND_EXCLUSION(dst_red, src_red, mix_alpha);
					PSD_BLEND_EXCLUSION(dst_green, src_green, mix_alpha);
					PSD_BLEND_EXCLUSION(dst_blue, src_blue, mix_alpha);
					break;
				case psd_blend_mode_hue:
				case psd_blend_mode_saturation:
				case psd_blend_mode_color:
				case psd_blend_mode_luminosity:
					psd_rgb_to_inthsb(src_red, src_green, src_blue, &src_hue, &src_saturation, &src_brightness);
					psd_rgb_to_inthsb(dst_red, dst_green, dst_blue, &dst_hue, &dst_saturation, &dst_brightness);
					switch(blend_mode)
					{
						case psd_blend_mode_hue:
							dst_hue = src_hue;
							break;
						case psd_blend_mode_saturation:
							dst_saturation = src_saturation;
							break;
						case psd_blend_mode_color:
							dst_hue = src_hue;
							dst_saturation = src_saturation;
							break;
						case psd_blend_mode_luminosity:
							dst_brightness = src_brightness;
							break;
					}
					psd_inthsb_to_rgb(dst_hue, dst_saturation, dst_brightness, &src_red, &src_green, &src_blue);
					dst_red = PSD_BLEND_CHANNEL(dst_red, src_red, mix_alpha);
					dst_green = PSD_BLEND_CHANNEL(dst_green, src_green, mix_alpha);
					dst_blue = PSD_BLEND_CHANNEL(dst_blue, src_blue, mix_alpha);
					break;
			}
			
			*dst_data = PSD_ARGB_TO_COLOR(dst_alpha, dst_red, dst_green, dst_blue);
		}
	}
}

#endif // ifdef PSD_SUPPORT_LAYER_BLEND


psd_status psd_image_blend(psd_context * context, psd_int left, psd_int top, psd_int width, psd_int height)
{
#ifdef PSD_SUPPORT_LAYER_BLEND
	psd_rect image_rect, layer_rect, mask_rect, rc;
	psd_int i;
	psd_layer_record * layer;
	psd_layer_mask_info * layer_mask_info;
	psd_status status;
	
	if(context == NULL)
		return psd_status_invalid_context;

	if(context->rand_data == NULL)
	{
		status = psd_build_rand_data(context);
		if(status != psd_status_done)
			return status;
	}

	psd_make_rect(&image_rect, left, top, left + width, top + height);
	psd_make_rect(&rc, 0, 0, context->width, context->height);
	if(psd_incept_rect(&image_rect, &rc, &image_rect) == psd_false)
		return psd_status_blend_empty_rect;
	
	if(context->blending_image_data == NULL)
	{
		context->blending_image_data = (psd_argb_color *)psd_malloc(context->width * context->height * 4);
		if(context->blending_image_data == NULL)
			return psd_status_malloc_failed;
		if(image_rect.left != 0 || image_rect.top != 0 || 
			image_rect.right != context->width || image_rect.bottom != context->height)
		{
			psd_color_memset(context->blending_image_data, psd_color_clear, context->width * context->height);
		}
	}

	psd_image_clear_rect(context, &image_rect);

	for(i = 0; i < context->layer_count; i ++)
	{
		layer = &context->layer_records[i];
		if(layer->visible == psd_false || layer->opacity == 0)
			continue;
		if(layer->group_layer != NULL)
		{
			if(layer->group_layer->visible == psd_false || layer->group_layer->opacity == 0)
				continue;
		}

		// if it is
		psd_adjustment_layer_blend(context, layer, &image_rect);

		// if it is
#ifdef PSD_SUPPORT_EFFECTS_BLEND
		psd_layer_effects_blend_background(context, layer, &image_rect);
#endif
		
		if(layer->image_data != NULL && layer->fill_opacity > 0 && layer->width > 0 && layer->height > 0)
		{
			psd_make_rect(&layer_rect, layer->left, layer->top, layer->right, layer->bottom);
			if(psd_incept_rect(&image_rect, &layer_rect, &layer_rect) == psd_true)
			{
				layer_mask_info = &layer->layer_mask_info;
				if(layer_mask_info->disabled == psd_false && layer_mask_info->mask_data != NULL && layer_mask_info->default_color == 0)
				{
					psd_make_rect(&mask_rect, layer_mask_info->left, layer_mask_info->top,
						layer_mask_info->right, layer_mask_info->bottom);
					if(psd_incept_rect(&mask_rect, &layer_rect, &layer_rect) == psd_false)
						continue;
				}
				
				if(psd_layer_check_restricted(context, layer) == psd_true)
				{
					if(layer->blend_mode != psd_blend_mode_normal ||
						(layer->group_layer != NULL && layer->group_layer->divider_blend_mode != psd_blend_mode_pass_through))
						psd_layer_blend_restricted(context, layer, &layer_rect);
					else
						psd_layer_blend_normal_restricted(context, layer, &layer_rect);
				}
				else
				{
					if(layer->blend_mode != psd_blend_mode_normal ||
						(layer->group_layer != NULL && layer->group_layer->divider_blend_mode != psd_blend_mode_pass_through))
						psd_layer_blend(context, layer, &layer_rect);
					else
						psd_layer_blend_normal(context, layer, &layer_rect);
				}
			}
		}

		// if it is
#ifdef PSD_SUPPORT_EFFECTS_BLEND
		psd_layer_effects_blend_foreground(context, layer, &image_rect);
#endif
	}

	return psd_status_done;
#else // ifdef PSD_SUPPORT_LAYER_BLEND
	return psd_status_unsupport_yet;
#endif // ifdef PSD_SUPPORT_LAYER_BLEND
}

void psd_image_blend_free(psd_context * context)
{
	psd_freeif(context->rand_data);
	psd_freeif(context->blending_image_data);
}

