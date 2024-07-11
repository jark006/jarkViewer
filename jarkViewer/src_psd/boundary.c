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
 * $Id: boundary.c, created by Patrick in 2006.07.19, libpsd@graphest.com Exp $
 *
 * Notice: we use some codes form the libart to implement the boundary and stroke,
 * but we found there is a bug that causes the segment when it's rendering the boundary, 
 * and we don't know how to fix it.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libpsd.h"
#include "psd_system.h"
#include "psd_bitmap.h"
#include "psd_color.h"
#include "psd_math.h"


#define PSD_BOUNDARY_THRESHOLD			24
/* BoundSeg array growth parameter */
#define PSD_BOUNDARY_MAX_SEGS_INC  		2048
#define PSD_EPSILON 					1e-6
#define PSD_EPSILON_2 					1e-12
#define PSD_EPSILON_A 					1e-5 /* Threshold for breaking lines at point insertions */
#define PSD_M_SQRT2         			1.41421356237309504880  /* sqrt(2) */
/* Note: BNEG is 1 for \ lines, and 0 for /. Thus,
   x[(flags & BNEG) ^ 1] <= x[flags & BNEG] */
#define PSD_ART_ACTIVE_FLAGS_BNEG 		1
/* This flag is set if the segment has been inserted into the active
   list. */
#define PSD_ART_ACTIVE_FLAGS_IN_ACTIVE 	2
/* This flag is set when the segment is to be deleted in the
   horiz commit process. */
#define PSD_ART_ACTIVE_FLAGS_DEL 		4
/* This flag is set if the seg_id is a valid output segment. */
#define PSD_ART_ACTIVE_FLAGS_OUT 		8
/* This flag is set if the segment is in the horiz list. */
#define PSD_ART_ACTIVE_FLAGS_IN_HORIZ 	16

typedef struct _psd_gimp_bound_seg
{
	psd_int     	x1;
	psd_int     	y1;
	psd_int     	x2;
	psd_int     	y2;
	psd_bool    	open;
	psd_bool    	visited;
} psd_gimp_bound_seg;

typedef struct _psd_gimp_boundary
{
	/*  The array of segments  */
	psd_gimp_bound_seg *	segs;
	psd_int      		num_segs;
	psd_int      		max_segs;

	/*  The array of vertical segments  */
	psd_int *			vert_segs;

	/*  The empty segment arrays */
	psd_int *			empty_segs_n;
	psd_int *			empty_segs_c;
	psd_int *			empty_segs_l;
	psd_int				max_empty_segs;
} psd_gimp_boundary;

typedef struct _psd_vector2
{
	psd_double			x, y;
} psd_gimp_vector2;

typedef struct _psd_art_point {
	/*< public >*/
	psd_double x, y;
} psd_art_point;

typedef struct _psd_art_drect {
	/*< public >*/
	psd_double x0, y0, x1, y1;
} psd_art_drect;

typedef enum {
	PSD_ART_MOVETO,
	PSD_ART_MOVETO_OPEN,
	PSD_ART_CURVETO,
	PSD_ART_LINETO,
	PSD_ART_END
} psd_art_pathcode;

typedef enum {
	PSD_ART_WIND_RULE_NONZERO,
	PSD_ART_WIND_RULE_INTERSECT,
	PSD_ART_WIND_RULE_ODDEVEN,
	PSD_ART_WIND_RULE_POSITIVE
} psd_art_wind_rule;

typedef enum {
	PSD_ART_BREAK_LEFT = 1,
	PSD_ART_BREAK_RIGHT = 2
} psd_art_break_flags;

/* CURVETO is not allowed! */
typedef struct _psd_art_vpath {
	psd_art_pathcode code;
	psd_double x;
	psd_double y;
} psd_art_vpath;

typedef struct _psd_art_svp_seg {
	psd_int n_points;
	psd_int dir; /* == 0 for "up", 1 for "down" */
	psd_art_drect bbox;
	psd_art_point *points;
} psd_art_svp_seg;

typedef struct _psd_art_svp {
	psd_int n_segs;
	psd_art_svp_seg segs[1];
} psd_art_svp;

typedef struct _psd_gimp_scan_convert
{
	psd_double         	ratio_xy;

	psd_bool        	clip;
	psd_int            	clip_x;
	psd_int            	clip_y;
	psd_int            	clip_w;
	psd_int            	clip_h;

	/* stuff necessary for the _add_polygons API...  :-/  */
	psd_bool        	got_first;
	psd_bool        	need_closing;
	psd_gimp_vector2     first;
	psd_gimp_vector2     prev;

	psd_bool        	have_open;
	psd_int           	num_nodes;
	psd_art_vpath   *vpath;

	psd_art_svp     *svp;      /* Sorted vector path
	               (extension no longer possible)          */

	/* stuff necessary for the rendering callback */
	psd_bitmap *	dst_bmp;
	psd_int				x0;
	psd_int				x1;
} psd_gimp_scan_convert;

typedef struct _psd_art_svp_writer psd_art_svp_writer;
struct _psd_art_svp_writer {
  psd_int (*add_segment)(psd_art_svp_writer *self, psd_int wind_left, psd_int delta_wind,
		      psd_double x, psd_double y);
  void (*add_point)(psd_art_svp_writer *self, psd_int seg_id, psd_double x, psd_double y);
  void (*close_segment)(psd_art_svp_writer *self, psd_int seg_id);
};

typedef struct _psd_art_svp_writer_rewind {
	psd_art_svp_writer super;
	psd_art_wind_rule rule;
	psd_art_svp *svp;
	psd_int n_segs_max;
	psd_int *n_points_max;
} psd_art_svp_writer_rewind;

typedef struct _psd_art_pri_point {
	psd_double x;
	psd_double y;
	void *user_data;
} psd_art_pri_point;

typedef struct _psd_art_pri_q {
	psd_int n_items;
	psd_int n_items_max;
	psd_art_pri_point **items;
} psd_art_pri_q;

typedef struct _psd_art_active_seg		psd_art_active_seg;
struct _psd_art_active_seg {
	psd_int flags;
	psd_int wind_left, delta_wind;
	psd_art_active_seg *left, *right; /* doubly linked list structure */

	const psd_art_svp_seg *in_seg;
	psd_int in_curs;

	psd_double x[2];
	psd_double y0, y1;
	psd_double a, b, c; /* line equation; ax+by+c = 0 for the line, a^2 + b^2 = 1,
		     and a>0 */

	/* bottom point and intersection point stack */
	psd_int n_stack;
	psd_int n_stack_max;
	psd_art_point *stack;

	/* horiz commit list */
	psd_art_active_seg *horiz_left, *horiz_right;
	psd_double horiz_x;
	psd_int horiz_delta_wind;
	psd_int seg_id;
};

typedef struct _psd_art_intersect_ctx {
	const psd_art_svp *in;
	psd_art_svp_writer *out;

	psd_art_pri_q *pq;

	psd_art_active_seg *active_head;

	psd_double y;
	psd_art_active_seg *horiz_first;
	psd_art_active_seg *horiz_last;

	/* segment index of next input segment to be added to pri q */
	psd_int in_curs;
} psd_art_intersect_ctx;

typedef struct _psd_art_svp_render_aa_step {
	psd_int x;
	psd_int delta; /* stored with 16 fractional bits */
} psd_art_svp_render_aa_step;

typedef struct _psd_art_svp_render_aa_iter {
  const psd_art_svp *svp;
  psd_int x0, x1;
  psd_int y;
  psd_int seg_ix;

  psd_int *active_segs;
  psd_int n_active_segs;
  psd_int *cursor;
  psd_double *seg_x;
  psd_double *seg_dx;

  psd_art_svp_render_aa_step *steps;
} psd_art_svp_render_aa_iter;

extern psd_float psd_carm_sqrt(psd_float x);


static psd_bool psd_get_bounds(psd_bitmap * src_bmp, psd_int *x1, psd_int *y1, psd_int *x2, psd_int *y2)
{
	psd_int tx1, tx2, ty1, ty2;
	psd_int i, j, width, height;
	psd_argb_color * image_data;

	/*  go through and calculate the bounds  */
	tx1 = src_bmp->width;
	ty1 = src_bmp->height;
	tx2 = 0;
	ty2 = 0;
	width = src_bmp->width;
	height = src_bmp->height;

	for(i = 0; i < height; i ++)
	{
		if(tx1 > 0)
		{
			image_data = src_bmp->image_data + i * width;
			for(j = 0; j < tx1; j ++)
			{
				if(*image_data > 0x00FFFFFF)
				{
					tx1 = j;
					break;
				}
				image_data ++;
			}
		}

		if(tx2 < width)
		{
			image_data = src_bmp->image_data + i * width + width - 1;
			for(j = width - 1; j >= tx2; j --)
			{
				if(*image_data > 0x00FFFFFF)
				{
					tx2 = j;
					break;
				}
				image_data --;
			}
		}
		else if(tx1 == 0 && tx2 == width)
		{
			break;
		}
	}

	if(tx1 > tx2)
		return psd_false;

	for(i = tx1; i < tx2; i ++)
	{
		if(ty1 > 0)
		{
			image_data = src_bmp->image_data + i;
			for(j = 0; j < ty1; j ++)
			{
				if(*image_data > 0x00FFFFFF)
				{
					ty1 = j;
					break;
				}
				image_data += width;
			}
		}

		if(ty2 < height)
		{
			image_data = src_bmp->image_data + (height - 1) * width + i;
			for(j = height - 1; j >= ty2; j --)
			{
				if(*image_data > 0x00FFFFFF)
				{
					ty2 = j;
					break;
				}
				image_data -= width;
			}
		}
		else if(ty1 == 0 && ty2 == height)
		{
			break;
		}
	}

	if(ty1 > ty2)
		return psd_false;

	*x1 = tx1;
	*y1 = ty1;
	*x2 = tx2;
	*y2 = ty2;

	return psd_true;
}

static psd_gimp_bound_seg * psd_boundary_free(psd_gimp_boundary *boundary, psd_bool free_segs)
{
	psd_gimp_bound_seg *segs = NULL;

	if(free_segs)
		psd_free(boundary->segs);
	else
		segs = boundary->segs;

	psd_free(boundary->vert_segs);
	psd_free(boundary->empty_segs_n);
	psd_free(boundary->empty_segs_c);
	psd_free(boundary->empty_segs_l);

	psd_free(boundary);

	return segs;
}

/*  private functions  */
static psd_gimp_boundary * psd_boundary_new(psd_bitmap * src_bmp, psd_int x1, psd_int y1, 
 	psd_int x2, psd_int y2)
{
	psd_int i;
	psd_gimp_boundary *boundary = (psd_gimp_boundary *)psd_malloc(sizeof(psd_gimp_boundary));
	memset(boundary, 0, sizeof(psd_gimp_boundary));

	/*  array for determining the vertical line segments
	*  which must be drawn
	*/
	boundary->vert_segs = (psd_int *)psd_malloc((x2 + 1) * 4);
	memset(boundary->vert_segs, 0, (x2 + 1) * 4);

	for(i = 0; i <= x2; i++)
		boundary->vert_segs[i] = -1;

	/*  find the maximum possible number of empty segments
	*  given the current mask
	*/
	boundary->max_empty_segs = (x2 - x1) + 3;

	boundary->empty_segs_n = (psd_int *)psd_malloc(boundary->max_empty_segs * 4);
	memset(boundary->empty_segs_n, 0, boundary->max_empty_segs * 4);
	boundary->empty_segs_c = (psd_int *)psd_malloc(boundary->max_empty_segs * 4);
	memset(boundary->empty_segs_c, 0, boundary->max_empty_segs * 4);
	boundary->empty_segs_l = (psd_int *)psd_malloc(boundary->max_empty_segs * 4);
	memset(boundary->empty_segs_l, 0, boundary->max_empty_segs * 4);

	return boundary;
}

static void psd_find_empty_segs(psd_bitmap * src_bmp, psd_int scanline, psd_int empty_segs[],
	psd_int max_empty, psd_int *num_empty, psd_int x1, psd_int y1, psd_int x2, psd_int y2)
{
	psd_argb_color * data;
	psd_int x;
	psd_int val, last;
	psd_int l_num_empty;

	data  = NULL;
	*num_empty = 0;

	if(scanline < y1 || scanline >= y2)
	{
		empty_segs[(*num_empty)++] = 0;
		empty_segs[(*num_empty)++] = 0x7FFFFFFF;
		return;
	}

	empty_segs[(*num_empty)++] = 0;
	last = -1;
	l_num_empty = *num_empty;
	data  = src_bmp->image_data + scanline * src_bmp->width + x1;
	for(x = x1; x < x2; x ++, data ++)
	{
		if(PSD_GET_ALPHA_COMPONENT(*data) > PSD_BOUNDARY_THRESHOLD)
			val = 1;
		else
			val = -1;

		if(last != val)
			empty_segs[l_num_empty++] = x;
		last = val;
	}

	*num_empty = l_num_empty;
	if(last > 0)
		empty_segs[(*num_empty)++] = x;

	empty_segs[(*num_empty)++] = 0x7FFFFFFF;
}

static void psd_boundary_add_seg(psd_gimp_boundary *boundary, psd_int x1, psd_int y1, psd_int x2, psd_int y2,
	psd_bool open)
{
	if(boundary->num_segs >= boundary->max_segs)
	{
		boundary->max_segs += PSD_BOUNDARY_MAX_SEGS_INC;
		boundary->segs = (psd_gimp_bound_seg *)psd_realloc(boundary->segs, sizeof(psd_gimp_bound_seg) * boundary->max_segs);
	}

	boundary->segs[boundary->num_segs].x1 = x1;
	boundary->segs[boundary->num_segs].y1 = y1;
	boundary->segs[boundary->num_segs].x2 = x2;
	boundary->segs[boundary->num_segs].y2 = y2;
	boundary->segs[boundary->num_segs].open = open;
	boundary->num_segs ++;
}

static void psd_process_horiz_seg(psd_gimp_boundary *boundary, psd_int x1, psd_int y1, psd_int x2, psd_int y2,
	psd_bool open)
{
	/*  This procedure accounts for any vertical segments that must be
	drawn to close in the horizontal segments.                     */

	if(boundary->vert_segs[x1] >= 0)
	{
		psd_boundary_add_seg(boundary, x1, boundary->vert_segs[x1], x1, y1, !open);
		boundary->vert_segs[x1] = -1;
	}
	else
		boundary->vert_segs[x1] = y1;

	if(boundary->vert_segs[x2] >= 0)
	{
		psd_boundary_add_seg(boundary, x2, boundary->vert_segs[x2], x2, y2, open);
		boundary->vert_segs[x2] = -1;
	}
	else
		boundary->vert_segs[x2] = y2;

	psd_boundary_add_seg(boundary, x1, y1, x2, y2, open);
}

static void psd_make_horiz_segs(psd_gimp_boundary *boundary, psd_int start, psd_int end, psd_int scanline,
	psd_int empty[], psd_int num_empty, psd_bool open)
{
	psd_int empty_index;
	psd_int e_s, e_e;    /* empty segment start and end values */

	for(empty_index = 0; empty_index < num_empty; empty_index += 2)
	{
		e_s = *empty++;
		e_e = *empty++;

		if(e_s <= start && e_e >= end)
			psd_process_horiz_seg(boundary,
				start, scanline, end, scanline, open);
		else if((e_s > start && e_s < end) ||
			(e_e < end && e_e > start))
			psd_process_horiz_seg(boundary,
				PSD_MAX(e_s, start), scanline,
				PSD_MIN(e_e, end), scanline, open);
	}
}

static psd_gimp_boundary * psd_generate_boundary(psd_bitmap * src_bmp, psd_int x1, psd_int y1, 
 	psd_int x2, psd_int y2)
{
	psd_gimp_boundary *boundary;
	psd_int      scanline;
	psd_int      i;
	psd_int      start, end;
	psd_int     *tmp_segs;
	psd_int      num_empty_n = 0;
	psd_int      num_empty_c = 0;
	psd_int      num_empty_l = 0;

	boundary = psd_boundary_new(src_bmp, x1, y1, x2, y2);

	start = y1;
	end   = y2;

	/*  Find the empty segments for the previous and current scanlines  */
	psd_find_empty_segs(src_bmp, start - 1, boundary->empty_segs_l,
		boundary->max_empty_segs, &num_empty_l,
		x1, y1, x2, y2);
	psd_find_empty_segs(src_bmp, start, boundary->empty_segs_c,
		boundary->max_empty_segs, &num_empty_c,
		x1, y1, x2, y2);

	for(scanline = start; scanline < end; scanline++)
	{
		/*  find the empty segment list for the next scanline  */
		psd_find_empty_segs(src_bmp, scanline + 1, boundary->empty_segs_n,
			boundary->max_empty_segs, &num_empty_n,
			x1, y1, x2, y2);

		/*  process the segments on the current scanline  */
		for(i = 1; i < num_empty_c - 1; i += 2)
		{
			psd_make_horiz_segs(boundary,
				boundary->empty_segs_c [i],
				boundary->empty_segs_c [i+1],
				scanline, boundary->empty_segs_l, num_empty_l, psd_true);
			psd_make_horiz_segs(boundary,
				boundary->empty_segs_c [i],
				boundary->empty_segs_c [i+1],
				scanline + 1, boundary->empty_segs_n, num_empty_n, psd_false);
		}

		/*  get the next scanline of empty segments, swap others  */
		tmp_segs               = boundary->empty_segs_l;
		boundary->empty_segs_l = boundary->empty_segs_c;
		num_empty_l            = num_empty_c;
		boundary->empty_segs_c = boundary->empty_segs_n;
		num_empty_c            = num_empty_n;
		boundary->empty_segs_n = tmp_segs;
	}

	return boundary;
}

static psd_gimp_bound_seg * psd_boundary_find(psd_bitmap * src_bmp, psd_int x1, psd_int y1, 
	psd_int x2, psd_int y2, psd_int *num_segs)
{
	psd_gimp_boundary *boundary;

	boundary = psd_generate_boundary(src_bmp, x1, y1, x2, y2);
	*num_segs = boundary->num_segs;

	return psd_boundary_free(boundary, psd_false);
}

static psd_bool psd_get_boundary(psd_bitmap * src_bmp, psd_gimp_bound_seg **segs, psd_int * num_segs, 
	psd_int *x1, psd_int *y1, psd_int *x2, psd_int *y2)
{
	if(psd_get_bounds(src_bmp, x1, y1, x2, y2) == psd_true)
		*segs = psd_boundary_find(src_bmp, *x1, *y1, *x2, *y2, num_segs);
	else
		return psd_false;

	if(*segs == NULL)
		return psd_false;

	return psd_true;
}

/*  sorting utility functions  */
static psd_int psd_find_segment(psd_gimp_bound_seg *segs, psd_int num_segs, psd_int x, psd_int y)
{
	psd_int index;

	for(index = 0; index < num_segs; index++)
	{
		if(((segs[index].x1 == x && segs[index].y1 == y) ||
			(segs[index].x2 == x && segs[index].y2 == y)) &&
			segs[index].visited == psd_false)
			return index;
	}

	return -1;
}

/**
 * boundary_sort:
 * @segs:       unsorted input segs.
 * @num_segs:   number of input segs
 * @num_groups: number of groups in the sorted segs
 *
 * This function takes an array of #BoundSeg's as returned by
 * boundary_find() and sorts it by contiguous groups. The returned
 * array contains markers consisting of -1 coordinates and is
 * @num_groups elements longer than @segs.
 *
 * Return value: the sorted segs
 **/
static psd_gimp_bound_seg * psd_boundary_sort(psd_gimp_bound_seg *segs, psd_int num_segs, psd_int *num_groups)
{
	psd_gimp_boundary * boundary;
	psd_int i;
	psd_int index;
	psd_int x, y;
	psd_int startx, starty;
	psd_bool empty;
	psd_gimp_bound_seg * new_segs;

	*num_groups = 0;

	for(i = 0; i < num_segs; i++)
		segs[i].visited = psd_false;

	boundary = (psd_gimp_boundary *)psd_malloc(sizeof(psd_gimp_boundary));
	memset(boundary, 0, sizeof(psd_gimp_boundary));

	index    = 0;
	new_segs = NULL;
	empty    = psd_false;

	while(empty == psd_false)
	{
		empty = psd_true;

		/*  find the index of a non-visited segment to start a group  */
		for(i = 0; i < num_segs; i++)
		{
			if(segs[i].visited == psd_false)
			{
				index = i;
				empty = psd_false;
				i = num_segs;
			}
		}

		if(empty == psd_false)
		{
			psd_boundary_add_seg(boundary,
				segs[index].x1, segs[index].y1,
				segs[index].x2, segs[index].y2,
				segs[index].open);

			segs[index].visited = psd_true;

			startx = segs[index].x1;
			starty = segs[index].y1;
			x = segs[index].x2;
			y = segs[index].y2;

			while((index = psd_find_segment(segs, num_segs, x, y)) != -1)
			{
				/*  make sure ordering is correct  */
				if(x == segs[index].x1 && y == segs[index].y1)
				{
					psd_boundary_add_seg(boundary,
				        segs[index].x1, segs[index].y1,
				        segs[index].x2, segs[index].y2,
				        segs[index].open);
					x = segs[index].x2;
					y = segs[index].y2;
				}
				else
				{
					psd_boundary_add_seg(boundary,
				        segs[index].x2, segs[index].y2,
				        segs[index].x1, segs[index].y1,
				        segs[index].open);
					x = segs[index].x1;
					y = segs[index].y1;
				}

				segs[index].visited = psd_true;
			}

			/*  Mark the end of a group  */
			*num_groups = *num_groups + 1;
			psd_boundary_add_seg(boundary, -1, -1, -1, -1, psd_false);
		}
	}

	return psd_boundary_free(boundary, psd_false);
}

static void psd_scan_convert_close_add_points(psd_gimp_scan_convert *sc)
{
	if(sc->need_closing &&
		(sc->prev.x != sc->first.x || sc->prev.y != sc->first.y))
	{
		sc->vpath = (psd_art_vpath *)psd_realloc(sc->vpath, sizeof(psd_art_vpath) * (sc->num_nodes + 2));
		
		sc->vpath[sc->num_nodes].code = PSD_ART_LINETO;
		sc->vpath[sc->num_nodes].x = sc->first.x;
		sc->vpath[sc->num_nodes].y = sc->first.y;
		sc->num_nodes++;
		sc->vpath[sc->num_nodes].code = PSD_ART_END;
		sc->vpath[sc->num_nodes].x = 0.0;
		sc->vpath[sc->num_nodes].y = 0.0;
		sc->num_nodes++;
	}

	sc->need_closing = psd_false;
}

/**
 * gimp_scan_convert_add_polyline:
 * @sc:       a #GimpScanConvert context
 * @n_points: number of points to add
 * @points:   array of points to add
 * @closed:   whether to close the polyline and make it a polygon
 *
 * Add a polyline with @n_points @points that may be open or closed.
 * It is not recommended to mix gimp_scan_convert_add_polyline() with
 * gimp_scan_convert_add_points().
 *
 * Please note that you should use gimp_scan_convert_stroke() if you
 * specify open polygons.
 */
static void psd_scan_convert_add_polyline(psd_gimp_scan_convert *sc,
	psd_int n_points, psd_gimp_vector2 *points, psd_bool closed)
{
	psd_gimp_vector2  prev = { 0.0, 0.0, };
	psd_int         i;

	if(n_points <= 0)
		return;

	if(sc->need_closing)
		psd_scan_convert_close_add_points(sc);

	if(closed == psd_false)
		sc->have_open = psd_true;

	/* make sure that we have enough space for the nodes */
	sc->vpath = (psd_art_vpath *)psd_realloc(sc->vpath, sizeof(psd_art_vpath) * (sc->num_nodes + n_points + 2));

	for(i = 0; i < n_points; i++)
	{
		/* compress multiple identical coordinates */
		if(i == 0 ||
			prev.x != points[i].x ||
			prev.y != points[i].y)
		{
			sc->vpath[sc->num_nodes].code = (i == 0 ? (closed ?
				PSD_ART_MOVETO :
				PSD_ART_MOVETO_OPEN) :
				PSD_ART_LINETO);
			sc->vpath[sc->num_nodes].x = points[i].x;
			sc->vpath[sc->num_nodes].y = points[i].y;
			sc->num_nodes++;
			prev = points[i];
		}
	}

	/* close the polyline when needed */
	if(closed && (prev.x != points[0].x ||
		prev.y != points[0].y))
	{
		sc->vpath[sc->num_nodes].x = points[0].x;
		sc->vpath[sc->num_nodes].y = points[0].y;
		sc->vpath[sc->num_nodes].code = PSD_ART_LINETO;
		sc->num_nodes++;
	}

	sc->vpath[sc->num_nodes].code = PSD_ART_END;
	sc->vpath[sc->num_nodes].x = 0.0;
	sc->vpath[sc->num_nodes].y = 0.0;

	/* If someone wants to mix this function with _add_points ()
	* try to do something reasonable...
	*/

	sc->got_first = psd_false;
}

/**
 * art_svp_free: Free an #ArtSVP structure.
 * @svp: #ArtSVP to free.
 * 
 * Frees an #ArtSVP structure and all the segments in it.
 **/
static void psd_art_svp_free(psd_art_svp *svp)
{
	psd_int n_segs = svp->n_segs;
	psd_int i;

	for(i = 0; i < n_segs; i++)
		psd_free(svp->segs[i].points);
	psd_free(svp);
}

/**
 * gimp_scan_convert_free:
 * @sc: a #GimpScanConvert context
 *
 * Frees the resources allocated for @sc.
 */
static void psd_scan_convert_free(psd_gimp_scan_convert *sc)
{
	if(sc == NULL)
		return;

	if(sc->vpath)
		psd_free(sc->vpath);
	if(sc->svp)
		psd_art_svp_free(sc->svp);

	psd_free(sc);
}

/**
 * art_vpath_add_point: Add point to vpath.
 * @p_vpath: Where the pointer to the #ArtVpath structure is stored.
 * @pn_points: Pointer to the number of points in *@p_vpath.
 * @pn_points_max: Pointer to the number of points allocated.
 * @code: The pathcode for the new point.
 * @x: The X coordinate of the new point.
 * @y: The Y coordinate of the new point.
 *
 * Adds a new point to *@p_vpath, reallocating and updating *@p_vpath
 * and *@pn_points_max as necessary. *@pn_points is incremented.
 *
 * This routine always adds the point after all points already in the
 * vpath. Thus, it should be called in the order the points are
 * desired.
 **/
static void psd_art_vpath_add_point(psd_art_vpath **p_vpath, psd_int *pn_points, psd_int *pn_points_max,
	psd_art_pathcode code, psd_double x, psd_double y)
{
	psd_int i;

	i = (*pn_points)++;
	if(i == *pn_points_max)
	{
		if(i > 0)
		{
			*pn_points_max *= 2;
			*p_vpath = (psd_art_vpath *)psd_realloc(*p_vpath, sizeof(psd_art_vpath) * *pn_points_max);
		}
		else
		{
			*pn_points_max = 1;
			*p_vpath = (psd_art_vpath *)psd_malloc(sizeof(psd_art_vpath));
		}
	}
	
	(*p_vpath)[i].code = code;
	(*p_vpath)[i].x = x;
	(*p_vpath)[i].y = y;
}

/* Render an arc segment starting at (xc + x0, yc + y0) to (xc + x1,
   yc + y1), centered at (xc, yc), and with given radius. Both x0^2 +
   y0^2 and x1^2 + y1^2 should be equal to radius^2.

   A positive value of radius means curve to the left, negative means
   curve to the right.
*/
static void psd_art_svp_vpath_stroke_arc(psd_art_vpath **p_vpath, psd_int *pn, psd_int *pn_max,
	psd_double xc, psd_double yc,
	psd_double x0, psd_double y0,
	psd_double x1, psd_double y1,
	psd_double radius,
	psd_double flatness)
{
	psd_double theta;
	psd_double th_0, th_1;
	psd_int n_pts;
	psd_int i;
	psd_double aradius;

	aradius = PSD_ABS(radius);
	theta = 2 * PSD_M_SQRT2 * (psd_double)psd_carm_sqrt((psd_float)(flatness / aradius));
	th_0 = atan2(y0, x0);
	th_1 = atan2(y1, x1);
	if(radius > 0)
	{
		/* curve to the left */
		if(th_0 < th_1) th_0 += PSD_PI * 2;
			n_pts = (psd_int)ceil ((th_0 - th_1) / theta);
	}
	else
	{
		/* curve to the right */
		if(th_1 < th_0) th_1 += PSD_PI * 2;
			n_pts = (psd_int)ceil((th_1 - th_0) / theta);
	}
	psd_art_vpath_add_point(p_vpath, pn, pn_max,
		PSD_ART_LINETO, xc + x0, yc + y0);
	for(i = 1; i < n_pts; i++)
	{
		theta = th_0 + (th_1 - th_0) * i / n_pts;
		psd_art_vpath_add_point(p_vpath, pn, pn_max,
			PSD_ART_LINETO, xc + cos(theta) * aradius,
			yc + sin(theta) * aradius);
	}
	psd_art_vpath_add_point(p_vpath, pn, pn_max,
		PSD_ART_LINETO, xc + x1, yc + y1);
}

/* Assume that forw and rev are at point i0. Bring them to i1,
   joining with the vector i1 - i2.

   This used to be psd_true, but isn't now that the stroke_raw code is
   filtering out (near)zero length vectors: {It so happens that all
   invocations of this function maintain the precondition i1 = i0 + 1,
   so we could decrease the number of arguments by one. We haven't
   done that here, though.}

   forw is to the line's right and rev is to its left.

   Precondition: no zero-length vectors, otherwise a divide by
   zero will happen.  */
static void psd_render_seg(psd_art_vpath **p_forw, psd_int *pn_forw, psd_int *pn_forw_max,
	psd_art_vpath **p_rev, psd_int *pn_rev, psd_int *pn_rev_max,
	psd_art_vpath *vpath, psd_int i0, psd_int i1, psd_int i2,
	psd_double line_width, psd_double flatness)
{
	psd_double dx0, dy0;
	psd_double dx1, dy1;
	psd_double dlx0, dly0;
	psd_double dlx1, dly1;
	psd_double dmx, dmy;
	psd_double dmr2;
	psd_double scale;
	psd_double cross;

	/* The vectors of the lines from i0 to i1 and i1 to i2. */
	dx0 = vpath[i1].x - vpath[i0].x;
	dy0 = vpath[i1].y - vpath[i0].y;

	dx1 = vpath[i2].x - vpath[i1].x;
	dy1 = vpath[i2].y - vpath[i1].y;

	/* Set dl[xy]0 to the vector from i0 to i1, rotated counterclockwise
	90 degrees, and scaled to the length of line_width. */
	scale = line_width / (psd_double)psd_carm_sqrt((psd_float)(dx0 * dx0 + dy0 * dy0));
	dlx0 = dy0 * scale;
	dly0 = -dx0 * scale;

	/* Set dl[xy]1 to the vector from i1 to i2, rotated counterclockwise
	90 degrees, and scaled to the length of line_width. */
	scale = line_width / (psd_double)psd_carm_sqrt((psd_float)(dx1 * dx1 + dy1 * dy1));
	dlx1 = dy1 * scale;
	dly1 = -dx1 * scale;

	/* now, forw's last point is expected to be colinear along d[xy]0
	to point i0 - dl[xy]0, and rev with i0 + dl[xy]0. */

	/* positive for positive area (i.e. left turn) */
	cross = dx1 * dy0 - dx0 * dy1;

	dmx = (dlx0 + dlx1) * 0.5;
	dmy = (dly0 + dly1) * 0.5;
	dmr2 = dmx * dmx + dmy * dmy;

	/* the case when dmr2 is zero or very small bothers me
	(i.e. near a 180 degree angle)
	ALEX: So, we avoid the optimization when dmr2 is very small. This should
	be safe since dmx/y is only used in optimization and in MITER case, and MITER
	should be converted to BEVEL when dmr2 is very small. */
	if(dmr2 > PSD_EPSILON_2)
	{
		scale = line_width * line_width / dmr2;
		dmx *= scale;
		dmy *= scale;
	}

	if(cross * cross < PSD_EPSILON_2 && dx0 * dx1 + dy0 * dy1 >= 0)
	{
		/* going straight */
		psd_art_vpath_add_point(p_forw, pn_forw, pn_forw_max,
			PSD_ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
		psd_art_vpath_add_point(p_rev, pn_rev, pn_rev_max,
			PSD_ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
	}
	else if(cross > 0)
	{
		/* left turn, forw is outside and rev is inside */

		if((dmr2 > PSD_EPSILON_2) &&
			/* check that i1 + dm[xy] is inside i0-i1 rectangle */
			(dx0 + dmx) * dx0 + (dy0 + dmy) * dy0 > 0 &&
			/* and that i1 + dm[xy] is inside i1-i2 rectangle */
			((dx1 - dmx) * dx1 + (dy1 - dmy) * dy1 > 0))
		{
			/* can safely add single intersection point */
			psd_art_vpath_add_point(p_rev, pn_rev, pn_rev_max,
				PSD_ART_LINETO, vpath[i1].x + dmx, vpath[i1].y + dmy);
		}
		else
		{
			/* need to loop-de-loop the inside */
			psd_art_vpath_add_point(p_rev, pn_rev, pn_rev_max,
				PSD_ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
			psd_art_vpath_add_point(p_rev, pn_rev, pn_rev_max,
				PSD_ART_LINETO, vpath[i1].x, vpath[i1].y);
			psd_art_vpath_add_point(p_rev, pn_rev, pn_rev_max,
				PSD_ART_LINETO, vpath[i1].x + dlx1, vpath[i1].y + dly1);
		}

		psd_art_svp_vpath_stroke_arc(p_forw, pn_forw, pn_forw_max,
			vpath[i1].x, vpath[i1].y,
			-dlx0, -dly0,
			-dlx1, -dly1,
			line_width,
			flatness);
	}
	else
	{
		/* right turn, rev is outside and forw is inside */
		if((dmr2 > PSD_EPSILON_2) &&
			/* check that i1 - dm[xy] is inside i0-i1 rectangle */
			(dx0 - dmx) * dx0 + (dy0 - dmy) * dy0 > 0 &&
			/* and that i1 - dm[xy] is inside i1-i2 rectangle */
			((dx1 + dmx) * dx1 + (dy1 + dmy) * dy1 > 0))
		{
			/* can safely add single intersection point */
			psd_art_vpath_add_point(p_forw, pn_forw, pn_forw_max,
				PSD_ART_LINETO, vpath[i1].x - dmx, vpath[i1].y - dmy);
		}
		else
		{
			/* need to loop-de-loop the inside */
			psd_art_vpath_add_point(p_forw, pn_forw, pn_forw_max,
				PSD_ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
			psd_art_vpath_add_point(p_forw, pn_forw, pn_forw_max,
				PSD_ART_LINETO, vpath[i1].x, vpath[i1].y);
			psd_art_vpath_add_point(p_forw, pn_forw, pn_forw_max,
				PSD_ART_LINETO, vpath[i1].x - dlx1, vpath[i1].y - dly1);
		}

		psd_art_svp_vpath_stroke_arc(p_rev, pn_rev, pn_rev_max,
			vpath[i1].x, vpath[i1].y,
			dlx0, dly0,
			dlx1, dly1,
			-line_width,
			flatness);
	}
}

/* caps i1, under the assumption of a vector from i0 */
static void psd_render_cap(psd_art_vpath **p_result, psd_int *pn_result, psd_int *pn_result_max,
	psd_art_vpath *vpath, psd_int i0, psd_int i1,
	psd_double line_width, psd_double flatness)
{
	psd_double dx0, dy0;
	psd_double dlx0, dly0;
	psd_double scale;
	psd_int n_pts;
	psd_int i;

	dx0 = vpath[i1].x - vpath[i0].x;
	dy0 = vpath[i1].y - vpath[i0].y;

	/* Set dl[xy]0 to the vector from i0 to i1, rotated counterclockwise
	90 degrees, and scaled to the length of line_width. */
	scale = line_width / (psd_double)psd_carm_sqrt((psd_float)(dx0 * dx0 + dy0 * dy0));
	dlx0 = dy0 * scale;
	dly0 = -dx0 * scale;

	n_pts = (psd_int)ceil(PSD_PI / (2.0 * PSD_M_SQRT2 * (psd_double)psd_carm_sqrt((psd_float)(flatness / line_width))));
	psd_art_vpath_add_point(p_result, pn_result, pn_result_max,
		PSD_ART_LINETO, vpath[i1].x - dlx0, vpath[i1].y - dly0);
	for(i = 1; i < n_pts; i++)
	{
		psd_double theta, c_th, s_th;

		theta = PSD_PI * i / n_pts;
		c_th = cos(theta);
		s_th = sin(theta);
		psd_art_vpath_add_point(p_result, pn_result, pn_result_max,
			PSD_ART_LINETO,
			vpath[i1].x - dlx0 * c_th - dly0 * s_th,
			vpath[i1].y - dly0 * c_th + dlx0 * s_th);
	}
	psd_art_vpath_add_point(p_result, pn_result, pn_result_max,
		PSD_ART_LINETO, vpath[i1].x + dlx0, vpath[i1].y + dly0);
}

/**
 * art_svp_from_vpath_raw: Stroke a vector path, raw version
 * @vpath: #ArtVPath to stroke.
 * @join: Join style.
 * @cap: Cap style.
 * @line_width: Width of stroke.
 * @miter_limit: Miter limit.
 * @flatness: Flatness.
 *
 * Exactly the same as art_svp_vpath_stroke(), except that the resulting
 * stroke outline may self-intersect and have regions of winding number
 * greater than 1.
 *
 * Return value: Resulting raw stroked outline in svp format.
 **/
static psd_art_vpath * psd_art_svp_vpath_stroke_raw(psd_art_vpath *vpath, 
	psd_double line_width, psd_double flatness)
{
	psd_int begin_idx, end_idx;
	psd_int i;
	psd_art_vpath *forw, *rev;
	psd_int n_forw, n_rev;
	psd_int n_forw_max, n_rev_max;
	psd_art_vpath *result;
	psd_int n_result, n_result_max;
	psd_double half_lw = 0.5 * line_width;
	psd_int closed;
	psd_int last, this, next, second;
	psd_double dx, dy;

	n_forw_max = 16;
	forw = (psd_art_vpath *)psd_malloc(sizeof(psd_art_vpath) * n_forw_max);

	n_rev_max = 16;
	rev = (psd_art_vpath *)psd_malloc(sizeof(psd_art_vpath) * n_rev_max);

	n_result = 0;
	n_result_max = 16;
	result = (psd_art_vpath *)psd_malloc(sizeof(psd_art_vpath) * n_result_max);

	for(begin_idx = 0; vpath[begin_idx].code != PSD_ART_END; begin_idx = end_idx)
	{
		n_forw = 0;
		n_rev = 0;

		closed = (vpath[begin_idx].code == PSD_ART_MOVETO);

		/* we don't know what the first point joins with until we get to the
		last point and see if it's closed. So we start with the second
		line in the path.

		Note: this is not strictly psd_true (we now know it's closed from
		the opening pathcode), but why fix code that isn't broken?
		*/

		this = begin_idx;
		/* skip over identical points at the beginning of the subpath */
		for(i = this + 1; vpath[i].code == PSD_ART_LINETO; i++)
		{
			dx = vpath[i].x - vpath[this].x;
			dy = vpath[i].y - vpath[this].y;
			if(dx * dx + dy * dy > PSD_EPSILON_2)
				break;
		}
		next = i;
		second = next;

		/* invariant: this doesn't coincide with next */
		while(vpath[next].code == PSD_ART_LINETO)
		{
			last = this;
			this = next;
			/* skip over identical points after the beginning of the subpath */
			for(i = this + 1; vpath[i].code == PSD_ART_LINETO; i++)
			{
				dx = vpath[i].x - vpath[this].x;
				dy = vpath[i].y - vpath[this].y;
				if(dx * dx + dy * dy > PSD_EPSILON_2)
					break;
			}
			next = i;
			if(vpath[next].code != PSD_ART_LINETO)
			{
				/* reached end of path */
				/* make "closed" detection conform to PostScript
				semantics (i.e. explicit closepath code rather than
				just the fact that end of the path is the beginning) */
				if(closed &&
					vpath[this].x == vpath[begin_idx].x &&
					vpath[this].y == vpath[begin_idx].y)
				{
					psd_int j;

					/* path is closed, render join to beginning */
					psd_render_seg(&forw, &n_forw, &n_forw_max,
						&rev, &n_rev, &n_rev_max,
						vpath, last, this, second,
						half_lw, flatness);

					/* do forward path */
					psd_art_vpath_add_point(&result, &n_result, &n_result_max,
						PSD_ART_MOVETO, forw[n_forw - 1].x,
						forw[n_forw - 1].y);
					for(j = 0; j < n_forw; j++)
						psd_art_vpath_add_point(&result, &n_result, &n_result_max,
							PSD_ART_LINETO, forw[j].x,
							forw[j].y);
					
					/* do reverse path, reversed */
					psd_art_vpath_add_point(&result, &n_result, &n_result_max,
						PSD_ART_MOVETO, rev[0].x,
						rev[0].y);
					for(j = n_rev - 1; j >= 0; j--)
						psd_art_vpath_add_point(&result, &n_result, &n_result_max,
							PSD_ART_LINETO, rev[j].x,
							rev[j].y);
				}
				else
				{
					/* path is open */
					psd_int j;

					/* add to forw rather than result to ensure that
					forw has at least one point. */
					psd_render_cap(&forw, &n_forw, &n_forw_max,
						vpath, last, this,
						half_lw, flatness);
					psd_art_vpath_add_point(&result, &n_result, &n_result_max,
						PSD_ART_MOVETO, forw[0].x,
						forw[0].y);
					for(j = 1; j < n_forw; j++)
						psd_art_vpath_add_point(&result, &n_result, &n_result_max,
							PSD_ART_LINETO, forw[j].x,
							forw[j].y);
					for(j = n_rev - 1; j >= 0; j--)
						psd_art_vpath_add_point(&result, &n_result, &n_result_max,
							PSD_ART_LINETO, rev[j].x,
							rev[j].y);
					psd_render_cap(&result, &n_result, &n_result_max,
						vpath, second, begin_idx,
						half_lw, flatness);
					psd_art_vpath_add_point(&result, &n_result, &n_result_max,
						PSD_ART_LINETO, forw[0].x,
						forw[0].y);
				}
			}
			else
			{
				psd_render_seg(&forw, &n_forw, &n_forw_max,
					&rev, &n_rev, &n_rev_max,
					vpath, last, this, next,
					half_lw, flatness);
			}
		}
		end_idx = next;
	}

	psd_free(forw);
	psd_free(rev);
	psd_art_vpath_add_point(&result, &n_result, &n_result_max, PSD_ART_END, 0, 0);
	
	return result;
}

/* reverse a list of points in place */
static void psd_reverse_points(psd_art_point *points, psd_int n_points)
{
	psd_int i;
	psd_art_point tmp_p;

	for(i = 0; i < (n_points >> 1); i++)
	{
		tmp_p = points[i];
		points[i] = points[n_points - (i + 1)];
		points[n_points - (i + 1)] = tmp_p;
	}
}

/**
 * art_svp_seg_compare: Compare two segments of an svp.
 * @seg1: First segment to compare.
 * @seg2: Second segment to compare.
 * 
 * Compares two segments of an svp. Return 1 if @seg2 is below or to the
 * right of @seg1, -1 otherwise.
 **/
static psd_int psd_art_svp_seg_compare(const void *s1, const void *s2)
{
	const psd_art_svp_seg *seg1 = s1;
	const psd_art_svp_seg *seg2 = s2;

	if(seg1->points[0].y > seg2->points[0].y)
		return 1;
	else if(seg1->points[0].y < seg2->points[0].y)
		return -1;
	else if(seg1->points[0].x > seg2->points[0].x)
		return 1;
	else if(seg1->points[0].x < seg2->points[0].x)
		return -1;
	else if((seg1->points[1].x - seg1->points[0].x) *
		(seg2->points[1].y - seg2->points[0].y) -
		(seg1->points[1].y - seg1->points[0].y) *
		(seg2->points[1].x - seg2->points[0].x) > 0)
		return 1;
	
	return -1;
}

/**
 * art_svp_from_vpath: Convert a vpath to a sorted vector path.
 * @vpath: #ArtVPath to convert.
 *
 * Converts a vector path into sorted vector path form. The svp form is
 * more efficient for rendering and other vector operations.
 *
 * Basically, the implementation is to traverse the vector path,
 * generating a new segment for each "run" of points in the vector
 * path with monotonically increasing Y values. All the resulting
 * values are then sorted.
 *
 * Note: I'm not sure that the sorting rule is correct with respect
 * to numerical stability issues.
 *
 * Return value: Resulting sorted vector path.
 **/
static psd_art_svp * psd_art_svp_from_vpath(psd_art_vpath *vpath)
{
	psd_int n_segs, n_segs_max;
	psd_art_svp *svp;
	psd_int dir;
	psd_int new_dir;
	psd_int i;
	psd_art_point *points;
	psd_int n_points, n_points_max;
	psd_double x, y;
	psd_double x_min, x_max;

	n_segs = 0;
	n_segs_max = 16;
	svp = (psd_art_svp *)psd_malloc(sizeof(psd_art_svp) +
		(n_segs_max - 1) * sizeof(psd_art_svp_seg));

	dir = 0;
	n_points = 0;
	n_points_max = 0;
	points = NULL;
	i = 0;

	x = y = 0; /* unnecessary, given "first code must not be LINETO" invariant,
	but it makes gcc -Wall -ansi -pedantic happier */
	x_min = x_max = 0; /* same */

	while(vpath[i].code != PSD_ART_END)
	{
		if(vpath[i].code == PSD_ART_MOVETO || vpath[i].code == PSD_ART_MOVETO_OPEN)
		{
			if(points != NULL && n_points >= 2)
			{
				if(n_segs == n_segs_max)
				{
					n_segs_max <<= 1;
					
					svp = (psd_art_svp *)psd_realloc(svp, sizeof(psd_art_svp) +
						(n_segs_max - 1) * sizeof(psd_art_svp_seg));
				}
				svp->segs[n_segs].n_points = n_points;
				svp->segs[n_segs].dir = (dir > 0);
				if(dir < 0)
					psd_reverse_points(points, n_points);
				svp->segs[n_segs].points = points;
				svp->segs[n_segs].bbox.x0 = x_min;
				svp->segs[n_segs].bbox.x1 = x_max;
				svp->segs[n_segs].bbox.y0 = points[0].y;
				svp->segs[n_segs].bbox.y1 = points[n_points - 1].y;
				n_segs++;
				points = NULL;
			}

			if(points == NULL)
			{
				n_points_max = 4;
				points = (psd_art_point *)psd_malloc(sizeof(psd_art_point) * n_points_max);
			}

			n_points = 1;
			points[0].x = x = vpath[i].x;
			points[0].y = y = vpath[i].y;
			x_min = x;
			x_max = x;
			dir = 0;
		}
		else /* must be LINETO */
		{
			new_dir = (vpath[i].y > y ||
				(vpath[i].y == y && vpath[i].x > x)) ? 1 : -1;
			if(dir && dir != new_dir)
			{
				/* new segment */
				x = points[n_points - 1].x;
				y = points[n_points - 1].y;
				if(n_segs == n_segs_max)
				{
					n_segs_max <<= 1;
					svp = (psd_art_svp *)psd_realloc(svp, sizeof(psd_art_svp) +
						(n_segs_max - 1) * sizeof(psd_art_svp_seg));
				}
				svp->segs[n_segs].n_points = n_points;
				svp->segs[n_segs].dir = (dir > 0);
				if(dir < 0)
					psd_reverse_points(points, n_points);
				svp->segs[n_segs].points = points;
				svp->segs[n_segs].bbox.x0 = x_min;
				svp->segs[n_segs].bbox.x1 = x_max;
				svp->segs[n_segs].bbox.y0 = points[0].y;
				svp->segs[n_segs].bbox.y1 = points[n_points - 1].y;
				n_segs++;

				n_points = 1;
				n_points_max = 4;
				points = (psd_art_point *)psd_malloc(sizeof(psd_art_point) * n_points_max);
				points[0].x = x;
				points[0].y = y;
				x_min = x;
				x_max = x;
			}

			if(points != NULL)
			{
				if(n_points == n_points_max)
				{
					if(n_points > 0)
					{
						n_points_max *= 2;
						points = (psd_art_point *)psd_realloc(points, sizeof(psd_art_point) * n_points_max);
					}
					else
					{
						n_points_max = 1;
						points = (psd_art_point *)psd_malloc(sizeof(psd_art_point));
					}
				}
				points[n_points].x = x = vpath[i].x;
				points[n_points].y = y = vpath[i].y;
				if(x < x_min)
					x_min = x;
				else if(x > x_max)
					x_max = x;
				n_points++;
			}
			dir = new_dir;
		}
		i++;
	}

	if(points != NULL)
	{
		if(n_points >= 2)
		{
			if(n_segs == n_segs_max)
			{
				n_segs_max *= 2;
				svp = (psd_art_svp *)psd_realloc(svp, sizeof(psd_art_svp) +
					(n_segs_max - 1) * sizeof(psd_art_svp_seg));
			}
			svp->segs[n_segs].n_points = n_points;
			svp->segs[n_segs].dir = (dir > 0);
			if(dir < 0)
				psd_reverse_points(points, n_points);
			svp->segs[n_segs].points = points;
			svp->segs[n_segs].bbox.x0 = x_min;
			svp->segs[n_segs].bbox.x1 = x_max;
			svp->segs[n_segs].bbox.y0 = points[0].y;
			svp->segs[n_segs].bbox.y1 = points[n_points - 1].y;
			n_segs++;
		}
		else
		{
			psd_free(points);
		}
	}

	svp->n_segs = n_segs;

	qsort(&svp->segs, n_segs, sizeof(psd_art_svp_seg), psd_art_svp_seg_compare);

	return svp;
}

static psd_int psd_art_svp_writer_rewind_add_segment(psd_art_svp_writer *self, psd_int wind_left,
	psd_int delta_wind, psd_double x, psd_double y)
{
	psd_art_svp_writer_rewind *swr = (psd_art_svp_writer_rewind *)self;
	psd_art_svp *svp;
	psd_art_svp_seg *seg;
	psd_bool left_filled, right_filled;
	psd_int wind_right = wind_left + delta_wind;
	psd_int seg_num;
	const psd_int init_n_points_max = 4;

	switch(swr->rule)
	{
		case PSD_ART_WIND_RULE_NONZERO:
			left_filled = (wind_left != 0);
			right_filled = (wind_right != 0);
			break;
		case PSD_ART_WIND_RULE_INTERSECT:
			left_filled = (wind_left > 1);
			right_filled = (wind_right > 1);
			break;
		case PSD_ART_WIND_RULE_ODDEVEN:
			left_filled = (wind_left & 1);
			right_filled = (wind_right & 1);
			break;
		case PSD_ART_WIND_RULE_POSITIVE:
			left_filled = (wind_left > 0);
			right_filled = (wind_right > 0);
			break;
		default:
			//art_die ("Unknown wind rule %d\n", swr->rule);
			psd_assert(0);
		break;
	}
	if(left_filled == right_filled)
	{
		/* discard segment now */
		return -1;
	}

	svp = swr->svp;
	seg_num = svp->n_segs++;
	if(swr->n_segs_max == seg_num)
	{
		swr->n_segs_max <<= 1;
		svp = (psd_art_svp *)psd_realloc(svp, sizeof(psd_art_svp) +
			(swr->n_segs_max - 1) * sizeof(psd_art_svp_seg));
		swr->svp = svp;
		swr->n_points_max = (psd_int *)psd_realloc(swr->n_points_max, swr->n_segs_max * 4);
	}
	seg = &svp->segs[seg_num];
	seg->n_points = 1;
	seg->dir = right_filled;
	swr->n_points_max[seg_num] = init_n_points_max;
	seg->bbox.x0 = x;
	seg->bbox.y0 = y;
	seg->bbox.x1 = x;
	seg->bbox.y1 = y;
	seg->points = (psd_art_point *)psd_malloc(sizeof(psd_art_point) * init_n_points_max);
	seg->points[0].x = x;
	seg->points[0].y = y;

	return seg_num;
}

static void psd_art_svp_writer_rewind_add_point(psd_art_svp_writer *self, psd_int seg_id,
	psd_double x, psd_double y)
{
	psd_art_svp_writer_rewind *swr = (psd_art_svp_writer_rewind *)self;
	psd_art_svp_seg *seg;
	psd_int n_points;

	if(seg_id < 0)
		/* omitted segment */
		return;

	seg = &swr->svp->segs[seg_id];
	n_points = seg->n_points++;
	if(swr->n_points_max[seg_id] == n_points)
	{
		if(n_points > 0)
		{
			swr->n_points_max[seg_id] *= 2;
			seg->points = (psd_art_point *)psd_realloc(seg->points, 
				sizeof(psd_art_point) * swr->n_points_max[seg_id]);
		}
		else
		{
			swr->n_points_max[seg_id] = 1;
			seg->points = (psd_art_point *)psd_malloc(sizeof(psd_art_point));
		}
	}
	seg->points[n_points].x = x;
	seg->points[n_points].y = y;
	if(x < seg->bbox.x0)
		seg->bbox.x0 = x;
	if(x > seg->bbox.x1)
		seg->bbox.x1 = x;
	seg->bbox.y1 = y;
}

static void psd_art_svp_writer_rewind_close_segment(psd_art_svp_writer *self, psd_int seg_id)
{
	//do nothing
}

static psd_art_svp_writer * psd_art_svp_writer_rewind_new(psd_art_wind_rule rule)
{
	psd_art_svp_writer_rewind *result = (psd_art_svp_writer_rewind *)psd_malloc(sizeof(psd_art_svp_writer_rewind));

	result->super.add_segment = psd_art_svp_writer_rewind_add_segment;
	result->super.add_point = psd_art_svp_writer_rewind_add_point;
	result->super.close_segment = psd_art_svp_writer_rewind_close_segment;

	result->rule = rule;
	result->n_segs_max = 16;
	result->svp = (psd_art_svp *)psd_malloc(sizeof(psd_art_svp) + 
		(result->n_segs_max - 1) * sizeof(psd_art_svp_seg));
	result->svp->n_segs = 0;
	result->n_points_max = (psd_int *)psd_malloc(result->n_segs_max * 4);

	return &result->super;
}

static psd_art_pri_q * psd_art_pri_new(void)
{
	psd_art_pri_q *result = (psd_art_pri_q *)psd_malloc(sizeof(psd_art_pri_q));

	result->n_items = 0;
	result->n_items_max = 16;
	result->items = (psd_art_pri_point **)psd_malloc(sizeof(psd_art_pri_point *) * result->n_items_max);
	return result;
}

static void psd_art_pri_insert(psd_art_pri_q *pq, psd_art_pri_point *point)
{
	if(pq->n_items == pq->n_items_max)
	{
		if(pq->n_items > 0)
		{
			pq->n_items_max *= 2;
			pq->items = (psd_art_pri_point **)psd_realloc(pq->items, sizeof(psd_art_pri_point *) * pq->n_items_max);
		}
		else
		{
			pq->n_items_max = 1;
			pq->items = (psd_art_pri_point **)psd_malloc(sizeof(psd_art_pri_point *));
		}
	}

	pq->items[pq->n_items++] = point;
}

static psd_bool psd_art_pri_empty(psd_art_pri_q *pq)
{
	return pq->n_items == 0;
}

/* Choose least point in queue */
static psd_art_pri_point * psd_art_pri_choose(psd_art_pri_q *pq)
{
	psd_int i;
	psd_int best = 0;
	psd_double best_x, best_y;
	psd_double y;
	psd_art_pri_point *result;

	if(pq->n_items == 0)
		return NULL;

	best_x = pq->items[best]->x;
	best_y = pq->items[best]->y;

	for(i = 1; i < pq->n_items; i++)
	{
		y = pq->items[i]->y;
		if(y < best_y || (y == best_y && pq->items[i]->x < best_x))
		{
			best = i;
			best_x = pq->items[best]->x;
			best_y = y;
		}
	}
	result = pq->items[best];
	pq->items[best] = pq->items[--pq->n_items];
	return result;
}

/**
 * art_svp_intersect_active_free: Free an active segment.
 * @seg: Segment to delete.
 *
 * Frees @seg.
 **/
static /* todo inline */ void psd_art_svp_intersect_active_free(psd_art_active_seg *seg)
{
	psd_free(seg->stack);
	psd_free(seg);
}

/**
 * art_svp_intersect_horiz_commit: Commit points in horiz list to output.
 * @ctx: Intersection context.
 *
 * The main function of the horizontal commit is to output new
 * points to the output writer.
 *
 * This "commit" pass is also where winding numbers are assigned,
 * because doing it here provides much greater tolerance for inputs
 * which are not in strict SVP order.
 *
 * Each cluster in the horiz_list contains both segments that are in
 * the active list (ART_ACTIVE_FLAGS_DEL is psd_false) and that are not,
 * and are scheduled to be deleted (ART_ACTIVE_FLAGS_DEL is psd_true). We
 * need to deal with both.
 **/
static void psd_art_svp_intersect_horiz_commit(psd_art_intersect_ctx *ctx)
{
	psd_art_active_seg *seg;
	psd_int winding_number = 0; /* initialization just to avoid warning */
	psd_int horiz_wind = 0;
	psd_double last_x = 0; /* initialization just to avoid warning */

	/* Output points to svp writer. */
	for(seg = ctx->horiz_first; seg != NULL;)
	{
		/* Find a cluster with common horiz_x, */
		psd_art_active_seg *curs;
		psd_double x = seg->horiz_x;

		/* Generate any horizontal segments. */
		if(horiz_wind != 0)
		{
			psd_art_svp_writer *swr = ctx->out;
			psd_int seg_id;

			seg_id = swr->add_segment(swr, winding_number, horiz_wind,
				last_x, ctx->y);
			swr->add_point(swr, seg_id, x, ctx->y);
			swr->close_segment(swr, seg_id);
		}

		/* Find first active segment in cluster. */

		for(curs = seg; curs != NULL && curs->horiz_x == x;
			curs = curs->horiz_right)
			if(!(curs->flags & PSD_ART_ACTIVE_FLAGS_DEL))
				break;

		if(curs != NULL && curs->horiz_x == x)
		{
			/* There exists at least one active segment in this cluster. */

			/* Find beginning of cluster. */
			for(; curs->left != NULL; curs = curs->left)
				if(curs->left->horiz_x != x)
					break;

			if(curs->left != NULL)
				winding_number = curs->left->wind_left + curs->left->delta_wind;
			else
				winding_number = 0;

			do
			{
				if(!(curs->flags & PSD_ART_ACTIVE_FLAGS_OUT) ||
					curs->wind_left != winding_number)
				{
					psd_art_svp_writer *swr = ctx->out;

					if(curs->flags & PSD_ART_ACTIVE_FLAGS_OUT)
					{
						swr->add_point(swr, curs->seg_id,
							curs->horiz_x, ctx->y);
						swr->close_segment(swr, curs->seg_id);
					}

					curs->seg_id = swr->add_segment(swr, winding_number,
						curs->delta_wind,
						x, ctx->y);
					curs->flags |= PSD_ART_ACTIVE_FLAGS_OUT;
				}
				curs->wind_left = winding_number;
				winding_number += curs->delta_wind;
				curs = curs->right;
			}
			while(curs != NULL && curs->horiz_x == x);
		}

		/* Skip past cluster. */
		do
		{
			psd_art_active_seg *next = seg->horiz_right;

			seg->flags &= ~PSD_ART_ACTIVE_FLAGS_IN_HORIZ;
			horiz_wind += seg->horiz_delta_wind;
			seg->horiz_delta_wind = 0;
			if(seg->flags & PSD_ART_ACTIVE_FLAGS_DEL)
			{
				if(seg->flags & PSD_ART_ACTIVE_FLAGS_OUT)
				{
					psd_art_svp_writer *swr = ctx->out;
					swr->close_segment(swr, seg->seg_id);
				}
				psd_art_svp_intersect_active_free(seg);
			}
			seg = next;
		}
		while(seg != NULL && seg->horiz_x == x);

		last_x = x;
	}
	ctx->horiz_first = NULL;
	ctx->horiz_last = NULL;
}

/**
 * art_svp_intersect_setup_seg: Set up an active segment from input segment.
 * @seg: Active segment.
 * @pri_pt: Priority queue point to initialize.
 *
 * Sets the x[], a, b, c, flags, and stack fields according to the
 * line from the current cursor value. Sets the priority queue point
 * to the bottom point of this line. Also advances the input segment
 * cursor.
 **/
static void psd_art_svp_intersect_setup_seg(psd_art_active_seg *seg, psd_art_pri_point *pri_pt)
{
	const psd_art_svp_seg *in_seg = seg->in_seg;
	psd_int in_curs = seg->in_curs++;
	psd_double x0, y0, x1, y1;
	psd_double dx, dy, s;
	psd_double a, b, r2;

	x0 = in_seg->points[in_curs].x;
	y0 = in_seg->points[in_curs].y;
	x1 = in_seg->points[in_curs + 1].x;
	y1 = in_seg->points[in_curs + 1].y;
	pri_pt->x = x1;
	pri_pt->y = y1;
	dx = x1 - x0;
	dy = y1 - y0;
	r2 = dx * dx + dy * dy;
	s = r2 == 0 ? 1 : 1 / (psd_double)psd_carm_sqrt((psd_float)r2);
	seg->a = a = dy * s;
	seg->b = b = -dx * s;
	seg->c = -(a * x0 + b * y0);
	seg->flags = (seg->flags & ~PSD_ART_ACTIVE_FLAGS_BNEG) | (dx > 0);
	seg->x[0] = x0;
	seg->x[1] = x1;
	seg->y0 = y0;
	seg->y1 = y1;
	seg->n_stack = 1;
	seg->stack[0].x = x1;
	seg->stack[0].y = y1;
}

static void psd_art_svp_intersect_push_pt(psd_art_intersect_ctx *ctx, psd_art_active_seg *seg,
	psd_double x, psd_double y)
{
	psd_art_pri_point *pri_pt;
	psd_int n_stack = seg->n_stack;

	if(n_stack == seg->n_stack_max)
	{
		if(n_stack > 0)
		{
			seg->n_stack_max *= 2;
			seg->stack = (psd_art_point *)psd_realloc(seg->stack, sizeof(psd_art_point) * seg->n_stack_max);
		}
		else
		{
			seg->n_stack_max = 1;
			seg->stack = (psd_art_point *)psd_malloc(sizeof(psd_art_point));
		}
	}
	seg->stack[n_stack].x = x;
	seg->stack[n_stack].y = y;
	seg->n_stack++;

	seg->x[1] = x;
	seg->y1 = y;

	pri_pt = (psd_art_pri_point *)psd_malloc(sizeof(psd_art_pri_point));
	pri_pt->x = x;
	pri_pt->y = y;
	pri_pt->user_data = seg;
	psd_art_pri_insert(ctx->pq, pri_pt);
}

/**
 * art_svp_intersect_add_horiz: Add point to horizontal list.
 * @ctx: Intersector context.
 * @seg: Segment with point to insert into horizontal list.
 *
 * Inserts @seg into horizontal list, keeping it in ascending horiz_x
 * order.
 *
 * Note: the horiz_commit routine processes "clusters" of segs in the
 * horiz list, all sharing the same horiz_x value. The cluster is
 * processed in active list order, rather than horiz list order. Thus,
 * the order of segs in the horiz list sharing the same horiz_x
 * _should_ be irrelevant. Even so, we use b as a secondary sorting key,
 * as a "belt and suspenders" defensive coding tactic.
 **/
static void psd_art_svp_intersect_add_horiz(psd_art_intersect_ctx *ctx, psd_art_active_seg *seg)
{
	psd_art_active_seg **pp = &ctx->horiz_last;
	psd_art_active_seg *place;
	psd_art_active_seg *place_right = NULL;

	if(seg->flags & PSD_ART_ACTIVE_FLAGS_IN_HORIZ)
		return;
	seg->flags |= PSD_ART_ACTIVE_FLAGS_IN_HORIZ;

	for(place = *pp; place != NULL && (place->horiz_x > seg->horiz_x ||
		(place->horiz_x == seg->horiz_x &&
		place->b < seg->b));
		place = *pp)
	{
		place_right = place;
		pp = &place->horiz_left;
	}
	*pp = seg;
	seg->horiz_left = place;
	seg->horiz_right = place_right;
	if (place == NULL)
		ctx->horiz_first = seg;
	else
		place->horiz_right = seg;
}

/**
 * art_svp_intersect_break: Break an active segment.
 *
 * Note: y must be greater than the top point's y, and less than
 * the bottom's.
 *
 * Return value: x coordinate of break point.
 */
static psd_double psd_art_svp_intersect_break(psd_art_intersect_ctx *ctx, psd_art_active_seg *seg,
	psd_double x_ref, psd_double y, psd_art_break_flags break_flags)
{
	psd_double x0, y0, x1, y1;
	const psd_art_svp_seg *in_seg = seg->in_seg;
	psd_int in_curs = seg->in_curs;
	psd_double x;

	x0 = in_seg->points[in_curs - 1].x;
	y0 = in_seg->points[in_curs - 1].y;
	x1 = in_seg->points[in_curs].x;
	y1 = in_seg->points[in_curs].y;
	x = x0 + (x1 - x0) * ((y - y0) / (y1 - y0));

	/* I think we can count on min(x0, x1) <= x <= max(x0, x1) with sane
	arithmetic, but it might be worthwhile to check just in case. */

	if(y > ctx->y)
		psd_art_svp_intersect_push_pt(ctx, seg, x, y);
	else
	{
		seg->x[0] = x;
		seg->y0 = y;
		seg->horiz_x = x;
		psd_art_svp_intersect_add_horiz(ctx, seg);
	}

	return x;
}

/**
 * art_svp_intersect_add_point: Add a point, breaking nearby neighbors.
 * @ctx: Intersector context.
 * @x: X coordinate of point to add.
 * @y: Y coordinate of point to add.
 * @seg: "nearby" segment, or NULL if leftmost.
 *
 * Return value: Segment immediately to the left of the new point, or
 * NULL if the new point is leftmost.
 **/
static psd_art_active_seg * psd_art_svp_intersect_add_point(psd_art_intersect_ctx *ctx, psd_double x, psd_double y,
	psd_art_active_seg *seg, psd_art_break_flags break_flags)
{
	psd_art_active_seg *left, *right;
	psd_double x_min = x, x_max = x;
	psd_bool left_live, right_live;
	psd_double d;
	psd_double new_x;
	psd_art_active_seg *test, *result = NULL;
	psd_double x_test;

	left = seg;
	if(left == NULL)
		right = ctx->active_head;
	else
		right = left->right; 
	left_live = (break_flags & PSD_ART_BREAK_LEFT) && (left != NULL);
	right_live = (break_flags & PSD_ART_BREAK_RIGHT) && (right != NULL);
	while(left_live || right_live)
	{
		if(left_live)
		{
			if(x <= left->x[left->flags & PSD_ART_ACTIVE_FLAGS_BNEG] &&
				/* It may be that one of these conjuncts turns out to be always
				psd_true. We test both anyway, to be defensive. */
				y != left->y0 && y < left->y1)
			{
				d = x_min * left->a + y * left->b + left->c;
				if(d < PSD_EPSILON_A)
				{
					new_x = psd_art_svp_intersect_break(ctx, left, x_min, y,
						PSD_ART_BREAK_LEFT);
					if(new_x > x_max)
					{
						x_max = new_x;
						right_live = (right != NULL);
					}
					else if(new_x < x_min)
						x_min = new_x;
					left = left->left;
					left_live = (left != NULL);
				}
				else
				left_live = psd_false;
			}
			else
				left_live = psd_false;
		}
		else if(right_live)
		{
			if(x >= right->x[(right->flags & PSD_ART_ACTIVE_FLAGS_BNEG) ^ 1] &&
				/* It may be that one of these conjuncts turns out to be always
				psd_true. We test both anyway, to be defensive. */
				y != right->y0 && y < right->y1)
			{
				d = x_max * right->a + y * right->b + right->c;
				if(d > -PSD_EPSILON_A)
				{
					new_x = psd_art_svp_intersect_break(ctx, right, x_max, y,
						PSD_ART_BREAK_RIGHT);
					if(new_x < x_min)
					{
						x_min = new_x;
						left_live = (left != NULL);
					}
					else if(new_x >= x_max)
						x_max = new_x;
					right = right->right;
					right_live = (right != NULL);
				}
				else
					right_live = psd_false;
			}
			else
				right_live = psd_false;
		}
	}

	/* Ascending order is guaranteed by break_flags. Thus, we don't need
	to actually fix up non-ascending pairs. */

	/* Now, (left, right) defines an interval of segments broken. Sort
	into ascending x order. */
	test = left == NULL ? ctx->active_head : left->right;
	result = left;
	if(test != NULL && test != right)
	{
		if(y == test->y0)
			x_test = test->x[0];
		else /* assert y == test->y1, I think */
			x_test = test->x[1];
		for(;;)
		{
			if(x_test <= x)
				result = test;
			test = test->right;
			if(test == right)
				break;
			new_x = x_test;
			x_test = new_x;
		}
	}
	return result;
}

static void psd_art_svp_intersect_swap_active(psd_art_intersect_ctx *ctx,
	psd_art_active_seg *left_seg, psd_art_active_seg *right_seg)
{
	right_seg->left = left_seg->left;
	if(right_seg->left != NULL)
		right_seg->left->right = right_seg;
	else
		ctx->active_head = right_seg;
	left_seg->right = right_seg->right;
	if(left_seg->right != NULL)
		left_seg->right->left = left_seg;
	left_seg->left = right_seg;
	right_seg->right = left_seg;
}

/**
 * art_svp_intersect_test_cross: Test crossing of a pair of active segments.
 * @ctx: Intersector context.
 * @left_seg: Left segment of the pair.
 * @right_seg: Right segment of the pair.
 * @break_flags: Flags indicating whether to break neighbors.
 *
 * Tests crossing of @left_seg and @right_seg. If there is a crossing,
 * inserts the intersection point into both segments.
 *
 * Return value: True if the intersection took place at the current
 * scan line, indicating further iteration is needed.
 **/
static psd_bool psd_art_svp_intersect_test_cross(psd_art_intersect_ctx *ctx,
	psd_art_active_seg *left_seg, psd_art_active_seg *right_seg,
	psd_art_break_flags break_flags)
{
	psd_double left_x0, left_y0, left_x1;
	psd_double left_y1 = left_seg->y1;
	psd_double right_y1 = right_seg->y1;
	psd_double d;

	const psd_art_svp_seg *in_seg;
	psd_int in_curs;
	psd_double d0, d1, t;
	psd_double x, y; /* intersection point */

	if(left_seg->y0 == right_seg->y0 && left_seg->x[0] == right_seg->x[0])
	{
		/* Top points of left and right segments coincide. This case
		feels like a bit of duplication - we may want to merge it
		with the cases below. However, this way, we're sure that this
		logic makes only localized changes. */

		if(left_y1 < right_y1)
		{
			/* Test left (x1, y1) against right segment */
			psd_double left_x1 = left_seg->x[1];

			if(left_x1 <
				right_seg->x[(right_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG) ^ 1] ||
				left_y1 == right_seg->y0)
				return psd_false;
			d = left_x1 * right_seg->a + left_y1 * right_seg->b + right_seg->c;
			if(d < -PSD_EPSILON_A)
				return psd_false;
			else if(d < PSD_EPSILON_A)
			{
				/* I'm unsure about the break flags here. */
				psd_double right_x1 = psd_art_svp_intersect_break(ctx, right_seg,
					left_x1, left_y1,
					PSD_ART_BREAK_RIGHT);
				if(left_x1 <= right_x1)
					return psd_false;
			}
		}
		else if(left_y1 > right_y1)
		{
			/* Test right (x1, y1) against left segment */
			psd_double right_x1 = right_seg->x[1];

			if(right_x1 > left_seg->x[left_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG] ||
				right_y1 == left_seg->y0)
				return psd_false;
			d = right_x1 * left_seg->a + right_y1 * left_seg->b + left_seg->c;
			if(d > PSD_EPSILON_A)
				return psd_false;
			else if(d > -PSD_EPSILON_A)
			{
				/* See above regarding break flags. */
				psd_double left_x1 = psd_art_svp_intersect_break(ctx, left_seg,
					right_x1, right_y1,
					PSD_ART_BREAK_LEFT);
				if(left_x1 <= right_x1)
					return psd_false;
			}
		}
		else /* left_y1 == right_y1 */
		{
			psd_double left_x1 = left_seg->x[1];
			psd_double right_x1 = right_seg->x[1];

			if(left_x1 <= right_x1)
				return psd_false;
		}
		psd_art_svp_intersect_swap_active(ctx, left_seg, right_seg);
		return psd_true;
	}

	if(left_y1 < right_y1)
	{
		/* Test left (x1, y1) against right segment */
		psd_double left_x1 = left_seg->x[1];

		if(left_x1 <
			right_seg->x[(right_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG) ^ 1] ||
			left_y1 == right_seg->y0)
			return psd_false;
		d = left_x1 * right_seg->a + left_y1 * right_seg->b + right_seg->c;
		if(d < -PSD_EPSILON_A)
			return psd_false;
		else if(d < PSD_EPSILON_A)
		{
			psd_double right_x1 = psd_art_svp_intersect_break(ctx, right_seg,
				left_x1, left_y1,
				PSD_ART_BREAK_RIGHT);
			if(left_x1 <= right_x1)
				return psd_false;
		}
	}
	else if(left_y1 > right_y1)
	{
		/* Test right (x1, y1) against left segment */
		psd_double right_x1 = right_seg->x[1];

		if(right_x1 > left_seg->x[left_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG] ||
			right_y1 == left_seg->y0)
			return psd_false;
		d = right_x1 * left_seg->a + right_y1 * left_seg->b + left_seg->c;
		if(d > PSD_EPSILON_A)
			return psd_false;
		else if(d > -PSD_EPSILON_A)
		{
			psd_double left_x1 = psd_art_svp_intersect_break(ctx, left_seg,
				right_x1, right_y1,
				PSD_ART_BREAK_LEFT);
			if(left_x1 <= right_x1)
				return psd_false;
		}
	}
	else /* left_y1 == right_y1 */
	{ 
		psd_double left_x1 = left_seg->x[1];
		psd_double right_x1 = right_seg->x[1];

		if(left_x1 <= right_x1)
			return psd_false;
	}

	/* The segments cross. Find the intersection point. */

	in_seg = left_seg->in_seg;
	in_curs = left_seg->in_curs;
	left_x0 = in_seg->points[in_curs - 1].x;
	left_y0 = in_seg->points[in_curs - 1].y;
	left_x1 = in_seg->points[in_curs].x;
	left_y1 = in_seg->points[in_curs].y;
	d0 = left_x0 * right_seg->a + left_y0 * right_seg->b + right_seg->c;
	d1 = left_x1 * right_seg->a + left_y1 * right_seg->b + right_seg->c;
	if(d0 == d1)
	{
		x = left_x0;
		y = left_y0;
	}
	else
	{
		/* Is this division always safe? It could possibly overflow. */
		t = d0 / (d0 - d1);
		if(t <= 0)
		{
			x = left_x0;
			y = left_y0;
		}
		else if(t >= 1)
		{
			x = left_x1;
			y = left_y1;
		}
		else
		{
			x = left_x0 + t * (left_x1 - left_x0);
			y = left_y0 + t * (left_y1 - left_y0);
		}
	}

	/* Make sure intersection point is within bounds of right seg. */
	if(y < right_seg->y0)
	{
		x = right_seg->x[0];
		y = right_seg->y0;
	}
	else if(y > right_seg->y1)
	{
		x = right_seg->x[1];
		y = right_seg->y1;
	}
	else if(x < right_seg->x[(right_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG) ^ 1])
		x = right_seg->x[(right_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG) ^ 1];
	else if(x > right_seg->x[right_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG])
		x = right_seg->x[right_seg->flags & PSD_ART_ACTIVE_FLAGS_BNEG];

	if(y == left_seg->y0)
	{
		if(y != right_seg->y0)
		{
			psd_art_svp_intersect_push_pt(ctx, right_seg, x, y);
			if((break_flags & PSD_ART_BREAK_RIGHT) && right_seg->right != NULL)
				psd_art_svp_intersect_add_point(ctx, x, y, right_seg->right,
					break_flags);
		}
		else
		{
		/* Intersection takes place at current scan line; process
		immediately rather than queueing intersection point into
		priq. */
		psd_art_active_seg *winner, *loser;

		/* Choose "most vertical" segement */
		if(left_seg->a > right_seg->a)
		{
			winner = left_seg;
			loser = right_seg;
		}
		else
		{
			winner = right_seg;
			loser = left_seg;
		}

		loser->x[0] = winner->x[0];
		loser->horiz_x = loser->x[0];
		loser->horiz_delta_wind += loser->delta_wind;
		winner->horiz_delta_wind -= loser->delta_wind;

		psd_art_svp_intersect_swap_active(ctx, left_seg, right_seg);
		return psd_true;
		}
	}
	else if(y == right_seg->y0)
	{
		psd_art_svp_intersect_push_pt(ctx, left_seg, x, y);
		if((break_flags & PSD_ART_BREAK_LEFT) && left_seg->left != NULL)
			psd_art_svp_intersect_add_point(ctx, x, y, left_seg->left,
				break_flags);
	}
	else
	{
		/* Insert the intersection point into both segments. */
		psd_art_svp_intersect_push_pt(ctx, left_seg, x, y);
		psd_art_svp_intersect_push_pt(ctx, right_seg, x, y);
		if((break_flags & PSD_ART_BREAK_LEFT) && left_seg->left != NULL)
			psd_art_svp_intersect_add_point(ctx, x, y, left_seg->left, break_flags);
		if((break_flags & PSD_ART_BREAK_RIGHT) && right_seg->right != NULL)
			psd_art_svp_intersect_add_point(ctx, x, y, right_seg->right, break_flags);
	}
	return psd_false;
}

/**
 * art_svp_intersect_horiz: Add horizontal line segment.
 * @ctx: Intersector context.
 * @seg: Segment on which to add horizontal line.
 * @x0: Old x position.
 * @x1: New x position.
 *
 * Adds a horizontal line from @x0 to @x1, and updates the current
 * location of @seg to @x1.
 **/
static void psd_art_svp_intersect_horiz(psd_art_intersect_ctx *ctx, psd_art_active_seg *seg,
	psd_double x0, psd_double x1)
{
	psd_art_active_seg *hs;

	if(x0 == x1)
		return;

	hs = (psd_art_active_seg *)psd_malloc(sizeof(psd_art_active_seg));

	hs->flags = PSD_ART_ACTIVE_FLAGS_DEL | (seg->flags & PSD_ART_ACTIVE_FLAGS_OUT);
	if(seg->flags & PSD_ART_ACTIVE_FLAGS_OUT)
	{
		psd_art_svp_writer *swr = ctx->out;

		swr->add_point(swr, seg->seg_id, x0, ctx->y);
	}
	hs->seg_id = seg->seg_id;
	hs->horiz_x = x0;
	hs->horiz_delta_wind = seg->delta_wind;
	hs->stack = NULL;

	/* Ideally, the (a, b, c) values will never be read. However, there
	are probably some tests remaining that don't check for _DEL
	before evaluating the line equation. For those, these
	initializations will at least prevent a UMR of the values, which
	can crash on some platforms. */
	hs->a = 0.0;
	hs->b = 0.0;
	hs->c = 0.0;

	seg->horiz_delta_wind -= seg->delta_wind;

	psd_art_svp_intersect_add_horiz(ctx, hs);

	if(x0 > x1)
	{
		psd_art_active_seg *left;
		psd_bool first = psd_true;

		for(left = seg->left; left != NULL; left = seg->left)
		{
			psd_int left_bneg = left->flags & PSD_ART_ACTIVE_FLAGS_BNEG;

			if(left->x[left_bneg] <= x1)
				break;
			if(left->x[left_bneg ^ 1] <= x1 &&
				x1 * left->a + ctx->y * left->b + left->c >= 0)
				break;
			if(left->y0 != ctx->y && left->y1 != ctx->y)
			{
				psd_art_svp_intersect_break(ctx, left, x1, ctx->y, 
					PSD_ART_BREAK_LEFT);
			}
			psd_art_svp_intersect_swap_active(ctx, left, seg);
			if(first && left->right != NULL)
			{
				psd_art_svp_intersect_test_cross(ctx, left, left->right,
					PSD_ART_BREAK_RIGHT);
				first = psd_false;
			}
		}
	}
	else
	{
		psd_art_active_seg *right;
		psd_bool first = psd_true;

		for(right = seg->right; right != NULL; right = seg->right)
		{
			psd_int right_bneg = right->flags & PSD_ART_ACTIVE_FLAGS_BNEG;

			if(right->x[right_bneg ^ 1] >= x1)
				break;
			if(right->x[right_bneg] >= x1 &&
				x1 * right->a + ctx->y * right->b + right->c <= 0)
				break;
			if(right->y0 != ctx->y && right->y1 != ctx->y)
			{
				psd_art_svp_intersect_break(ctx, right, x1, ctx->y,
					PSD_ART_BREAK_LEFT);
			}
			psd_art_svp_intersect_swap_active(ctx, seg, right);
			if(first && right->left != NULL)
			{
				psd_art_svp_intersect_test_cross(ctx, right->left, right,
					PSD_ART_BREAK_RIGHT);
				first = psd_false;
			}
		}
	}

	seg->x[0] = x1;
	seg->x[1] = x1;
	seg->horiz_x = x1;
	seg->flags &= ~PSD_ART_ACTIVE_FLAGS_OUT;
}

/**
 * art_svp_intersect_insert_cross: Test crossings of newly inserted line.
 *
 * Tests @seg against its left and right neighbors for intersections.
 * Precondition: the line in @seg is not purely horizontal.
 **/
static void psd_art_svp_intersect_insert_cross(psd_art_intersect_ctx *ctx,
	psd_art_active_seg *seg)
{
	psd_art_active_seg *left = seg, *right = seg;

	for(;;)
	{
		if(left != NULL)
		{
			psd_art_active_seg *leftc;

			for(leftc = left->left; leftc != NULL; leftc = leftc->left)
				if(!(leftc->flags & PSD_ART_ACTIVE_FLAGS_DEL))
					break;
			if(leftc != NULL &&
				psd_art_svp_intersect_test_cross(ctx, leftc, left, 
				PSD_ART_BREAK_LEFT))
			{
				if(left == right || right == NULL)
					right = left->right;
			}
			else
			{
				left = NULL;
			}
		}
		else if(right != NULL && right->right != NULL)
		{
			psd_art_active_seg *rightc;

			for(rightc = right->right; rightc != NULL; rightc = rightc->right)
				if(!(rightc->flags & PSD_ART_ACTIVE_FLAGS_DEL))
					break;
			if(rightc != NULL &&
				psd_art_svp_intersect_test_cross(ctx, right, rightc,
				PSD_ART_BREAK_RIGHT))
			{
				if(left == right || left == NULL)
					left = right->left;
			}
			else
			{
				right = NULL;
			}
		}
		else
			break;
	}
}

/**
 * art_svp_intersect_insert_line: Insert a line into the active list.
 * @ctx: Intersector context.
 * @seg: Segment containing line to insert.
 *
 * Inserts the line into the intersector context, taking care of any
 * intersections, and adding the appropriate horizontal points to the
 * active list.
 **/
static void psd_art_svp_intersect_insert_line(psd_art_intersect_ctx *ctx, psd_art_active_seg *seg)
{
	if(seg->y1 == seg->y0)
	{
		psd_art_svp_intersect_horiz(ctx, seg, seg->x[0], seg->x[1]);
	}
	else
	{
		psd_art_svp_intersect_insert_cross(ctx, seg);
		psd_art_svp_intersect_add_horiz(ctx, seg);
	}
}

static void psd_art_svp_intersect_add_seg(psd_art_intersect_ctx *ctx, const psd_art_svp_seg *in_seg)
{
	psd_art_active_seg *seg = (psd_art_active_seg *)psd_malloc(sizeof(psd_art_active_seg));
	psd_art_active_seg *test;
	psd_double x0, y0;
	psd_art_active_seg *beg_range;
	psd_art_active_seg *last = NULL;
	psd_art_active_seg *left, *right;
	psd_art_pri_point *pri_pt = (psd_art_pri_point *)psd_malloc(sizeof(psd_art_pri_point));

	seg->flags = 0;
	seg->in_seg = in_seg;
	seg->in_curs = 0;

	seg->n_stack_max = 4;
	seg->stack = (psd_art_point *)psd_malloc(sizeof(psd_art_point) * seg->n_stack_max);

	seg->horiz_delta_wind = 0;

	seg->wind_left = 0;

	pri_pt->user_data = seg;
	psd_art_svp_intersect_setup_seg(seg, pri_pt);
	psd_art_pri_insert(ctx->pq, pri_pt);

	/* Find insertion place for new segment */
	/* This is currently a left-to-right scan, but should be replaced
	with a binary search as soon as it's validated. */

	x0 = in_seg->points[0].x;
	y0 = in_seg->points[0].y;
	beg_range = NULL;
	for(test = ctx->active_head; test != NULL; test = test->right)
	{
		psd_double d;
		psd_int test_bneg = test->flags & PSD_ART_ACTIVE_FLAGS_BNEG;

		if(x0 < test->x[test_bneg])
		{
			if(x0 < test->x[test_bneg ^ 1])
				break;
			d = x0 * test->a + y0 * test->b + test->c;
			if(d < 0)
				break;
		}
		last = test;
	}

	left = psd_art_svp_intersect_add_point(ctx, x0, y0, last, PSD_ART_BREAK_LEFT | PSD_ART_BREAK_RIGHT);
	seg->left = left;
	if(left == NULL)
	{
		right = ctx->active_head;
		ctx->active_head = seg;
	}
	else
	{
		right = left->right;
		left->right = seg;
	}
	seg->right = right;
	if(right != NULL)
		right->left = seg;

	seg->delta_wind = in_seg->dir ? 1 : -1;
	seg->horiz_x = x0;

	psd_art_svp_intersect_insert_line(ctx, seg);
}

static void psd_art_svp_intersect_process_intersection(psd_art_intersect_ctx *ctx,
	psd_art_active_seg *seg)
{
	psd_int n_stack = --seg->n_stack;
	seg->x[1] = seg->stack[n_stack - 1].x;
	seg->y1 = seg->stack[n_stack - 1].y;
	seg->x[0] = seg->stack[n_stack].x;
	seg->y0 = seg->stack[n_stack].y;
	seg->horiz_x = seg->x[0];
	psd_art_svp_intersect_insert_line(ctx, seg);
}

/**
 * art_svp_intersect_active_delete: Delete segment from active list.
 * @ctx: Intersection context.
 * @seg: Segment to delete.
 *
 * Deletes @seg from the active list.
 **/
static /* todo inline */ void psd_art_svp_intersect_active_delete(psd_art_intersect_ctx *ctx, psd_art_active_seg *seg)
{
	psd_art_active_seg *left = seg->left, *right = seg->right;

	if(left != NULL)
		left->right = right;
	else
		ctx->active_head = right;
	if(right != NULL)
		right->left = left;
}

static void psd_art_svp_intersect_advance_cursor(psd_art_intersect_ctx *ctx, psd_art_active_seg *seg,
	psd_art_pri_point *pri_pt)
{
	const psd_art_svp_seg *in_seg = seg->in_seg;
	psd_int in_curs = seg->in_curs;
	psd_art_svp_writer *swr = seg->flags & PSD_ART_ACTIVE_FLAGS_OUT ? ctx->out : NULL;

	if(swr != NULL)
		swr->add_point(swr, seg->seg_id, seg->x[1], seg->y1);
	if(in_curs + 1 == in_seg->n_points)
	{
		psd_art_active_seg *left = seg->left, *right = seg->right;

		seg->flags |= PSD_ART_ACTIVE_FLAGS_DEL;
		psd_art_svp_intersect_add_horiz(ctx, seg);
		psd_art_svp_intersect_active_delete(ctx, seg);
		if(left != NULL && right != NULL)
			psd_art_svp_intersect_test_cross(ctx, left, right,
				PSD_ART_BREAK_LEFT | PSD_ART_BREAK_RIGHT);
		psd_free(pri_pt);
	}
	else
	{
		seg->horiz_x = seg->x[1];

		psd_art_svp_intersect_setup_seg(seg, pri_pt);
		psd_art_pri_insert (ctx->pq, pri_pt);
		psd_art_svp_intersect_insert_line(ctx, seg);
	}
}

static void psd_art_pri_free(psd_art_pri_q *pq)
{
	psd_free(pq->items);
	psd_free(pq);
}

static void psd_art_svp_intersector(const psd_art_svp *in, psd_art_svp_writer *out)
{
	psd_art_intersect_ctx *ctx;
	psd_art_pri_q *pq;
	psd_art_pri_point *first_point;

	if(in->n_segs == 0)
		return;

	ctx = (psd_art_intersect_ctx *)psd_malloc(sizeof(psd_art_intersect_ctx));
	ctx->in = in;
	ctx->out = out;
	pq = psd_art_pri_new();
	ctx->pq = pq;

	ctx->active_head = NULL;

	ctx->horiz_first = NULL;
	ctx->horiz_last = NULL;

	ctx->in_curs = 0;
	first_point = (psd_art_pri_point *)psd_malloc(sizeof(psd_art_pri_point));
	first_point->x = in->segs[0].points[0].x;
	first_point->y = in->segs[0].points[0].y;
	first_point->user_data = NULL;
	ctx->y = first_point->y;
	psd_art_pri_insert(pq, first_point);

	while(!psd_art_pri_empty(pq))
	{
		psd_art_pri_point *pri_point = psd_art_pri_choose(pq);
		psd_art_active_seg *seg = (psd_art_active_seg *)pri_point->user_data;

		if(ctx->y != pri_point->y)
		{
			psd_art_svp_intersect_horiz_commit(ctx);
			ctx->y = pri_point->y;
		}

		if(seg == NULL)
		{
			/* Insert new segment from input */
			const psd_art_svp_seg *in_seg = &in->segs[ctx->in_curs++];
			psd_art_svp_intersect_add_seg(ctx, in_seg);
			if(ctx->in_curs < in->n_segs)
			{
				const psd_art_svp_seg *next_seg = &in->segs[ctx->in_curs];
				pri_point->x = next_seg->points[0].x;
				pri_point->y = next_seg->points[0].y;
				/* user_data is already NULL */
				psd_art_pri_insert(pq, pri_point);
			}
			else
			{
				psd_free(pri_point);
			}
		}
		else
		{
			psd_int n_stack = seg->n_stack;

			if(n_stack > 1)
			{
				psd_art_svp_intersect_process_intersection(ctx, seg);
				psd_free(pri_point);
			}
			else
			{
				psd_art_svp_intersect_advance_cursor(ctx, seg, pri_point);
			}
		}
	}

	psd_art_svp_intersect_horiz_commit(ctx);

	psd_art_pri_free(pq);
	psd_free(ctx);
}

static psd_art_svp * psd_art_svp_writer_rewind_reap(psd_art_svp_writer *self)
{
	psd_art_svp_writer_rewind *swr = (psd_art_svp_writer_rewind *)self;
	psd_art_svp *result = swr->svp;

	psd_free(swr->n_points_max);
	psd_free(swr);
	return result;
}

/* Render a vector path into a stroked outline.

   Status of this routine:

   Basic correctness: Only miter and bevel line joins are implemented,
   and only butt line caps. Otherwise, seems to be fine.

   Numerical stability: We cheat (adding random perturbation). Thus,
   it seems very likely that no numerical stability problems will be
   seen in practice.

   Speed: Should be pretty good.

   Precision: The perturbation fuzzes the coordinates slightly,
   but not enough to be visible.  */
/**
 * art_svp_vpath_stroke: Stroke a vector path.
 * @vpath: #ArtVPath to stroke.
 * @join: Join style.
 * @cap: Cap style.
 * @line_width: Width of stroke.
 * @miter_limit: Miter limit.
 * @flatness: Flatness.
 *
 * Computes an svp representing the stroked outline of @vpath. The
 * width of the stroked line is @line_width.
 *
 * Lines are joined according to the @join rule. Possible values are
 * ART_PATH_STROKE_JOIN_MITER (for mitered joins),
 * ART_PATH_STROKE_JOIN_ROUND (for round joins), and
 * ART_PATH_STROKE_JOIN_BEVEL (for bevelled joins). The mitered join
 * is converted to a bevelled join if the miter would extend to a
 * distance of more than @miter_limit * @line_width from the actual
 * join point.
 *
 * If there are open subpaths, the ends of these subpaths are capped
 * according to the @cap rule. Possible values are
 * ART_PATH_STROKE_CAP_BUTT (squared cap, extends exactly to end
 * point), ART_PATH_STROKE_CAP_ROUND (rounded half-circle centered at
 * the end point), and ART_PATH_STROKE_CAP_SQUARE (squared cap,
 * extending half @line_width past the end point).
 *
 * The @flatness parameter controls the accuracy of the rendering. It
 * is most important for determining the number of points to use to
 * approximate circular arcs for round lines and joins. In general, the
 * resulting vector path will be within @flatness pixels of the "ideal"
 * path containing actual circular arcs. I reserve the right to use
 * the @flatness parameter to convert bevelled joins to miters for very
 * small turn angles, as this would reduce the number of points in the
 * resulting outline path.
 *
 * The resulting path is "clean" with respect to self-intersections, i.e.
 * the winding number is 0 or 1 at each point.
 *
 * Return value: Resulting stroked outline in svp format.
 **/
static psd_art_svp * psd_art_svp_vpath_stroke(psd_art_vpath *vpath,
	psd_double line_width, psd_double flatness)
{
	psd_art_vpath *vpath_stroke;
	psd_art_svp *svp, *svp2;
	psd_art_svp_writer *swr;

	vpath_stroke = psd_art_svp_vpath_stroke_raw(vpath, line_width, flatness);
	svp = psd_art_svp_from_vpath(vpath_stroke);
	psd_free(vpath_stroke);

	swr = psd_art_svp_writer_rewind_new(PSD_ART_WIND_RULE_NONZERO);
	psd_art_svp_intersector(svp, swr);

	svp2 = psd_art_svp_writer_rewind_reap(swr);
	psd_art_svp_free(svp);
	
	return svp2;
}

static void psd_stroke_scan_convert(psd_gimp_scan_convert *sc, psd_int stroke_size)
{
	psd_art_svp *stroke;
	psd_int i;

	if(sc->need_closing)
		psd_scan_convert_close_add_points(sc);

	if(sc->ratio_xy != 1.0)
	{
		for(i = 0; i < sc->num_nodes; i++)
			sc->vpath[i].x *= sc->ratio_xy;
	}

	if(sc->vpath)
	{
		stroke = psd_art_svp_vpath_stroke(sc->vpath, stroke_size, 0.2);

		if(sc->ratio_xy != 1.0)
		{
			psd_art_svp_seg *segment;
			psd_art_point  *point;
			psd_int i, j;

			for(i = 0; i < stroke->n_segs; i++)
			{
				segment = stroke->segs + i;
				segment->bbox.x0 /= sc->ratio_xy;
				segment->bbox.x1 /= sc->ratio_xy;

				for(j = 0; j < segment->n_points; j++)
				{
					point = segment->points + j;
					point->x /= sc->ratio_xy;
				}
			}
		}

		sc->svp = stroke;
	}
}

/* private function to convert the vpath to a svp when not using
 * gimp_scan_convert_stroke
 */
static void psd_scan_convert_finish(psd_gimp_scan_convert *sc)
{
	psd_art_svp       *svp, *svp2;
	psd_art_svp_writer *swr;

	/* return gracefully on empty path */
	if(!sc->vpath)
		return;

	if(sc->need_closing)
		psd_scan_convert_close_add_points(sc);

	if(sc->svp)
		return;   /* We already have a valid SVP */

	/* Debug output of libart path */
	/* {
	*   gint i;
	*   for (i = 0; i < sc->num_nodes + 1; i++)
	*     {
	*       g_printerr ("X: %f, Y: %f, Type: %d\n", sc->vpath[i].x,
	*                                               sc->vpath[i].y,
	*                                               sc->vpath[i].code );
	*     }
	* }
	*/

	if(sc->have_open)
	{
		psd_int i;

		for(i = 0; i < sc->num_nodes; i++)
			if(sc->vpath[i].code == PSD_ART_MOVETO_OPEN)
				sc->vpath[i].code = PSD_ART_MOVETO;
	}

	svp = psd_art_svp_from_vpath(sc->vpath);

	swr = psd_art_svp_writer_rewind_new(PSD_ART_WIND_RULE_ODDEVEN);
	psd_art_svp_intersector(svp, swr);

	svp2 = psd_art_svp_writer_rewind_reap(swr); /* this also frees swr */

	psd_art_svp_free(svp);

	sc->svp = svp2;
}

/* Render the sorted vector path in the given rectangle, antialiased.

   This interface uses a callback for the actual pixel rendering. The
   callback is called y1 - y0 times (once for each scan line). The y
   coordinate is given as an argument for convenience (it could be
   stored in the callback's private data and incremented on each
   call).

   The rendered polygon is represented in a semi-runlength format: a
   start value and a sequence of "steps". Each step has an x
   coordinate and a value delta. The resulting value at position x is
   equal to the sum of the start value and all step delta values for
   which the step x coordinate is less than or equal to x. An
   efficient algorithm will traverse the steps left to right, keeping
   a running sum.

   All x coordinates in the steps are guaranteed to be x0 <= x < x1.
   (This guarantee is a change from the gfonted vpaar renderer, and is
   designed to simplify the callback).

   There is now a further guarantee that no two steps will have the
   same x value. This may allow for further speedup and simplification
   of renderers.

   The value 0x8000 represents 0% coverage by the polygon, while
   0xff8000 represents 100% coverage. This format is designed so that
   >> 16 results in a standard 0x00..0xff value range, with nice
   rounding.

   Status of this routine:

   Basic correctness: OK

   Numerical stability: pretty good, although probably not
   bulletproof.

   Speed: Needs more aggressive culling of bounding boxes.  Can
   probably speed up the [x0,x1) clipping of step values.  Can do more
   of the step calculation in fixed point.

   Precision: No known problems, although it should be tested
   thoroughly, especially for symmetry.

*/

static psd_art_svp_render_aa_iter * psd_art_svp_render_aa_iter_new(const psd_art_svp *svp,
	psd_int x0, psd_int y0, psd_int x1, psd_int y1)
{
	psd_art_svp_render_aa_iter *iter = (psd_art_svp_render_aa_iter *)psd_malloc(sizeof(psd_art_svp_render_aa_iter));

	iter->svp = svp;
	iter->y = y0;
	iter->x0 = x0;
	iter->x1 = x1;
	iter->seg_ix = 0;

	iter->active_segs = (psd_int *)psd_malloc(svp->n_segs * sizeof(psd_int));
	iter->cursor = (psd_int *)psd_malloc(svp->n_segs * sizeof(psd_int));
	iter->seg_x = (psd_double *)psd_malloc(svp->n_segs * sizeof(psd_double));
	iter->seg_dx = (psd_double *)psd_malloc(svp->n_segs * sizeof(psd_double));
	iter->steps = (psd_art_svp_render_aa_step *)psd_malloc((x1 - x0) * sizeof(psd_art_svp_render_aa_step));
	iter->n_active_segs = 0;

	return iter;
}

static void psd_art_svp_render_insert_active(psd_int i, psd_int *active_segs, psd_int n_active_segs,
	psd_double *seg_x, psd_double *seg_dx)
{
	psd_int j;
	psd_double x;
	psd_int tmp1, tmp2;

	/* this is a cheap hack to get ^'s sorted correctly */
	x = seg_x[i] + 0.001 * seg_dx[i];
	for(j = 0; j < n_active_segs && seg_x[active_segs[j]] < x; j++);

	tmp1 = i;
	while(j < n_active_segs)
	{
		tmp2 = active_segs[j];
		active_segs[j] = tmp1;
		tmp1 = tmp2;
		j++;
	}
	active_segs[j] = tmp1;
}

static void psd_art_svp_render_delete_active(psd_int *active_segs, psd_int j, psd_int n_active_segs)
{
	psd_int k;

	for(k = j; k < n_active_segs; k++)
		active_segs[k] = active_segs[k + 1];
}

#define PSD_ADD_STEP(xpos, xdelta)                      \
	/* stereotype code fragment for adding a step */    \
	if(n_steps == 0 || steps[n_steps - 1].x < xpos)     \
	{                                                   \
		sx = n_steps;                                   \
		steps[sx].x = xpos;                             \
		steps[sx].delta = xdelta;                       \
		n_steps++;                                      \
	}                                                   \
	else                                                \
	{                                                   \
		for(sx = n_steps; sx > 0; sx--)                 \
		{                                               \
			if(steps[sx - 1].x == xpos)                 \
			{                                           \
				steps[sx - 1].delta += xdelta;          \
				sx = n_steps;                           \
				break;                                  \
			}                                           \
			else if(steps[sx - 1].x < xpos)             \
			{                                           \
				break;                                  \
			}                                           \
		}                                               \
		if(sx < n_steps)                                \
		{                                               \
			memmove(&steps[sx + 1], &steps[sx],         \
				(n_steps - sx) * sizeof(steps[0]));     \
			steps[sx].x = xpos;                         \
			steps[sx].delta = xdelta;                   \
			n_steps++;                                  \
		}                                               \
	}

static void psd_art_svp_render_aa_iter_step(psd_art_svp_render_aa_iter *iter, psd_int *p_start,
	psd_art_svp_render_aa_step **p_steps, psd_int *p_n_steps)
{
	const psd_art_svp *svp = iter->svp;
	psd_int *active_segs = iter->active_segs;
	psd_int n_active_segs = iter->n_active_segs;
	psd_int *cursor = iter->cursor;
	psd_double *seg_x = iter->seg_x;
	psd_double *seg_dx = iter->seg_dx;
	psd_int i = iter->seg_ix;
	psd_int j;
	psd_int x0 = iter->x0;
	psd_int x1 = iter->x1;
	psd_int y = iter->y;
	psd_int seg_index;

	psd_int x;
	psd_art_svp_render_aa_step *steps = iter->steps;
	psd_int n_steps;
	psd_double y_top, y_bot;
	psd_double x_top, x_bot;
	psd_double x_min, x_max;
	psd_int ix_min, ix_max;
	psd_double delta; /* delta should be psd_int too? */
	psd_int last, this;
	psd_int xdelta;
	psd_double rslope, drslope;
	psd_int start;
	const psd_art_svp_seg *seg;
	psd_int curs;
	psd_double dy;

	psd_int sx;

	/* insert new active segments */
	for(; i < svp->n_segs && svp->segs[i].bbox.y0 < y + 1; i++)
	{
		if(svp->segs[i].bbox.y1 > y &&
			svp->segs[i].bbox.x0 < x1)
		{
			seg = &svp->segs[i];
			/* move cursor to topmost vector which overlaps [y,y+1) */
			for(curs = 0; seg->points[curs + 1].y < y; curs++);
				cursor[i] = curs;
			dy = seg->points[curs + 1].y - seg->points[curs].y;
			if(PSD_ABS(dy) >= PSD_EPSILON)
				seg_dx[i] = (seg->points[curs + 1].x - seg->points[curs].x) / dy;
			else
				seg_dx[i] = 1e12;
			seg_x[i] = seg->points[curs].x +
				(y - seg->points[curs].y) * seg_dx[i];
			psd_art_svp_render_insert_active(i, active_segs, n_active_segs++,
				seg_x, seg_dx);
		}
	}

	n_steps = 0;

	/* render the runlengths, advancing and deleting as we go */
	start = 0x8000;

	for(j = 0; j < n_active_segs; j++)
	{
		seg_index = active_segs[j];
		seg = &svp->segs[seg_index];
		curs = cursor[seg_index];
		while(curs != seg->n_points - 1 &&
			seg->points[curs].y < y + 1)
		{
			y_top = y;
			if(y_top < seg->points[curs].y)
				y_top = seg->points[curs].y;
			y_bot = y + 1;
			if(y_bot > seg->points[curs + 1].y)
				y_bot = seg->points[curs + 1].y;
			if(y_top != y_bot)
			{
				delta = (seg->dir ? 16711680.0 : -16711680.0) *
					(y_bot - y_top);
				x_top = seg_x[seg_index] + (y_top - y) * seg_dx[seg_index];
				x_bot = seg_x[seg_index] + (y_bot - y) * seg_dx[seg_index];
				if(x_top < x_bot)
				{
					x_min = x_top;
					x_max = x_bot;
				}
				else
				{
					x_min = x_bot;
					x_max = x_top;
				}
				ix_min = (psd_int)floor(x_min);
				ix_max = (psd_int)floor(x_max);
				if(ix_min >= x1)
				{
					/* skip; it starts to the right of the render region */
				}
				else if(ix_max < x0)
					/* it ends to the left of the render region */
					start += (psd_int)delta;
				else if(ix_min == ix_max)
				{
					/* case 1, antialias a single pixel */
					xdelta = (psd_int)((ix_min + 1 - (x_min + x_max) * 0.5) * delta);

					PSD_ADD_STEP(ix_min, xdelta)

					if(ix_min + 1 < x1)
					{
						xdelta = (psd_int)delta - xdelta;

						PSD_ADD_STEP(ix_min + 1, xdelta)
					}
				}
				else
				{
					/* case 2, antialias a run */
					rslope = 1.0 / PSD_ABS(seg_dx[seg_index]);
					drslope = delta * rslope;
					last = (psd_int)(drslope * 0.5 *
						(ix_min + 1 - x_min) * (ix_min + 1 - x_min));
					xdelta = last;
					if(ix_min >= x0)
					{
						PSD_ADD_STEP(ix_min, xdelta)

						x = ix_min + 1;
					}
					else
					{
						start += last;
						x = x0;
					}
					if(ix_max > x1)
						ix_max = x1;
					for(; x < ix_max; x++)
					{
						this = (psd_int)((seg->dir ? 16711680.0 : -16711680.0) * rslope *
							(x + 0.5 - x_min));
						xdelta = this - last;
						last = this;

						PSD_ADD_STEP(x, xdelta)
					}
					if(x < x1)
					{
						this = (psd_int)(delta * (1 - 0.5 *
							(x_max - ix_max) * (x_max - ix_max) *
							rslope));
						xdelta = this - last;
						last = this;

						PSD_ADD_STEP(x, xdelta)

						if(x + 1 < x1)
						{
							xdelta = (psd_int)delta - last;

							PSD_ADD_STEP(x + 1, xdelta)
						}
					}
				}
			}
			curs++;
			if(curs != seg->n_points - 1 &&
				seg->points[curs].y < y + 1)
			{
				dy = seg->points[curs + 1].y - seg->points[curs].y;
				if (PSD_ABS(dy) >= PSD_EPSILON)
					seg_dx[seg_index] = (seg->points[curs + 1].x -
						seg->points[curs].x) / dy;
				else
					seg_dx[seg_index] = 1e12;
				seg_x[seg_index] = seg->points[curs].x +
					(y - seg->points[curs].y) * seg_dx[seg_index];
			}
			/* break here, instead of duplicating predicate in while? */
		}
		if(seg->points[curs].y >= y + 1)
		{
			curs--;
			cursor[seg_index] = curs;
			seg_x[seg_index] += seg_dx[seg_index];
		}
		else
		{
			psd_art_svp_render_delete_active(active_segs, j--,
				--n_active_segs);
		}
	}

	*p_start = start;
	*p_steps = steps;
	*p_n_steps = n_steps;

	iter->seg_ix = i;
	iter->n_active_segs = n_active_segs;
	iter->y++;
}

static void psd_art_svp_render_aa_iter_done(psd_art_svp_render_aa_iter *iter)
{
	psd_free(iter->steps);

	psd_free(iter->seg_dx);
	psd_free(iter->seg_x);
	psd_free(iter->cursor);
	psd_free(iter->active_segs);
	psd_free(iter);
}

/**
 * art_svp_render_aa: Render SVP antialiased.
 * @svp: The #ArtSVP to render.
 * @x0: Left coordinate of destination rectangle.
 * @y0: Top coordinate of destination rectangle.
 * @x1: Right coordinate of destination rectangle.
 * @y1: Bottom coordinate of destination rectangle.
 * @callback: The callback which actually paints the pixels.
 * @callback_data: Private data for @callback.
 *
 * Renders the sorted vector path in the given rectangle, antialiased.
 *
 * This interface uses a callback for the actual pixel rendering. The
 * callback is called @y1 - @y0 times (once for each scan line). The y
 * coordinate is given as an argument for convenience (it could be
 * stored in the callback's private data and incremented on each
 * call).
 *
 * The rendered polygon is represented in a semi-runlength format: a
 * start value and a sequence of "steps". Each step has an x
 * coordinate and a value delta. The resulting value at position x is
 * equal to the sum of the start value and all step delta values for
 * which the step x coordinate is less than or equal to x. An
 * efficient algorithm will traverse the steps left to right, keeping
 * a running sum.
 *
 * All x coordinates in the steps are guaranteed to be @x0 <= x < @x1.
 * (This guarantee is a change from the gfonted vpaar renderer from
 * which this routine is derived, and is designed to simplify the
 * callback).
 *
 * The value 0x8000 represents 0% coverage by the polygon, while
 * 0xff8000 represents 100% coverage. This format is designed so that
 * >> 16 results in a standard 0x00..0xff value range, with nice
 * rounding.
 * 
 **/
static void psd_art_svp_render_aa(const psd_art_svp *svp,
	psd_int x0, psd_int y0, psd_int x1, psd_int y1,
	void (*callback) (void *callback_data,
				     psd_int y,
				     psd_int start,
				     psd_art_svp_render_aa_step *steps, psd_int n_steps),
	void *callback_data)
{
	psd_art_svp_render_aa_iter *iter;
	psd_int y;
	psd_int start;
	psd_art_svp_render_aa_step *steps;
	psd_int n_steps;

	iter = psd_art_svp_render_aa_iter_new(svp, x0, y0, x1, y1);

	for(y = y0; y < y1; y++)
	{
		psd_art_svp_render_aa_iter_step(iter, &start, &steps, &n_steps);
			(*callback)(callback_data, y, start, steps, n_steps);
	}

	psd_art_svp_render_aa_iter_done(iter);
}

static void psd_scan_convert_render_callback(void * user_data,
	psd_int y, psd_int start_value, psd_art_svp_render_aa_step *steps, psd_int n_steps)
{
	psd_gimp_scan_convert *sc = (psd_gimp_scan_convert *)user_data;
	psd_int cur_value = start_value;
	psd_int k, run_x0, run_x1;
	psd_argb_color * image_data = sc->dst_bmp->image_data + y * sc->dst_bmp->width;

#define VALUE_TO_PIXEL(x) ((x) >> 16)

	if(n_steps > 0)
	{
		run_x1 = steps[0].x;

		if(run_x1 > sc->x0)
		{
			psd_color_memset(image_data + sc->x0, 
				VALUE_TO_PIXEL(cur_value) << 24, run_x1 - sc->x0);
		}

		for(k = 0; k < n_steps - 1; k++)
		{
			cur_value += steps[k].delta;
			cur_value = PSD_MAX(cur_value, 0);

			run_x0 = run_x1;
			run_x1 = steps[k + 1].x;

			if(run_x1 > run_x0)
				psd_color_memset(image_data + run_x0, 
					VALUE_TO_PIXEL(cur_value) << 24, run_x1 - run_x0);
		}

		if(sc->x1 > run_x1)
		{
			cur_value += steps[k].delta;
			cur_value = PSD_MAX(cur_value, 0);
			psd_color_memset(image_data + run_x1, 
				VALUE_TO_PIXEL(cur_value) << 24, sc->x1 - run_x1);
		}
	}
	else
	{
		psd_color_memset(image_data + sc->x0, 
			VALUE_TO_PIXEL(cur_value) << 24, sc->x1 - sc->x0);
	}
}

static void psd_scan_convert_render_internal(psd_gimp_scan_convert *sc, psd_bitmap * dst_bmp, 
	psd_int x1, psd_int y1, psd_int x2, psd_int y2)
{
	psd_scan_convert_finish(sc);

	if(!sc->svp)
		return;

	sc->dst_bmp = dst_bmp;
	sc->x0 = x1;
	sc->x1 = x2;

	psd_art_svp_render_aa(sc->svp,
		x1, y1, x2, y2,
		psd_scan_convert_render_callback, sc);
}

static void psd_stroke_boundary(psd_bitmap * dst_bmp, psd_int stroke_size, 
	psd_gimp_bound_seg *segs, psd_int num_segs,
	psd_int x1, psd_int y1, psd_int x2, psd_int y2)
{
	psd_gimp_scan_convert *scan_convert;
	psd_gimp_bound_seg   *stroke_segs;
	psd_int             n_stroke_segs;
	psd_gimp_vector2     *points;
	psd_int             n_points;
	psd_int             seg;
	psd_int             i;

	stroke_segs = psd_boundary_sort(segs, num_segs, &n_stroke_segs);

	if(n_stroke_segs == 0)
		return;

	scan_convert = (psd_gimp_scan_convert *)psd_malloc(sizeof(psd_gimp_scan_convert));
	memset(scan_convert, 0, sizeof(psd_gimp_scan_convert));
	scan_convert->ratio_xy = 1.0;

	points = (psd_gimp_vector2 *)psd_malloc(sizeof(psd_gimp_vector2) * (num_segs + 4));
	memset(points, 0, sizeof(psd_gimp_vector2) * (num_segs + 4));

	seg = 0;
	n_points = 0;

	points[n_points].x = (psd_double)(stroke_segs[0].x1);
	points[n_points].y = (psd_double)(stroke_segs[0].y1);

	n_points++;

	for(i = 0; i < n_stroke_segs; i++)
	{
		while(stroke_segs[seg].x1 != -1 ||
			stroke_segs[seg].x2 != -1 ||
			stroke_segs[seg].y1 != -1 ||
			stroke_segs[seg].y2 != -1)
		{
			points[n_points].x = (psd_double)(stroke_segs[seg].x1);
			points[n_points].y = (psd_double)(stroke_segs[seg].y1);

			n_points++;
			seg++;
		}

		/* Close the stroke points up */
		points[n_points] = points[0];

		n_points++;

		psd_scan_convert_add_polyline(scan_convert, n_points, points, psd_true);
		
		n_points = 0;
		seg++;

		points[n_points].x = (psd_double)(stroke_segs[seg].x1);
		points[n_points].y = (psd_double)(stroke_segs[seg].y1);

		n_points++;
	}

	psd_free(points);
	psd_free(stroke_segs);

	psd_stroke_scan_convert(scan_convert, stroke_size);

	/* render the stroke into it */
 	psd_scan_convert_render_internal(scan_convert, dst_bmp, 
 		PSD_MAX(0, x1 - stroke_size), PSD_MAX(0, y1 - stroke_size), 
 		PSD_MIN(dst_bmp->width, x2 + stroke_size), PSD_MIN(dst_bmp->height, y2 + stroke_size));

	psd_scan_convert_free(scan_convert);
}

psd_bool psd_draw_stroke(psd_bitmap * dst_bmp, psd_bitmap * src_bmp, psd_int stroke_size)
{
	psd_gimp_bound_seg *segs;
	psd_int num_segs;
	psd_int x1, y1, x2, y2;

	if(psd_get_boundary(src_bmp, &segs, &num_segs, &x1, &y1, &x2, &y2) == psd_false)
		return psd_false;

	psd_stroke_boundary(dst_bmp, stroke_size, segs, num_segs, x1, y1, x2, y2);

	return psd_true;
}

