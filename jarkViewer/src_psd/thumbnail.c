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
 * $Id: thumbnail.c, created by Patrick in 2006.05.31, libpsd@graphest.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libpsd.h"
#include "psd_config.h"
#include "psd_system.h"
#include "psd_stream.h"
#include "psd_color.h"
#include "psd_math.h"

#ifdef PSD_INCLUDE_LIBJPEG
#include <setjmp.h>
#include "jpeglib.h"


/* we are a "source manager" as far as libjpeg is concerned */
#define JPEG_PROG_BUF_SIZE 32768

typedef struct {
	struct jpeg_source_mgr pub;					/* public fields */

	JOCTET buffer[JPEG_PROG_BUF_SIZE];			/* start of buffer */
	long  skip_next;							/* number of bytes to skip next read */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

/* error handler data */
struct error_handler_data {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

/* progressive loader context */
typedef struct {
	psd_uchar *						pixels;
	psd_uchar *						dptr;   /* current position in image_data */
	psd_int							width;
	psd_int							height;
	psd_int							number_channels;
	psd_int							row_stride;

	boolean							did_prescan;/* are we in image data yet? */
	boolean							got_header; /* have we loaded jpeg header? */
	boolean							src_initialized;/* TRUE when jpeg lib initialized */
	boolean                 		in_output;  /* did we get suspended in an output pass? */
	struct jpeg_decompress_struct 	cinfo;
	struct error_handler_data     	jerr;
} JpegProgContext;


/**** Progressive image loading handling *****/

static void fatal_error_handler (j_common_ptr cinfo)
{
	/* FIXME:
	 * We should somehow signal what error occurred to the caller so the
	 * caller can handle the error message */
	struct error_handler_data *errmgr;

	errmgr = (struct error_handler_data *) cinfo->err;
	cinfo->err->output_message (cinfo);
	longjmp (errmgr->setjmp_buffer, 1);
}

/* these routines required because we are acting as a source manager for */
/* libjpeg. */
static void init_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;
	src->skip_next = 0;
}

/* for progressive loading (called "I/O Suspension" by libjpeg docs) */
/* we do nothing except return "FALSE"                               */
static boolean fill_input_buffer (j_decompress_ptr cinfo)
{
	return FALSE;
}

static void skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;
	long   num_can_do;

	/* move as far as we can into current buffer */
	/* then set skip_next to catch the rest      */
	if (num_bytes > 0) {
		num_can_do = PSD_MIN(src->pub.bytes_in_buffer, num_bytes);
		src->pub.next_input_byte += num_can_do;
		src->pub.bytes_in_buffer -= num_can_do;

		src->skip_next = num_bytes - num_can_do;
	}
}

static void term_source (j_decompress_ptr cinfo)
{
	/* XXXX - probably should scream something has happened */
}

/* explode gray image data from jpeg library into rgb components in pixbuf */
static void explode_gray_into_buf (struct jpeg_decompress_struct *cinfo, psd_uchar **lines) 
{
	psd_int i, j;
	psd_int w;

	if(! (cinfo->output_components == 1))
		return;

	/* Expand grey->colour.  Expand from the end of the
	 * memory down, so we can use the same buffer.
	 */
	w = cinfo->image_width;
	for (i = cinfo->rec_outbuf_height - 1; i >= 0; i--) {
		psd_uchar *from, *to;
		
		from = lines[i] + w - 1;
		to = lines[i] + (w - 1) * 3;
		for (j = w - 1; j >= 0; j--) {
			to[0] = from[0];
			to[1] = from[0];
			to[2] = from[0];
			to -= 3;
			from--;
		}
	}
}

static void convert_cmyk_to_rgb (struct jpeg_decompress_struct *cinfo, psd_uchar **lines) 
{
	psd_int i, j;

	if(! (cinfo->output_components == 4))
		return;

	for (i = cinfo->rec_outbuf_height - 1; i >= 0; i--) {
		psd_uchar *p;
		
		p = lines[i];
		for (j = 0; j < cinfo->image_width; j++) {
			psd_int c, m, y, k;
			c = p[0];
			m = p[1];
			y = p[2];
			k = p[3];
			if (cinfo->saw_Adobe_marker) {
				p[0] = k*c / 255;
				p[1] = k*m / 255;
				p[2] = k*y / 255;
			}
			else {
				p[0] = (255 - k)*(255 - c) / 255;
				p[1] = (255 - k)*(255 - m) / 255;
				p[2] = (255 - k)*(255 - y) / 255;
			}
			p[3] = 255;
			p += 4;
		}
	}
}

static JpegProgContext * psd_jpeg_image_begin_load(void)
{
	JpegProgContext * jpeg_context;
	my_source_mgr   *src;

	jpeg_context = (JpegProgContext *)psd_malloc(sizeof(JpegProgContext));
	if(jpeg_context == NULL)
		return NULL;
	memset(jpeg_context, 0, sizeof(JpegProgContext));

	jpeg_context->got_header = FALSE;
	jpeg_context->did_prescan = FALSE;
	jpeg_context->src_initialized = FALSE;
	jpeg_context->in_output = FALSE;
	
	/* create libjpeg structures */
	jpeg_create_decompress (&jpeg_context->cinfo);

	jpeg_context->cinfo.src = (struct jpeg_source_mgr *)psd_malloc(sizeof(my_source_mgr));
	src = (my_src_ptr) jpeg_context->cinfo.src;

	jpeg_context->cinfo.err = jpeg_std_error (&jpeg_context->jerr.pub);
	jpeg_context->jerr.pub.error_exit = fatal_error_handler;

	src = (my_src_ptr) jpeg_context->cinfo.src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

	return jpeg_context;
}

static boolean psd_jpeg_image_load_increment(JpegProgContext * jpeg_context, psd_uchar * buffer, psd_int size)
{
	struct jpeg_decompress_struct *cinfo;
	my_src_ptr  src;
	psd_int num_left, num_copy;
	psd_int last_bytes_left;
	psd_int spinguard;
	boolean first;
	const psd_uchar * bufhd;

	src = (my_src_ptr) jpeg_context->cinfo.src;

	cinfo = &jpeg_context->cinfo;

	/* check for fatal error */
	if (setjmp (jpeg_context->jerr.setjmp_buffer)) {
		return FALSE;
	}

	/* skip over data if requested, handle psd_uint sizes cleanly */
	/* only can happen if we've already called jpeg_get_header once   */
	if (jpeg_context->src_initialized && src->skip_next) {
		if (src->skip_next > size) {
			src->skip_next -= size;
			return TRUE;
		} else {
			num_left = size - src->skip_next;
			bufhd = buffer + src->skip_next;
			src->skip_next = 0;
		}
	} else {
		num_left = size;
		bufhd = buffer;
	}

	if (num_left == 0)
		return TRUE;

	last_bytes_left = 0;
	spinguard = 0;
	first = TRUE;
	while (TRUE) {

		/* handle any data from caller we haven't processed yet */
		if (num_left > 0) {
			if(src->pub.bytes_in_buffer && 
			   src->pub.next_input_byte != src->buffer)
				memmove(src->buffer, src->pub.next_input_byte,
					src->pub.bytes_in_buffer);


			num_copy = PSD_MIN(JPEG_PROG_BUF_SIZE - src->pub.bytes_in_buffer,
					num_left);

			memcpy(src->buffer + src->pub.bytes_in_buffer, bufhd,num_copy);
			src->pub.next_input_byte = src->buffer;
			src->pub.bytes_in_buffer += num_copy;
			bufhd += num_copy;
			num_left -= num_copy;
		} else {
		/* did anything change from last pass, if not return */
			if (first) {
				last_bytes_left = src->pub.bytes_in_buffer;
				first = FALSE;
			} else if (src->pub.bytes_in_buffer == last_bytes_left)
				spinguard++;
			else
				last_bytes_left = src->pub.bytes_in_buffer;
		}

		/* should not go through twice and not pull bytes out of buffer */
		if (spinguard > 2)
			return TRUE;

		/* try to load jpeg header */
		if (!jpeg_context->got_header) {
			psd_int rc;

			rc = jpeg_read_header (cinfo, TRUE);
			jpeg_context->src_initialized = TRUE;

			if (rc == JPEG_SUSPENDED)
				continue;

			jpeg_context->got_header = TRUE;

		} else if (!jpeg_context->did_prescan) {
			psd_int rc;
                        
			/* start decompression */
			cinfo->buffered_image = TRUE;
			rc = jpeg_start_decompress (cinfo);
			cinfo->do_fancy_upsampling = FALSE;
			cinfo->do_block_smoothing = FALSE;

			jpeg_context->number_channels = cinfo->output_components == 4 ? 4 : 3;
			jpeg_context->pixels = (psd_uchar *)psd_malloc(cinfo->image_width * 
				cinfo->image_height * jpeg_context->number_channels);
			if(jpeg_context->pixels == NULL)
				return FALSE;
			jpeg_context->width = cinfo->image_width;
			jpeg_context->height = cinfo->image_height;
			jpeg_context->row_stride = jpeg_context->width * jpeg_context->number_channels;

			/* Use pixbuf buffer to store decompressed data */
			jpeg_context->dptr = jpeg_context->pixels;

			if (rc == JPEG_SUSPENDED)
				continue;

			jpeg_context->did_prescan = TRUE;
		} else {
			/* we're decompressing so feed jpeg lib scanlines */
			psd_uchar *lines[4];
			psd_uchar **lptr;
			psd_uchar *rowptr;
			psd_int   nlines, i;

			/* keep going until we've done all passes */
			while (!jpeg_input_complete (cinfo)) {
				if (!jpeg_context->in_output) {
					if (jpeg_start_output (cinfo, cinfo->input_scan_number)) {
						jpeg_context->in_output = TRUE;
						jpeg_context->dptr = jpeg_context->pixels;
					}
					else
						break;
				}
				/* keep going until we've done all scanlines */
				while (cinfo->output_scanline < cinfo->output_height) {
					lptr = lines;
					rowptr = (psd_uchar *)jpeg_context->dptr;
					for (i=0; i < cinfo->rec_outbuf_height; i++) {
						*lptr++ = rowptr;
						rowptr += jpeg_context->row_stride;
					}
					
					nlines = jpeg_read_scanlines (cinfo, lines,
								      cinfo->rec_outbuf_height);
					if (nlines == 0)
						break;

					switch (cinfo->out_color_space) {
					    case JCS_GRAYSCALE:
						    explode_gray_into_buf (cinfo, lines);
						    break;
					    case JCS_RGB:
						    /* do nothing */
						    break;
					    case JCS_CMYK:
						    convert_cmyk_to_rgb (cinfo, lines);
						    break;
					    default:
						    return FALSE;
					}

					jpeg_context->dptr += nlines * jpeg_context->row_stride;
				}
				if (cinfo->output_scanline >= cinfo->output_height && 
				    jpeg_finish_output (cinfo))
					jpeg_context->in_output = FALSE;
				else
					break;
			}
			if (jpeg_input_complete (cinfo))
				/* did entire image */
				return TRUE;
			else
				continue;
		}
	}

	return TRUE;
}

static void psd_jpeg_image_stop_load(JpegProgContext * jpeg_context)
{
	psd_freeif(jpeg_context->pixels);
	
	/* if we have an error? */
	if (setjmp (jpeg_context->jerr.setjmp_buffer)) {
		jpeg_destroy_decompress (&jpeg_context->cinfo);
	} else 
	{
		jpeg_finish_decompress(&jpeg_context->cinfo);
		jpeg_destroy_decompress(&jpeg_context->cinfo);
	}

	if (jpeg_context->cinfo.src) {
		my_src_ptr src = (my_src_ptr) jpeg_context->cinfo.src;
		psd_free(src);
	}

	psd_free(jpeg_context);
}

psd_status psd_thumbnail_decode_jpeg(psd_argb_color ** dst_image, psd_int compress_len, psd_context * context)
{
	psd_uchar * buffer;
	JpegProgContext * jpeg_context;
	psd_int length, i;
	psd_argb_color * image_data, * data;
	psd_uchar * pixels;

	buffer = (psd_uchar *)psd_malloc(4096);
	if(buffer == NULL)
		return psd_status_malloc_failed;

	jpeg_context = psd_jpeg_image_begin_load();
	if(jpeg_context == NULL)
	{
		psd_free(buffer);
		return psd_status_malloc_failed;
	}

	while(compress_len > 0)
	{
		length = psd_stream_get(context, buffer, PSD_MIN(4096, compress_len));
		if(length > 0)
		{
			if(psd_jpeg_image_load_increment(jpeg_context, buffer, length) == psd_false)
			{
				psd_free(buffer);
				psd_jpeg_image_stop_load(jpeg_context);
				return psd_status_thumbnail_decode_error;
			}
		}
		else if(length == 0)
		{
			psd_free(buffer);
			psd_jpeg_image_stop_load(jpeg_context);
			return psd_status_thumbnail_decode_error;
		}
		compress_len -= length;
	}

	image_data = (psd_argb_color *)psd_malloc(jpeg_context->width * jpeg_context->height * 4);
	if(image_data == NULL)
	{
		psd_free(buffer);
		psd_jpeg_image_stop_load(jpeg_context);
		return psd_status_malloc_failed;
	}
	pixels = jpeg_context->pixels;
	data = image_data;
	if(jpeg_context->number_channels == 4)
	{
		for(i = jpeg_context->width * jpeg_context->height; i --; )
		{
			*data = PSD_ARGB_TO_COLOR(*(pixels + 3), *pixels, *(pixels + 1), *(pixels + 2));
			data ++;
			pixels += 4;
		}
	}
	else
	{
		for(i = jpeg_context->width * jpeg_context->height; i --; )
		{
			*data = PSD_RGB_TO_COLOR(*pixels, *(pixels + 1), *(pixels + 2));
			data ++;
			pixels += 3;
		}
	}

	*dst_image = image_data;
	psd_free(buffer);
	psd_jpeg_image_stop_load(jpeg_context);
	
	return psd_status_done;
}

psd_status psd_thumbnail_decode_raw(psd_argb_color ** dst_image, psd_int image_len, psd_context * context)
{
	// currently, we don't support the raw format
	// since we don't meet any psd file with raw thumbnail
	psd_assert(0);
	psd_stream_get_null(context, image_len);
	
	return psd_status_done;
}

#endif
