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
 * $Id: fixed.c, created by Patrick in 2006.06.22, libpsd@graphest.com Exp $
 */

#include "libpsd.h"
#include "psd_fixed.h"


psd_fixed_26_6 psd_fixed_26_6_float(psd_float s)
{
	return (psd_fixed_26_6)(s * 64 + 0.5);
}

psd_fixed_26_6 psd_fixed_26_6_int(psd_int i)
{
	return (psd_fixed_26_6)(i << 6);
}

psd_int psd_fixed_26_6_floor(psd_fixed_26_6 f)
{
	return (f >> 6);
}

psd_int psd_fixed_26_6_ceil(psd_fixed_26_6 f)
{
	return ((f + 63) >> 6);
}

psd_int psd_fixed_26_6_round(psd_fixed_26_6 f)
{
	return ((f + 31) >> 6);
}

psd_fixed_16_16 psd_fixed_16_16_float(psd_float s)
{
	return (psd_fixed_16_16)(s * 65536 + 0.5);
}

psd_fixed_16_16 psd_fixed_16_16_int(psd_int i)
{
	return (psd_fixed_16_16)(i << 16);
}

psd_int psd_fixed_16_16_floor(psd_fixed_16_16 f)
{
	return (f >> 16);
}

psd_int psd_fixed_16_16_ceil(psd_fixed_16_16 f)
{
	return ((f + 65535) >> 16);
}

psd_int psd_fixed_16_16_round(psd_fixed_16_16 f)
{
	return ((f + 32767) >> 16);
}

psd_float psd_fixed_16_16_tofloat(psd_fixed_16_16 f)
{
	return (psd_float)f / 65536.0f;
}

psd_fixed_16_16 psd_fixed_16_16_mul(psd_fixed_16_16 first, psd_fixed_16_16 second)
{
	psd_int sign = 1, ai, bi, af, bf, item, tem;
	
	if (first < 0)
	{
		first = -first;
		sign = -sign;
	}
	if(second < 0)	
	{
		second = -second;
		sign = -sign;
	}
	
	if(first <= 46340L && second <= 46340L)
	{
		item = first * second >> 16;
		return (sign < 0 ? -item : item);
	}
	
	af = first & 0xFFFF;
	bf = second & 0xFFFF;
	ai = first >> 16;
	bi = second >> 16;

	item = ai * bi;
	if(item > 32767)
		return (sign < 0 ? -item : item); /* Number is too big */

	tem = af * bf;
	item = (item << 16) + (af * bi) + (ai * bf) + (tem >> 16);
	if(item >= 0x80000000)
		return (sign < 0 ? -item : item); /* Number is too big */

	return (sign < 0 ? -item : item);
}

psd_fixed_16_16 psd_fixed_16_16_div(psd_fixed_16_16 first, psd_fixed_16_16 second)
{
	psd_double first_f, second_f, result;
	
	if(first == 0 || second == 0)
		return 0;
	if(second == PSD_FIXED_16_16_ONE)
		return first;

	first_f = first / 65536.0;
	second_f = second / 65536.0;
	result = first_f / second_f;

	return (psd_fixed_16_16)(result * 65536.0);
}

psd_fixed_8_24 psd_fixed_8_24_float(psd_float s)
{
	return (psd_fixed_8_24)(s * 0x1000000 + 0.5);
}

psd_fixed_8_24 psd_fixed_8_24_int(psd_int i)
{
	return (psd_fixed_8_24)(i << 24);
}

psd_int psd_fixed_8_24_floor(psd_fixed_8_24 f)
{
	return (f >> 24);
}

psd_int psd_fixed_8_24_ceil(psd_fixed_8_24 f)
{
	return ((f + 255) >> 24);
}

psd_int psd_fixed_8_24_round(psd_fixed_8_24 f)
{
	return ((f + 127) >> 24);
}

psd_float psd_fixed_8_24_tofloat(psd_fixed_8_24 f)
{
	return (psd_float)f / 0x1000000;
}

