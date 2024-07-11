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
 * $Id: rect.c, created by Patrick in 2005.06.01, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_rect.h"
#include "psd_math.h"


psd_bool psd_incept_rect(psd_rect * r1, psd_rect * r2, psd_rect * dst_rect)
{
	dst_rect->left = PSD_MAX(r1->left, r2->left);
	dst_rect->right = PSD_MIN(r1->right, r2->right);
	dst_rect->top = PSD_MAX(r1->top, r2->top);
	dst_rect->bottom = PSD_MIN(r1->bottom, r2->bottom);

	if(dst_rect->left >= dst_rect->right || dst_rect->top >= dst_rect->bottom)
		return psd_false;

	return psd_true;
}

psd_bool psd_equal_rect(psd_rect * r1, psd_rect * r2)
{
	if((r1->left == r2->left) && (r1->right == r2->right) 
		&&(r1->top == r2->top) && (r1->bottom == r2->bottom) )
		return psd_true;

	return psd_false;
}

psd_bool psd_subtract_rect(psd_rect * r1, psd_rect * r2, psd_rect * dst_rect)
{
	if(r1->left == r2->left && r1->right == r2->right)
	{
		if(r1->top == r2->top)
		{
			if(r1->bottom < r2->bottom)
				psd_make_rect(dst_rect, r1->left, r1->bottom, r1->right, r2->bottom);
			else
				psd_make_rect(dst_rect, r1->left, r2->bottom, r1->right, r1->bottom);
		}
		else if(r1->bottom == r2->bottom)
		{
			if(r1->top < r2->top)
				psd_make_rect(dst_rect, r1->left, r1->top, r1->right, r2->top);
			else
				psd_make_rect(dst_rect, r1->left, r2->top, r1->right, r1->top);
		}
		else
			return psd_false;
		
		return psd_true;
	}
	else if(r1->top == r2->top && r1->bottom == r2->bottom)
	{
		if(r1->left == r2->left)
		{
			if(r1->right < r2->right)
				psd_make_rect(dst_rect, r1->right, r1->top, r2->right, r1->bottom);
			else
				psd_make_rect(dst_rect, r2->right, r1->top, r1->right, r1->bottom);
		}
		else if(r1->right == r2->right)
		{
			if(r1->left < r2->left)
				psd_make_rect(dst_rect, r1->left, r1->top, r2->left, r1->bottom);
			else
				psd_make_rect(dst_rect, r2->left, r1->top, r1->left, r1->bottom);
		}
		else
			return psd_false;
		
		return psd_true;
	}

	return psd_false;
}

void psd_make_rect(psd_rect * rc, psd_int left, psd_int top, psd_int right, psd_int bottom)
{
	rc->left = left;
	rc->top = top;
	rc->right = right;
	rc->bottom = bottom;
}

void psd_dup_rect(psd_rect * dst, psd_rect * src)
{
	memcpy(dst, src, sizeof(psd_rect));
}

psd_int psd_rect_width(psd_rect * rc)
{
	return (rc->right - rc->left);
}

psd_int psd_rect_height(psd_rect * rc)
{
	return (rc->bottom - rc->top);
}

void psd_offset_rect(psd_rect * rc, psd_int dlt_x, psd_int dlt_y)
{
	rc->left = rc->left + dlt_x;
	rc->right = rc->right + dlt_x;
	rc->top = rc->top + dlt_y;
	rc->bottom = rc->bottom + dlt_y;
}

void psd_inflate_rect(psd_rect * rc, psd_int dlt_x, psd_int dlt_y)
{
	if(rc->left - dlt_x > rc->right + dlt_x)
		dlt_x = psd_rect_width(rc) / 2;
	if(rc->top - dlt_y > rc->bottom + dlt_y)
		dlt_y = psd_rect_height(rc) / 2;
	
	rc->left -= dlt_x ;
	rc->right += dlt_x ;
	rc->top -= dlt_y ;
	rc->bottom += dlt_y ;
}

psd_bool psd_point_in_rect(psd_rect * rc, psd_int x, psd_int y)
{
	if(x >= rc->left && y >= rc->top && x < rc->right && y < rc->bottom)
		return psd_true;
	
	return psd_false;
}

psd_bool psd_is_empty_rect(psd_rect * rc)
{
	if(rc->right <= rc->left || rc->bottom <= rc->top)
		return psd_true;

	return psd_false;
}

