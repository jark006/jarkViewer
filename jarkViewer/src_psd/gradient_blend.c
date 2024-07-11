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
 * $Id: gradient_blend.c, created by Patrick in 2006.06.22, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_system.h"
#include "psd_color.h"
#include "psd_fixed.h"
#include "psd_bitmap.h"
#include "psd_gradient.h"
#include "psd_math.h"


#define PSD_COLOR_SCALE				4


// from quake3 source code
psd_float psd_carm_sqrt(psd_float x)
{
	union
	{
		psd_int 	intPart;
		psd_float 	floatPart;
	} convertor;
	union
	{
		psd_int 	intPart;
		psd_float 	floatPart;
	} convertor2;
	
	convertor.floatPart = x;
	convertor2.floatPart = x;
	convertor.intPart = 0x1FBCF800 + (convertor.intPart >> 1);
	convertor2.intPart = 0x5f3759df - (convertor2.intPart >> 1);
	
	return 0.5f*(convertor.floatPart + (x * convertor2.floatPart));
}

void psd_gradient_color_get_table(psd_gradient_color * gradient_color, psd_argb_color * color_table, psd_int table_count, psd_bool reverse)
{
	psd_int i, j;
	psd_argb_color color;
	psd_int r1, r2, r3, g1, g2, g3, b1, b2, b3;
	psd_fixed_16_16 red, green, blue, red_step, green_step, blue_step;
	psd_int midpoint, location1, location2;
	psd_int o1, o2, o3;
	psd_uint high_opacity;
	psd_fixed_16_16 opacity, opacity_step;

	if(gradient_color == NULL)
		return;

	if(gradient_color->color_stop[0].location > 0)
	{
		location1 = 0;
		location2 = (gradient_color->color_stop[0].location * table_count + 2048) / 4096;
		psd_color_memset(color_table + location1, gradient_color->color_stop[0].actual_color,
			location2 - location1);
	}
	
	for(i = 0; i < gradient_color->number_color_stops - 1; i ++)
	{
		location1 = (gradient_color->color_stop[i].location * table_count + 2048) / 4096;
		location2 = (gradient_color->color_stop[i + 1].location * table_count + 2048) / 4096;
		if(gradient_color->color_stop[i].actual_color == gradient_color->color_stop[i + 1].actual_color)
		{
			psd_color_memset(color_table + location1, gradient_color->color_stop[i].actual_color,
				location2 - location1);
			continue;
		}
		
		color = gradient_color->color_stop[i].actual_color;
		r1 = psd_get_red_component(color);
		g1 = psd_get_green_component(color);
		b1 = psd_get_blue_component(color);
		color = gradient_color->color_stop[i + 1].actual_color;
		r3 = psd_get_red_component(color);
		g3 = psd_get_green_component(color);
		b3 = psd_get_blue_component(color);
		
		if(gradient_color->color_stop[i + 1].midpoint == 50)
		{
			red = PSD_FIXED_16_16_INT(r1);
			green = PSD_FIXED_16_16_INT(g1);
			blue = PSD_FIXED_16_16_INT(b1);
			red_step = PSD_FIXED_16_16_INT(r3 - r1) / (location2 - location1);
			green_step = PSD_FIXED_16_16_INT(g3 - g1) / (location2 - location1);
			blue_step = PSD_FIXED_16_16_INT(b3 - b1) / (location2 - location1);
			for(j = location1; j < location2; j ++)
			{
				color_table[j] = PSD_RGB_TO_COLOR(PSD_FIXED_16_16_ROUND(red), 
					PSD_FIXED_16_16_ROUND(green), PSD_FIXED_16_16_ROUND(blue));
				red += red_step;
				green += green_step;
				blue += blue_step;
			}
		}
		else
		{
			midpoint = location1 + (gradient_color->color_stop[i + 1].midpoint * 
				(location2 - location1) + 50) / 100;
			r2 = (r1 + r3) / 2;
			g2 = (g1 + g3) / 2;
			b2 = (b1 + b3) / 2;
			
			red = PSD_FIXED_16_16_INT(r1);
			green = PSD_FIXED_16_16_INT(g1);
			blue = PSD_FIXED_16_16_INT(b1);
			red_step = PSD_FIXED_16_16_INT(r2 - r1) / (midpoint - location1);
			green_step = PSD_FIXED_16_16_INT(g2 - g1) / (midpoint - location1);
			blue_step = PSD_FIXED_16_16_INT(b2 - b1) / (midpoint - location1);
			for(j = location1; j < midpoint; j ++)
			{
				color_table[j] = PSD_RGB_TO_COLOR(PSD_FIXED_16_16_ROUND(red), 
					PSD_FIXED_16_16_ROUND(green), PSD_FIXED_16_16_ROUND(blue));
				red += red_step;
				green += green_step;
				blue += blue_step;
			}

			red = PSD_FIXED_16_16_INT(r2);
			green = PSD_FIXED_16_16_INT(g2);
			blue = PSD_FIXED_16_16_INT(b2);
			red_step = PSD_FIXED_16_16_INT(r3 - r2) / (location2 - midpoint);
			green_step = PSD_FIXED_16_16_INT(g3 - g2) / (location2 - midpoint);
			blue_step = PSD_FIXED_16_16_INT(b3 - b2) / (location2 - midpoint);
			for(j = midpoint; j < location2; j ++)
			{
				color_table[j] = PSD_RGB_TO_COLOR(PSD_FIXED_16_16_ROUND(red), 
					PSD_FIXED_16_16_ROUND(green), PSD_FIXED_16_16_ROUND(blue));
				red += red_step;
				green += green_step;
				blue += blue_step;
			}
		}
	}

	if(gradient_color->color_stop[gradient_color->number_color_stops - 1].location < 4096)
	{
		location1 = (gradient_color->color_stop[gradient_color->number_color_stops - 1].location * table_count + 2048) / 4096;
		location2 = table_count;
		psd_color_memset(color_table + location1, gradient_color->color_stop[gradient_color->number_color_stops - 1].actual_color,
			location2 - location1);
	}

	if(gradient_color->transparency_stop[0].location > 0)
	{
		if(gradient_color->transparency_stop[0].opacity != 100)
		{
			location1 = 0;
			location2 = (gradient_color->transparency_stop[0].location * table_count + 2048) / 4096;
			high_opacity = (gradient_color->transparency_stop[0].opacity * 255 + 50) / 100;
			high_opacity <<= 24;
			for(j = location1; j < location2; j ++)
			{
				color_table[j] = (color_table[j] & 0x00FFFFFF) | high_opacity;
			}
		}
	}
	
	for(i = 0; i < gradient_color->number_transparency_stops - 1; i ++)
	{
		if(gradient_color->transparency_stop[i].opacity == 100 && 
			gradient_color->transparency_stop[i + 1].opacity == 100)
		{
			continue;
		}

		location1 = (gradient_color->transparency_stop[i].location * table_count + 2048) / 4096;
		location2 = (gradient_color->transparency_stop[i + 1].location * table_count + 2048) / 4096;
		
		if(gradient_color->transparency_stop[i].opacity == 
			gradient_color->transparency_stop[i + 1].opacity)
		{
			high_opacity = (gradient_color->transparency_stop[i].opacity * 255 + 50) / 100;
			high_opacity <<= 24;
			for(j = location1; j < location2; j ++)
			{
				color_table[j] = (color_table[j] & 0x00FFFFFF) | high_opacity;
			}
		}
		else
		{
			o1 = (gradient_color->transparency_stop[i].opacity * 255 + 50) / 100;
			o3 = (gradient_color->transparency_stop[i + 1].opacity * 255 + 50) / 100;

			if(gradient_color->transparency_stop[i + 1].midpoint == 50)
			{
				opacity = PSD_FIXED_16_16_INT(o1);
				opacity_step = PSD_FIXED_16_16_INT(o3 - o1) / (location2 - location1);
				for(j = location1; j < location2 ; j ++)
				{
					color_table[j] = (color_table[j] & 0x00FFFFFF) | 
						(PSD_FIXED_16_16_ROUND(opacity) << 24);
					opacity += opacity_step;
				}
			}
			else
			{
				o2 = (o1 + o3) / 2;
				midpoint = location1 + (gradient_color->transparency_stop[i + 1].midpoint * 
					(location2 - location1) + 50) / 100;
				
				opacity = PSD_FIXED_16_16_INT(o1);
				opacity_step = PSD_FIXED_16_16_INT(o2 - o1) / (midpoint - location1);
				for(j = location1; j < midpoint; j ++)
				{
					color_table[j] = (color_table[j] & 0x00FFFFFF) | 
						(PSD_FIXED_16_16_ROUND(opacity) << 24);
					opacity += opacity_step;
				}

				opacity = PSD_FIXED_16_16_INT(o2);
				opacity_step = PSD_FIXED_16_16_INT(o3 - o2) / (location2 - midpoint);
				for(j = midpoint; j < location2; j ++)
				{
					color_table[j] = (color_table[j] & 0x00FFFFFF) | 
						(PSD_FIXED_16_16_ROUND(opacity) << 24);
					opacity += opacity_step;
				}
			}
		}
	}

	if(gradient_color->transparency_stop[gradient_color->number_transparency_stops - 1].location < 4096)
	{
		if(gradient_color->transparency_stop[gradient_color->number_transparency_stops - 1].opacity != 100)
		{
			location1 = (gradient_color->transparency_stop[gradient_color->number_transparency_stops - 1].location * table_count + 2048) / 4096;
			location2 = table_count;
			high_opacity = (gradient_color->transparency_stop[gradient_color->number_transparency_stops - 1].opacity * 255 + 50) / 100;
			high_opacity <<= 24;
			for(j = location1; j < location2; j ++)
			{
				color_table[j] = (color_table[j] & 0x00FFFFFF) | high_opacity;
			}
		}
	}

	if(reverse == psd_true)
	{
		j = table_count / 2;
		for(i = 0; i < j; i ++)
		{
			color = color_table[i];
			color_table[i] = color_table[table_count - 1 - i];
			color_table[table_count - 1 - i] = color;
		}
	}
}

psd_status psd_gradient_fill_linear(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse, 
	psd_int start_x, psd_int start_y, psd_int end_x, psd_int end_y)
{
	psd_int distance, table_count;
	psd_argb_color * color_table;
	psd_int temp, i, j, width, height, index;
	psd_argb_color * dst_data, cur_color;
	psd_fixed_16_16 delta_x, delta_y, line_x, pixel_x;

	width = bitmap->width;
	height = bitmap->height;

	if(start_x == end_x)
	{
		table_count = PSD_ABS(end_y - start_y);
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		if(start_y > end_y)
		{
			temp = start_y;
			start_y = end_y;
			end_y = temp;
			reverse = psd_true - reverse;
		}
		psd_gradient_color_get_table(gradient_color, color_table, table_count, reverse);

		dst_data = bitmap->image_data;
		cur_color = color_table[0];
		for(i = 0; i < PSD_MIN(start_y, height); i ++)
		{
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}

		for(i = PSD_MAX(start_y, 0); i < PSD_MIN(end_y, height); i ++)
		{
			cur_color = color_table[i - start_y];
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}

		cur_color = color_table[table_count - 1];
		for(i = PSD_MAX(end_y, 0); i < height; i ++)
		{
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}
	}
	
	else if(start_y == end_y)
	{
		table_count = PSD_ABS(end_x - start_x);
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		if(start_x > end_x)
		{
			temp = start_x;
			start_x = end_x;
			end_x = temp;
			reverse = psd_true - reverse;
		}
		psd_gradient_color_get_table(gradient_color, color_table, table_count, reverse);

		dst_data = bitmap->image_data;
		cur_color = color_table[0];
		for(i = 0; i < PSD_MIN(start_x, width); i ++)
		{
			*dst_data = cur_color;
			dst_data ++;
		}

		for(i = PSD_MAX(start_x, 0); i < PSD_MIN(end_x, width); i ++)
		{
			cur_color = color_table[i - start_x];
			*dst_data = cur_color;
			dst_data ++;
		}

		cur_color = color_table[table_count - 1];
		for(i = PSD_MAX(end_x, 0); i < width; i ++)
		{
			*dst_data = cur_color;
			dst_data ++;
		}

		for(i = 1; i < height; i ++)
		{
			memcpy(dst_data, bitmap->image_data, width * 4);
			dst_data += width;
		}
	}
	
	else
	{
		distance = (psd_int)psd_carm_sqrt((psd_float)((end_x - start_x) * (end_x - start_x) + (end_y - start_y) * (end_y - start_y)) + 0.5f);
		table_count = distance * PSD_COLOR_SCALE;
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		psd_gradient_color_get_table(gradient_color, color_table, table_count, reverse);

		delta_x = PSD_FIXED_16_16_INT(end_x - start_x) / distance;
		delta_y = PSD_FIXED_16_16_INT(end_y - start_y) / distance;
		line_x = -delta_y * start_y - delta_x * start_x;
		line_x *= PSD_COLOR_SCALE;
		delta_x *= PSD_COLOR_SCALE;
		delta_y *= PSD_COLOR_SCALE;
		dst_data = bitmap->image_data;
		for(i = 0; i < height; i ++)
		{
			pixel_x = line_x;
			for(j = 0; j < width; j ++)
			{
				index =  PSD_FIXED_16_16_ROUND(pixel_x);
				if(index <= 0)
					cur_color = color_table[0];
				else if(index >= table_count - 1)
					cur_color = color_table[table_count - 1];
				else
					cur_color = color_table[index];
				*dst_data = cur_color;
				dst_data ++;
				pixel_x += delta_x;
			}
			line_x += delta_y;
		}
	}
	
	psd_free(color_table);
	
	return psd_status_done;
}

psd_status psd_gradient_fill_radial(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse, 
	psd_int center_x, psd_int center_y, psd_int radius)
{
	psd_int width, height, left, top, right, bottom, i, j, ii, i2, j2;
	psd_int table_count, distance;
	psd_argb_color * color_table;
	psd_argb_color * dst_data, cur_color;

	width = bitmap->width;
	height = bitmap->height;
	left = center_x > radius ? center_x - radius : 0;
	top = center_y > radius ? center_y - radius : 0;
	right = center_x + radius < width ? center_x + radius : width;
	bottom = center_y + radius < height ? center_y + radius : height;

	if(center_x < width / 2)
		left = center_x - (right - center_x);
	else
		right = center_x + (center_x - left);
	if(center_y < height / 2)
		top = center_y - (bottom - center_y);
	else
		bottom = center_y + (center_y - top);

	table_count = radius * PSD_COLOR_SCALE;
	color_table = (psd_argb_color *)psd_malloc(table_count * 4);
	if(color_table == NULL)
		return psd_status_malloc_failed;
	psd_gradient_color_get_table(gradient_color, color_table, table_count, reverse);

	cur_color = color_table[table_count - 1];

	if(top >= 0)
	{
		dst_data = bitmap->image_data;
		for(i = 0; i <= PSD_MIN(top, height - 1); i ++)
		{
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}
	}

	if(bottom < height)
	{
		dst_data = bitmap->image_data + PSD_MAX(bottom, 0) * width;
		for(i = PSD_MAX(bottom, 0); i < height; i ++)
		{
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}
	}

	if(left >= 0)
	{
		dst_data = bitmap->image_data + PSD_MAX(top, 0) * width;
		for(i = PSD_MAX(top, 0); i < PSD_MIN(bottom, height); i ++)
		{
			psd_color_memset(dst_data, cur_color, PSD_MIN(left + 1, width));
			dst_data += width;
		}
	}

	if(right < width)
	{
		dst_data = bitmap->image_data + PSD_MAX(top, 0) * width + PSD_MAX(right, 0);
		for(i = PSD_MAX(top, 0); i < PSD_MIN(bottom, height); i ++)
		{
			psd_color_memset(dst_data, cur_color, PSD_MIN(width - right, width));
			dst_data += width;
		}
	}

	if(center_x >= 0 && center_x < width)
	{
		dst_data = bitmap->image_data + PSD_MAX(top + 1, 0) * width + center_x;
		for(i = PSD_MAX(top + 1, 0); i < PSD_MIN(bottom - 1, height); i ++)
		{
			distance = PSD_ABS(i - center_y) * PSD_COLOR_SCALE;
			cur_color = color_table[distance];
			*dst_data = cur_color;
			dst_data += width;
		}
	}

	if(center_y >= 0 && center_y < height)
	{
		dst_data = bitmap->image_data + center_y * width + PSD_MAX(left + 1, 0);
		for(i = PSD_MAX(left + 1, 0); i < PSD_MIN(right - 1, width); i ++)
		{
			distance = PSD_ABS(i - center_x) * PSD_COLOR_SCALE;
			cur_color = color_table[distance];
			*dst_data = cur_color;
			dst_data ++;
		}
	}

	for(i = top + 1; i < center_y; i ++)
	{
		ii = (center_y - i) * (center_y - i);
		i2 = center_y + (center_y - i);
		dst_data = bitmap->image_data + i * width + (left + 1);
		for(j = left + 1; j < center_x; j ++, dst_data ++)
		{
			distance = (psd_int)(psd_carm_sqrt((psd_float)(ii + (center_x - j) * (center_x - j))) * PSD_COLOR_SCALE + 0.5);
			if(distance >= table_count)
				cur_color = color_table[table_count - 1];
			else
				cur_color = color_table[distance];

			j2 = center_x + (center_x - j);
			if(i >= 0 && i < height)
			{
				if(j >= 0 && j < width)
					*dst_data = cur_color;
				if(j2 >= 0 && j2 < width)
					*(dst_data + (j2 - j)) = cur_color;
			}
			if(i2 >= 0 && i2 < height)
			{
				if(j >= 0 && j < width)
					*(dst_data + (i2 - i) * width) = cur_color;
				if(j2 >= 0 && j2 < width)
					*(dst_data + (i2 - i) * width + (j2 - j)) = cur_color;
			}
		}
	}

	psd_free(color_table);
	
	return psd_status_done;
}

psd_status psd_gradient_fill_angle(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse, 
	psd_int center_x, psd_int center_y, psd_int angle)
{
	psd_int left, top, width, height, i, j, i2, j2, dx, dy, tan_angle, cur_angle;
	psd_argb_color * color_table;
	psd_argb_color * dst_data, cur_color;

	width = bitmap->width;
	height = bitmap->height;
	left = center_x < width / 2 ? center_x - (width - center_x) : 0;
	top = center_y < height / 2 ? center_y - (height - center_y) : 0;

	color_table = (psd_argb_color *)psd_malloc(360 * 4);
	if(color_table == NULL)
		return psd_status_malloc_failed;
	psd_gradient_color_get_table(gradient_color, color_table, 360, psd_true - reverse);

	if(center_x >= 0 && center_x < width)
	{
		if(center_y > 0)
		{
			cur_angle = (90 - angle + 360) % 360;
			cur_color = color_table[cur_angle];
			dst_data = bitmap->image_data + center_x;
			for(i = 0; i < PSD_MIN(center_y, height); i ++)
			{
				*dst_data = cur_color;
				dst_data += width;
			}
		}
		if(center_y + 1 < height)
		{
			cur_angle = (270 - angle + 360) % 360;
			cur_color = color_table[cur_angle];
			dst_data = bitmap->image_data + PSD_MAX(center_y + 1, 0) * width + center_x;
			for(i = PSD_MAX(center_y + 1, 0); i < height; i ++)
			{
				*dst_data = cur_color;
				dst_data += width;
			}
		}
	}

	if(center_y >= 0 && center_y < height)
	{
		if(center_x > 0)
		{
			cur_angle = (180 - angle + 360) % 360;
			cur_color = color_table[cur_angle];
			dst_data = bitmap->image_data + center_y * width;
			psd_color_memset(dst_data, cur_color, PSD_MIN(center_x, width));
		}
		if(center_x + 1 < width)
		{
			cur_angle = (-angle + 360) % 360;
			cur_color = color_table[cur_angle];
			dst_data = bitmap->image_data + center_y * width + PSD_MAX(center_x + 1, 0);
			psd_color_memset(dst_data, cur_color, width - PSD_MAX(center_x + 1, 0));
		}
	}

	if(center_x >= 0 && center_x < width && center_y >= 0 && center_y < height)
	{
		dst_data = bitmap->image_data + center_y * width + center_x;
		*dst_data = color_table[0];
	}

	for(i = top; i < center_y; i ++)
	{
		i2 = center_y + (center_y - i);
		dy = center_y - i;
		dst_data = bitmap->image_data + i * width + left;
		for(j = left; j < center_x; j ++, dst_data ++)
		{
			j2 = center_x + (center_x - j);
			dx = center_x - j;
			if(dx >= dy)
				tan_angle = (dy * 45 + (dx >> 1)) / dx;
			else
				tan_angle = 90 - (dx * 45 + (dy >> 1)) / dy;
			tan_angle = PSD_CONSTRAIN(tan_angle, 1, 89);
			cur_angle = 90 - tan_angle - angle + 90;
			if(cur_angle < 0)
				cur_angle += 360;
			else if(cur_angle >= 360)
				cur_angle -= 360;

			if(j >= 0 && j < width)
			{
				if(i >= 0 && i < height)
					*dst_data = color_table[cur_angle];
				if(i2 >= 0 && i2 < height)
				{
					cur_angle += tan_angle * 2;
					if(cur_angle >= 360)
						cur_angle -= 360;
					*(dst_data + (i2 - i) * width) = color_table[cur_angle];
					cur_angle += 180 - tan_angle * 2;
					if(cur_angle >= 360)
						cur_angle -= 360;
				}
				else
				{
					cur_angle += 180;
					if(cur_angle >= 360)
						cur_angle -= 360;
				}
			}
			else
			{
				cur_angle += 180;
				if(cur_angle >= 360)
					cur_angle -= 360;
			}

			if(j2 >= 0 && j2 < width)
			{
				if(i2 >= 0 && i2 < height)
					*(dst_data + (i2 - i) * width + (j2 - j)) = color_table[cur_angle];
				if(i >= 0 && i < height)
				{
					cur_angle += tan_angle * 2;
					if(cur_angle >= 360)
						cur_angle -= 360;
					*(dst_data + (j2 - j)) = color_table[cur_angle];
				}
			}
		}
	}

	psd_free(color_table);
	
	return psd_status_done;
}

psd_status psd_gradient_fill_reflected(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse,
	psd_int start_x, psd_int start_y, psd_int end_x, psd_int end_y)
{
	psd_int distance, table_count;
	psd_argb_color * color_table;
	psd_int temp, i, j, width, height, index;
	psd_argb_color * dst_data, cur_color;
	psd_fixed_16_16 delta_x, delta_y, line_x, pixel_x;

	width = bitmap->width;
	height = bitmap->height;

	if(start_x == end_x)
	{
		table_count = PSD_ABS(end_y - start_y);
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		if(start_y > end_y)
		{
			temp = start_y;
			start_y = end_y;
			end_y = temp;
		}
		psd_gradient_color_get_table(gradient_color, color_table, table_count / 2, psd_true - reverse);
		psd_gradient_color_get_table(gradient_color, color_table + table_count / 2, table_count - table_count / 2, reverse);

		dst_data = bitmap->image_data;
		cur_color = color_table[0];
		for(i = 0; i < PSD_MIN(start_y, height); i ++)
		{
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}

		for(i = PSD_MAX(start_y, 0); i < PSD_MIN(end_y, height); i ++)
		{
			cur_color = color_table[i - start_y];
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}

		cur_color = color_table[table_count - 1];
		for(i = PSD_MAX(end_y, 0); i < height; i ++)
		{
			psd_color_memset(dst_data, cur_color, width);
			dst_data += width;
		}
	}
	
	else if(start_y == end_y)
	{
		table_count = PSD_ABS(end_x - start_x);
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		if(start_x > end_x)
		{
			temp = start_x;
			start_x = end_x;
			end_x = temp;
		}
		psd_gradient_color_get_table(gradient_color, color_table, table_count / 2, psd_true - reverse);
		psd_gradient_color_get_table(gradient_color, color_table + table_count / 2, table_count - table_count / 2, reverse);

		dst_data = bitmap->image_data;
		cur_color = color_table[0];
		for(i = 0; i < PSD_MIN(start_x, width); i ++)
		{
			*dst_data = cur_color;
			dst_data ++;
		}

		for(i = PSD_MAX(start_x, 0); i < PSD_MIN(end_x, width); i ++)
		{
			cur_color = color_table[i - start_x];
			*dst_data = cur_color;
			dst_data ++;
		}

		cur_color = color_table[table_count - 1];
		for(i = PSD_MAX(end_x, 0); i < width; i ++)
		{
			*dst_data = cur_color;
			dst_data ++;
		}

		for(i = 1; i < height; i ++)
		{
			memcpy(dst_data, bitmap->image_data, width * 4);
			dst_data += width;
		}
	}
	
	else
	{
		distance = (psd_int)psd_carm_sqrt((psd_float)((end_x - start_x) * (end_x - start_x) + (end_y - start_y) * (end_y - start_y)) + 0.5f);
		table_count = distance * PSD_COLOR_SCALE;
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		psd_gradient_color_get_table(gradient_color, color_table, table_count / 2, psd_true - reverse);
		psd_gradient_color_get_table(gradient_color, color_table + table_count / 2, table_count - table_count / 2, reverse);

		delta_x = PSD_FIXED_16_16_INT(end_x - start_x) / distance;
		delta_y = PSD_FIXED_16_16_INT(end_y - start_y) / distance;
		line_x = -delta_y * start_y - delta_x * start_x;
		line_x *= PSD_COLOR_SCALE;
		delta_x *= PSD_COLOR_SCALE;
		delta_y *= PSD_COLOR_SCALE;
		dst_data = bitmap->image_data;
		for(i = 0; i < height; i ++)
		{
			pixel_x = line_x;
			for(j = 0; j < width; j ++)
			{
				index =  PSD_FIXED_16_16_ROUND(pixel_x);
				if(index <= 0)
					cur_color = color_table[0];
				else if(index >= table_count - 1)
					cur_color = color_table[table_count - 1];
				else
					cur_color = color_table[index];
				*dst_data = cur_color;
				dst_data ++;
				pixel_x += delta_x;
			}
			line_x += delta_y;
		}
	}
	
	psd_free(color_table);
	
	return psd_status_done;
}

psd_status psd_gradient_fill_diamond(psd_bitmap * bitmap, psd_gradient_color * gradient_color, psd_bool reverse,
	psd_int center_x, psd_int center_y, psd_int radius, psd_int angle)
{
	psd_int width, height, i, j, dx, dy;
	psd_float ax, ay, xy;
	psd_int distance, table_count;
	psd_argb_color * color_table;
	psd_argb_color * dst_data, cur_color;
	psd_fixed_16_16 angle_sin, angle_cos;
	
	angle = (angle + 360) % 90;
	width = bitmap->width;
	height = bitmap->height;

	if(angle == 0)
	{
		table_count = radius;
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		psd_gradient_color_get_table(gradient_color, color_table, table_count, reverse);

		dst_data = bitmap->image_data;
		for(i = 0; i < height; i ++)
		{
			for(j = 0; j < width; j ++, dst_data ++)
			{
				distance = PSD_ABS(j - center_x) + PSD_ABS(i - center_y);
				if(distance >= table_count)
					cur_color = color_table[table_count - 1];
				else
					cur_color = color_table[distance];
				*dst_data = cur_color;
			}
		}
	}
	else
	{
		table_count = radius * PSD_COLOR_SCALE;
		color_table = (psd_argb_color *)psd_malloc(table_count * 4);
		if(color_table == NULL)
			return psd_status_malloc_failed;
		psd_gradient_color_get_table(gradient_color, color_table, table_count, reverse);

		if(angle <= 45)
		{
			ax = 1;
			ay = angle / 45.0f;
		}
		else
		{
			ay = 1;
			ax = (90 - angle) / 45.0f;
		}
		xy = psd_carm_sqrt(ax * ax + ay * ay);
		angle_sin = psd_fixed_16_16_float(ax / xy);
		angle_cos = psd_fixed_16_16_float(ay / xy);

		dst_data = bitmap->image_data;
		for(i = 0; i < height; i ++)
		{
			for(j = 0; j < width; j ++, dst_data ++)
			{
				dx = (j - center_x) * angle_cos + (i - center_y) * angle_sin;
				dy = -(j - center_x) * angle_sin + (i - center_y) * angle_cos;
				distance = PSD_FIXED_16_16_ROUND((PSD_ABS(dx) + PSD_ABS(dy)) * PSD_COLOR_SCALE);
				if(distance >= table_count)
					cur_color = color_table[table_count - 1];
				else
					cur_color = color_table[distance];
				*dst_data = cur_color;
			}
		}
	}
	
	psd_free(color_table);
	
	return psd_status_done;
}

