/*
 * pixel format descriptor
 * Copyright (c) 2009 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <string.h>

#include "avassert.h"
#include "avstring.h"
#include "common.h"
#include "pixfmt.h"
#include "pixdesc.h"
#include "internal.h"
#include "intreadwrite.h"
#include "version.h"

typedef struct {
    enum AVPixelFormat pix_fmt;
    AVPixFmtDescriptor desc;
} AVPixFmtDescriptorEntry;


static const AVPixFmtDescriptorEntry pix_desc[] = {
#ifdef USE_VAR_BIT_DEPTH
    {
        AV_PIX_FMT_YUV420P16LE,
        {
            //.name = "yuv420p16le",
            .nb_components = 3,
            .log2_chroma_w = 1,
            .log2_chroma_h = 1,
            .comp = {
                { 0, 1, 1, 0, 15 },        /* Y */
                { 1, 1, 1, 0, 15 },        /* U */
                { 2, 1, 1, 0, 15 },        /* V */
            },
        }
    },
    {
        AV_PIX_FMT_YUV422P16LE,
        {
            //.name = "yuv422p16le",
            .nb_components = 3,
            .log2_chroma_w = 1,
            .log2_chroma_h = 0,
            .flags = AV_PIX_FMT_FLAG_PLANAR,
            .comp = {
                { 0, 1, 1, 0, 15 },        /* Y */
                { 1, 1, 1, 0, 15 },        /* U */
                { 2, 1, 1, 0, 15 },        /* V */
            },
        }
    },
    {
        AV_PIX_FMT_YUV444P16LE,
        {
            //.name = "yuv444p16le",
            .nb_components = 3,
            .log2_chroma_w = 0,
            .log2_chroma_h = 0,
            .flags = AV_PIX_FMT_FLAG_PLANAR,
            .comp = {
                { 0, 1, 1, 0, 15 },        /* Y */
                { 1, 1, 1, 0, 15 },        /* U */
                { 2, 1, 1, 0, 15 },        /* V */
            },
        }
    },
    {
        AV_PIX_FMT_GRAY16LE,
        {
            //.name = "gray16le",
            .nb_components = 1,
            .log2_chroma_w = 0,
            .log2_chroma_h = 0,
            .comp = {
                { 0, 1, 1, 0, 15 },       /* Y */
            },
            //.alias = "y16le",
        },
    },
#else
    {
        AV_PIX_FMT_YUV420P,
        {
            //.name = "yuv420p",
            .nb_components = 3,
            .log2_chroma_w = 1,
            .log2_chroma_h = 1,
            .comp = {
                { 0, 1, 1, 0, 7 },        /* Y */
                { 1, 1, 1, 0, 7 },        /* U */
                { 2, 1, 1, 0, 7 },        /* V */
            },
        }
    },
    {
        AV_PIX_FMT_YUV422P,
        {
            //.name = "yuv422p",
            .nb_components = 3,
            .log2_chroma_w = 1,
            .log2_chroma_h = 0,
            .flags = AV_PIX_FMT_FLAG_PLANAR,
            .comp = {
                { 0, 1, 1, 0, 7 },        /* Y */
                { 1, 1, 1, 0, 7 },        /* U */
                { 2, 1, 1, 0, 7 },        /* V */
            },
        }
    },
    {
        AV_PIX_FMT_YUV444P,
        {
            //.name = "yuv444p",
            .nb_components = 3,
            .log2_chroma_w = 0,
            .log2_chroma_h = 0,
            .flags = AV_PIX_FMT_FLAG_PLANAR,
            .comp = {
                { 0, 1, 1, 0, 7 },        /* Y */
                { 1, 1, 1, 0, 7 },        /* U */
                { 2, 1, 1, 0, 7 },        /* V */
            },
        }
    },
    {
        AV_PIX_FMT_GRAY8,
        {
            //.name = "gray",
            .nb_components = 1,
            .log2_chroma_w = 0,
            .log2_chroma_h = 0,
            .comp = {
                { 0, 1, 1, 0, 7 },       /* Y */
            },
        },
    },
#endif
};

#define countof(x) (sizeof(x) / sizeof(x[0]))

const AVPixFmtDescriptor *av_pix_fmt_desc_get(enum AVPixelFormat pix_fmt)
{
    int i;
    for(i = 0; i < countof(pix_desc); i++) {
        if (pix_desc[i].pix_fmt == pix_fmt) {
            return &pix_desc[i].desc;
        }
    }
    return NULL;
}
