/*
 * Copyright (C) 2012 Ronald S. Bultje
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

#include "libavutil/attributes.h"
#include "libavutil/avassert.h"
#include "libavutil/common.h"
#include "videodsp.h"

#ifdef USE_PRED

#if !defined(USE_VAR_BIT_DEPTH)
#define BIT_DEPTH 8
#include "videodsp_template.c"
#undef BIT_DEPTH
#endif

#if defined(USE_VAR_BIT_DEPTH) || defined(USE_FULL)
#define BIT_DEPTH 16
#include "videodsp_template.c"
#undef BIT_DEPTH
#endif

#ifdef USE_FULL
static void just_return(uint8_t *buf, ptrdiff_t stride, int h)
{
}
#endif

av_cold void ff_videodsp_init(VideoDSPContext *ctx, int bpc)
{
#ifdef USE_FULL
    ctx->prefetch = just_return;
#endif

#ifdef USE_FULL
    if (bpc <= 8) {
        ctx->emulated_edge_mc = ff_emulated_edge_mc_8;
    } else {
        ctx->emulated_edge_mc = ff_emulated_edge_mc_16;
    }
#else
#if defined(USE_VAR_BIT_DEPTH)
    ctx->emulated_edge_mc = ff_emulated_edge_mc_var;
#else
    ctx->emulated_edge_mc = ff_emulated_edge_mc_8;
#endif
#endif /* !USE_FULL */

    if (ARCH_AARCH64)
        ff_videodsp_init_aarch64(ctx, bpc);
    if (ARCH_ARM)
        ff_videodsp_init_arm(ctx, bpc);
    if (ARCH_PPC)
        ff_videodsp_init_ppc(ctx, bpc);
    if (ARCH_X86)
        ff_videodsp_init_x86(ctx, bpc);
}

#endif
