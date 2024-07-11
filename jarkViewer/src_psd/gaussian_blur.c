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
 * $Id: gaussian_blur.c, created by Patrick in 2006.06.26, libpsd@graphest.com Exp $
 */

#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_color.h"
#include "psd_bitmap.h"
#include "psd_math.h"


/*
 * The equations: g(r) = exp (- r^2 / (2 * sigma^2))
 *                   r = sqrt (x^2 + y ^2)
 */
static psd_int * psd_make_curve(psd_double sigma, psd_int *length)
{
	psd_int    *curve;
	psd_double  sigma2;
	psd_double  l;
	psd_int     temp;
	psd_int     i, n;

	sigma2 = 2 * sigma * sigma;
	l = sqrt(-sigma2 * log(1.0 / 255.0));

	n = (psd_int)(ceil(l) * 2);
	if ((n % 2) == 0)
		n += 1;

	curve = (psd_int *)psd_malloc(n * 4);

	*length = n / 2;
	curve += *length;
	curve[0] = 255;

	for (i = 1; i <= *length; i++)
	{
		temp = (psd_int)(exp(- (i * i) / sigma2) * 255);
		curve[-i] = temp;
		curve[i] = temp;
	}

	return curve;
}

static void psd_run_length_encode (psd_color_component *src, psd_int *dest, psd_int width)
{
	psd_int start;
	psd_int i;
	psd_int j;
	psd_color_component last;

	last = *src;
	start = 0;

	for (i = 1; i < width; i++)
	{
		if (*src != last)
		{
			for (j = start; j < i; j++)
			{
				*dest++ = (i - j);
				*dest++ = last;
			}
			start = i;
			last = *src;
		}
		src ++;
	}

	for (j = start; j < i; j++)
	{
		*dest++ = (i - j);
		*dest++ = last;
	}
}

void psd_bitmap_gaussian_blur_alpha_channel(psd_bitmap * bitmap, psd_double radius)
{
	psd_int     width, height;
	psd_int    *buf, *bb;
	psd_int     pixels;
	psd_int     total = 1, total2;
	psd_int     i, row, col;
	psd_int     start, end;
	psd_int    *curve;
	psd_int    *sum = NULL;
	psd_int     val;
	psd_int     length;
	psd_int     initial_p, initial_m;
	psd_double  std_dev;
	psd_color_component * src, * sp;
	psd_argb_color * src_data, * dst_data;

	if(radius <= 0.0)
		return;

	width  = bitmap->width;
	height = bitmap->height;

	if(width < 1 || height < 1)
		return;

	buf = (psd_int *)psd_malloc(PSD_MAX(width, height) * 2 * 4);
	src = (psd_color_component *)psd_malloc(PSD_MAX(width, height));

	/*  First the vertical pass  */
	radius = fabs(radius) * 1.4;
	std_dev = sqrt(-(radius * radius) / (2 * log(1.0 / 255.0)));

	curve = psd_make_curve (std_dev, &length);
	sum = (psd_int *)psd_malloc((2 * length + 1) * 4);

	sum[0] = 0;
	for(i = 1; i <= length*2; i++)
		sum[i] = curve[i-length-1] + sum[i-1];
	sum += length;

	total = sum[length] - sum[-length];
	total2 = total / 2;

	/*  First, the vertical pass  */
	for(col = 0; col < width; col++)
	{
		for(row = 0, src_data = bitmap->image_data + width + col; row < height - 1; row ++, src_data += width)
			src[row] = PSD_GET_ALPHA_COMPONENT(*src_data);
		src[height - 1] = 0;

		sp = src;

		initial_p = *sp;
		initial_m = *(sp + height - 1);

		/*  Determine a run-length encoded version of the row  */
		psd_run_length_encode(sp, buf, height);

		for(row = 0, dst_data = bitmap->image_data + col; row < height; row ++, dst_data += width)
		{
			start = (row < length) ? -row : -length;
			end = (height <= (row + length) ? (height - row - 1) : length);

			val = 0;
			i = start;
			bb = buf + (row + i) * 2;

			if(start != -length)
				val += initial_p * (sum[start] - sum[-length]);

			while(i < end)
			{
				pixels = bb[0];
				i += pixels;

				if(i > end)
					i = end;

				val += bb[1] * (sum[i] - sum[start]);
				bb += (pixels * 2);
				start = i;
			}

			if(end != length)
				val += initial_m * (sum[length] - sum[end]);

			*dst_data = (*dst_data & 0x00FFFFFF) | ((val + total2) / total << 24);
		}
	}

	/*  Now the horizontal pass  */
	for(row = 0; row < height; row++)
	{
		for(col = 0, src_data = bitmap->image_data + row * width + 1; col < width - 1; col ++, src_data ++)
			src[col] = PSD_GET_ALPHA_COMPONENT(*src_data);
		src[width - 1] = 0;

		sp = src;

		initial_p = *sp;
		initial_m = *(sp + width - 1);

		/*  Determine a run-length encoded version of the row  */
		psd_run_length_encode(sp, buf, width);

		for(col = 0, dst_data = bitmap->image_data + row * width; col < width; col ++, dst_data ++)
		{
			start = (col < length) ? -col : -length;
			end = (width <= (col + length)) ? (width - col - 1) : length;

			val = 0;
			i = start;
			bb = buf + (col + i) * 2;

			if(start != -length)
				val += initial_p * (sum[start] - sum[-length]);

			while(i < end)
			{
				pixels = bb[0];
				i += pixels;

				if(i > end)
					i = end;

				val += bb[1] * (sum[i] - sum[start]);
				bb += (pixels * 2);
				start = i;
			}

			if(end != length)
				val += initial_m * (sum[length] - sum[end]);

			*dst_data = (*dst_data & 0x00FFFFFF) | ((val + total2) / total << 24);
		}
	}

	/*  free buffers  */
	psd_free(curve - length);
	psd_free(sum - length);
	psd_free(buf);
	psd_free(src);
}

