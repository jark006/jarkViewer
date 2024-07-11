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
 * $Id: color.c, created by Patrick in 2006.05.18, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_color.h"
#include "psd_math.h"


psd_argb_color psd_argb_to_color(psd_color_component alpha, psd_color_component red, 
	psd_color_component green, psd_color_component blue)
{
	return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

psd_argb_color psd_rgb_to_color(psd_color_component red, psd_color_component green, 
	psd_color_component blue)
{
	return (0xFF << 24) | (red << 16) | (green << 8) | blue;
}

psd_argb_color psd_cmyk_to_color(psd_double cyan, psd_double magenta, psd_double yellow, psd_double black)
{
	psd_int red, green, blue;

	red = (psd_int)( 1.0 - ( cyan *( 1 - black ) + black ) ) * 255;
	green = (psd_int)( 1.0 - ( magenta *( 1 - black ) + black ) ) * 255;
	blue = (psd_int)( 1.0 - ( yellow *( 1 - black ) + black ) ) * 255;
	red = PSD_CONSTRAIN(red, 0, 255);
	green = PSD_CONSTRAIN(green, 0, 255);
	blue = PSD_CONSTRAIN(blue, 0, 255);

	return PSD_RGB_TO_COLOR(red, green, blue);
}

psd_argb_color psd_acmyk_to_color(psd_color_component alpha, psd_double cyan, 
	psd_double magenta, psd_double yellow, psd_double black)
{
	psd_int red, green, blue;

	red = (psd_int)( 1.0 - ( cyan *( 1 - black ) + black ) ) * 255;
	green = (psd_int)( 1.0 - ( magenta *( 1 - black ) + black ) ) * 255;
	blue = (psd_int)( 1.0 - ( yellow *( 1 - black ) + black ) ) * 255;

	return PSD_ARGB_TO_COLOR(alpha, red, green, blue);
}

psd_argb_color psd_intcmyk_to_color(psd_int cyan, psd_int magenta, psd_int yellow, psd_int black)
{
	psd_int red, green, blue;

	red = (65535 - (cyan * (255 - black) + (black << 8))) >> 8;
	green = (65535 - (magenta * (255 - black) + (black << 8))) >> 8;
	blue = (65535 - (yellow * (255 - black) + (black << 8))) >> 8;

	return PSD_RGB_TO_COLOR(red, green, blue);
}

psd_argb_color psd_intacmyk_to_color(psd_color_component alpha, psd_int cyan, psd_int magenta, psd_int yellow, psd_int black)
{
	psd_int red, green, blue;

	red = (65535 - (cyan * (255 - black) + (black << 8))) >> 8;
	green = (65535 - (magenta * (255 - black) + (black << 8))) >> 8;
	blue = (65535 - (yellow * (255 - black) + (black << 8))) >> 8;

	return PSD_ARGB_TO_COLOR(alpha, red, green, blue);
}

psd_argb_color psd_lab_to_color(psd_int lightness, psd_int a, psd_int b)
{
	return psd_alab_to_color(255, lightness, a, b);
}

psd_argb_color psd_alab_to_color(psd_color_component alpha, psd_int lightness, psd_int a, psd_int b)
{
	// For the conversion we first convert values to XYZ and then to RGB
	// Standards used Observer = 2, Illuminant = D65
	// ref_X = 95.047, ref_Y = 100.000, ref_Z = 108.883
	psd_double x, y, z;
	const psd_double ref_x = 95.047;
	const psd_double ref_y = 100.000;
	const psd_double ref_z = 108.883;

	psd_double var_y = ( (psd_double)lightness + 16.0 ) / 116.0;
	psd_double var_x = (psd_double)a / 500.0 + var_y;
	psd_double var_z = var_y - (psd_double)b / 200.0;

	if ( pow(var_y, 3) > 0.008856 )
		var_y = pow(var_y, 3);
	else
		var_y = ( var_y - 16 / 116 ) / 7.787;

	if ( pow(var_x, 3) > 0.008856 )
		var_x = pow(var_x, 3);
	else
		var_x = ( var_x - 16 / 116 ) / 7.787;

	if ( pow(var_z, 3) > 0.008856 )
		var_z = pow(var_z, 3);
	else
		var_z = ( var_z - 16 / 116 ) / 7.787;

	x = ref_x * var_x;
	y = ref_y * var_y;
	z = ref_z * var_z;

	return psd_axyz_to_color(alpha, x, y, z);
}

psd_argb_color psd_xyz_to_color(psd_double x, psd_double y, psd_double z)
{
	return psd_axyz_to_color(255, x, y, z);
}

psd_argb_color psd_axyz_to_color(psd_color_component alpha, psd_double x, psd_double y, psd_double z)
{
	// Standards used Observer = 2, Illuminant = D65
	// ref_X = 95.047, ref_Y = 100.000, ref_Z = 108.883
	const psd_double ref_x = 95.047;
	const psd_double ref_y = 100.000;
	const psd_double ref_z = 108.883;

	psd_double var_x = x / 100.0;
	psd_double var_y = y / 100.0;
	psd_double var_z = z / 100.0;

	psd_double var_r = var_x * 3.2406 + var_y * (-1.5372) + var_z * (-0.4986);
	psd_double var_g = var_x * (-0.9689) + var_y * 1.8758 + var_z * 0.0415;
	psd_double var_b = var_x * 0.0557 + var_y * (-0.2040) + var_z * 1.0570;

	psd_int red, green, blue;

	if ( var_r > 0.0031308 )
		var_r = 1.055 * ( pow(var_r, 1/2.4) ) - 0.055;
	else
		var_r = 12.92 * var_r;

	if ( var_g > 0.0031308 )
		var_g = 1.055 * ( pow(var_g, 1/2.4) ) - 0.055;
	else
		var_g = 12.92 * var_g;

	if ( var_b > 0.0031308 )
		var_b = 1.055 * ( pow(var_b, 1/2.4) )- 0.055;
	else
		var_b = 12.92 * var_b;

	red = (psd_int)(var_r * 256.0);
	green = (psd_int)(var_g * 256.0);
	blue = (psd_int)(var_b * 256.0);

	return PSD_ARGB_TO_COLOR(alpha, red, green, blue);
}

static psd_int psd_hue_to_color(psd_int hue, psd_double m1, psd_double m2)
{
	psd_double v;
	
	hue %= 360;
	if(hue < 60)
		v = m1 + (m2 - m1) * hue / 60;
	else if(hue < 180)
		v = m2;
	else if(hue < 240)
		v = m1 + (m2 - m1) * (240 - hue) / 60;
	else
		v = m1;

	return (psd_int)(v * 255);
}

psd_argb_color psd_hsb_to_color(psd_int hue, psd_double saturation, psd_double brightness)
{
	return psd_ahsb_to_color(255, hue, saturation, brightness);
}

psd_argb_color psd_ahsb_to_color(psd_color_component alpha, psd_int hue, psd_double saturation, psd_double brightness)
{
	psd_double m1, m2;
	psd_int red, green, blue;

	if(saturation == 0.0)
	{
		blue = green = red = (psd_int)(255 * brightness);
	}
	else
	{
		if(brightness <= 0.5)
			m2 = brightness * (1 + saturation);
		else
			m2 = brightness + saturation - brightness * saturation;
		m1 = 2 * brightness - m2;
		red = psd_hue_to_color(hue + 120, m1, m2);
		green = psd_hue_to_color(hue, m1, m2);
		blue = psd_hue_to_color(hue - 120, m1, m2);
	}

	return PSD_ARGB_TO_COLOR(alpha, red, green, blue);
}

psd_color_component psd_get_alpha_component(psd_argb_color color)
{
	return color >> 24;
}

psd_color_component psd_get_red_component(psd_argb_color color)
{
	return (color & 0x00FF0000) >> 16;
}

psd_color_component psd_get_green_component(psd_argb_color color)
{
	return (color & 0x0000FF00) >> 8;
}

psd_color_component psd_get_blue_component(psd_argb_color color)
{
	return color & 0x000000FF;
}

psd_status psd_color_space_to_argb(psd_argb_color * dst_color, psd_color_space color_space, psd_ushort color_component[4])
{
	switch(color_space)
	{
		case psd_color_space_rgb:
			*dst_color = psd_rgb_to_color(
				(psd_color_component)color_component[0], 
				(psd_color_component)color_component[1], 
				(psd_color_component)color_component[2]);
			break;
			
		case psd_color_space_hsb:
			*dst_color = psd_hsb_to_color(
				color_component[0], 
				color_component[1] / 100.0, 
				color_component[2] / 100.0);
			break;
			
		case psd_color_space_cmyk:
			*dst_color = psd_cmyk_to_color(
				color_component[0] / 100.0, 
				color_component[1] / 100.0, 
				color_component[2] / 100.0, 
				color_component[3] / 100.0);
			break;
			
		case psd_color_space_lab:
			*dst_color = psd_lab_to_color(
				color_component[0], 
				color_component[1], 
				color_component[2]);
			break;
			
		default:
			*dst_color = psd_color_clear;
			return psd_status_unsupport_color_space;
	}

	return psd_status_done;
}

void psd_color_memset(psd_argb_color * bits, psd_argb_color color, psd_int length)
{
	psd_int bytes;

	if(length <= 0)
		return;
	
	*bits = color;
	length *= 4;
	length -= 4;
	bytes = 4;
	
	while(length > 0)
	{
		memcpy((psd_uchar *)bits + bytes, bits, length < bytes ? length : bytes);
		length -= bytes;
		bytes <<= 1;
	}
}

// hue is from 0 to 359
void psd_rgb_to_inthsb(psd_int red, psd_int green, psd_int blue, psd_int * hue, psd_int * saturation, psd_int * brightness)
{
	psd_int cmax, cmin, d;
	psd_int h;

	cmax = PSD_MAX(red, PSD_MAX(green, blue));
	cmin = PSD_MIN(red, PSD_MIN(green, blue));
	*brightness = (cmax + cmin + 1) >> 1;

	if(cmax == cmin)
	{
		*hue = 0;
		*saturation = 0;
	}
	else
	{
		d = cmax - cmin;
		if(*brightness < 128)
			*saturation = d * 255 / (cmax + cmin);
		else
			*saturation = d * 255 / (511 - cmax - cmin);
		
		if(red == cmax)
			h = (green - blue) * 60 / d;
		else if(green == cmax)
			h = 120 + (blue - red) * 60 / d;
		else
			h = 240 + (red - green) * 60 / d;
		*hue = (h + 360) % 360;
	}
}

#define HUE_TO_COLOR(hue, m1, m2, V)							\
do {															\
	psd_int h;													\
	h = (hue + 360) % 360;										\
	if(h < 60)				/*60 = 360 / 6;*/					\
		V = m1 + (m2 - m1) * h / 60;							\
	else if(h < 180)		/*180 = 360 / 2;*/					\
		V = m2;													\
	else if(h < 240)		/*240 = 360 * 2 / 3;*/				\
		V = m1 + (m2 - m1) * (240 - h) / 60;					\
	else														\
		V = m1;													\
} while(0)

void psd_inthsb_to_rgb(psd_int hue, psd_int saturation, psd_int brightness, psd_int * red, psd_int * green, psd_int * blue)
{
	psd_int m1, m2;

	if(saturation == 0)
	{
		*red = *green = *blue = brightness;
	}
	else
	{
		if(brightness < 128)
			m2 = brightness + (brightness * saturation >> 8);
		else
			m2 = brightness + saturation - (brightness * saturation >> 8);
		m1 = brightness * 2 - m2;
		HUE_TO_COLOR(hue + 120, m1, m2, *red);
		HUE_TO_COLOR(hue, m1, m2, *green);
		HUE_TO_COLOR(hue - 120, m1, m2, *blue);
	}
}

psd_int psd_rgb_get_brightness(psd_int red, psd_int green, psd_int blue)
{
	psd_int cmax, cmin;
	
	cmax = PSD_MAX(red, PSD_MAX(green, blue));
	cmin = PSD_MIN(red, PSD_MIN(green, blue));

	return (cmax + cmin + 1) >> 1;
}

void psd_rgb_to_intcmyk(psd_int red, psd_int green, psd_int blue, 
	psd_int * cyan, psd_int * magenta, psd_int * yellow, psd_int * black)
{
	psd_int dst_cyan, dst_magenta, dst_yellow, dst_black;
	
	dst_cyan = 255 - red;
	dst_magenta = 255 - green;
	dst_yellow = 255 - blue;

	dst_black = PSD_MIN(dst_cyan, PSD_MIN(dst_magenta, dst_yellow));
	if(dst_black < 255)
	{
		*cyan = (dst_cyan - dst_black) * 255 / (255 - dst_black);
		*magenta = (dst_magenta - dst_black) * 255 / (255 - dst_black);
		*yellow = (dst_yellow - dst_black) * 255 / (255 - dst_black);
	}
	else
	{
		*cyan = 0;
		*magenta = 0;
		*yellow = 0;
	}
	*black = dst_black;
}

void psd_intcmyk_to_rgb(psd_int cyan, psd_int magenta, psd_int yellow, psd_int black, 
	psd_int * red, psd_int * green, psd_int * blue)
{
	*red = (65535 - (cyan * (255 - black) + (black << 8))) >> 8;
	*green = (65535 - (magenta * (255 - black) + (black << 8))) >> 8;
	*blue = (65535 - (yellow * (255 - black) + (black << 8))) >> 8;
}

