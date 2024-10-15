/*
 * utils for libavcodec
 * Copyright (c) 2001 Fabrice Bellard
 * Copyright (c) 2002-2004 Michael Niedermayer <michaelni@gmx.at>
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

/**
 * @file
 * utils.
 */

#include "config.h"
#include "libavutil/atomic.h"
#include "libavutil/attributes.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/channel_layout.h"
#include "libavutil/crc.h"
#include "libavutil/frame.h"
#include "libavutil/internal.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/dict.h"
#include "avcodec.h"
#include "libavutil/opt.h"
#include "thread.h"
#include "internal.h"
#include "bytestream.h"
#include "version.h"
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#if CONFIG_ICONV
# include <iconv.h>
#endif

void avcodec_register_all(void)
{
}

int attribute_align_arg avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options)
{
    int ret = 0;

    if (codec->priv_data_size > 0) {
        if (!avctx->priv_data) {
            avctx->priv_data = av_mallocz(codec->priv_data_size);
            if (!avctx->priv_data) {
                ret = AVERROR(ENOMEM);
                goto end;
            }
        }
    } else {
        avctx->priv_data = NULL;
    }
    avctx->codec = codec;
    avctx->frame_number = 0;
    //    avctx->codec_descriptor = avcodec_descriptor_get(avctx->codec_id);

    avctx->thread_count = 1;

    avctx->pts_correction_num_faulty_pts =
    avctx->pts_correction_num_faulty_dts = 0;
    avctx->pts_correction_last_pts =
    avctx->pts_correction_last_dts = INT64_MIN;

    ret = avctx->codec->init(avctx);
    if (ret < 0) {
        goto free_and_end;
    }

    return 0;
free_and_end:
    av_freep(&avctx->priv_data);
    avctx->codec = NULL;
 end:
    return ret;
}

av_cold int avcodec_close(AVCodecContext *avctx)
{
    if (!avctx)
        return 0;

    if (avctx->codec && avctx->codec->close)
        avctx->codec->close(avctx);
    avctx->coded_frame = NULL;

    av_freep(&avctx->priv_data);
    avctx->codec = NULL;
    avctx->active_thread_type = 0;

    return 0;
}

int avcodec_default_execute(AVCodecContext *c, int (*func)(AVCodecContext *c2, void *arg2), void *arg, int *ret, int count, int size)
{
    int i;

    for (i = 0; i < count; i++) {
        int r = func(c, (char *)arg + i * size);
        if (ret)
            ret[i] = r;
    }
    return 0;
}

int avcodec_default_execute2(AVCodecContext *c, int (*func)(AVCodecContext *c2, void *arg2, int jobnr, int threadnr), void *arg, int *ret, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        int r = func(c, arg, i, 0);
        if (ret)
            ret[i] = r;
    }
    return 0;
}

int avcodec_default_get_buffer2(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((AVPixelFormat)frame->format);
    int i, h, linesize, bpp;

    for (i = 0; i < desc->nb_components; i++) {
        bpp = (desc->comp[i].depth_minus1 + 8) >> 3;
        linesize = FFALIGN(frame->width * bpp, 32);
        if (i == 1 || i == 2)
            linesize = FF_CEIL_RSHIFT(linesize, desc->log2_chroma_w);

        frame->linesize[i] = linesize;

        h = FFALIGN(frame->height, 32);
        if (i == 1 || i == 2)
            h = FF_CEIL_RSHIFT(h, desc->log2_chroma_h);
        
        frame->buf[i] = av_buffer_alloc(frame->linesize[i] * h + 32);
        if (!frame->buf[i])
            goto fail;

        frame->data[i] = frame->buf[i]->data;
    }
    return 0;
 fail:
    return -1;
}

int avcodec_get_context_defaults3(AVCodecContext *s, const AVCodec *codec)
{
    memset(s, 0, sizeof(AVCodecContext));

    s->codec_type = codec ? codec->type : AVMEDIA_TYPE_UNKNOWN;
    if (codec)
        s->codec_id = codec->id;

    s->time_base           = {0,1};
    s->framerate           = { 0, 1 };
    s->pkt_timebase        = { 0, 1 };
    s->get_buffer2         = avcodec_default_get_buffer2;
    //    s->get_format          = avcodec_default_get_format;
    s->execute             = avcodec_default_execute;
    s->execute2            = avcodec_default_execute2;
    s->sample_aspect_ratio = {0,1};
    s->pix_fmt             = AV_PIX_FMT_NONE;
    s->sample_fmt          = AV_SAMPLE_FMT_NONE;

    s->reordered_opaque    = AV_NOPTS_VALUE;
    if(codec && codec->priv_data_size){
        if(!s->priv_data){
            s->priv_data= av_mallocz(codec->priv_data_size);
            if (!s->priv_data) {
                return AVERROR(ENOMEM);
            }
        }
    }
    return 0;
}

int ff_thread_ref_frame(ThreadFrame *dst, ThreadFrame *src)
{
    return 0;
}

AVCodecContext *avcodec_alloc_context3(const AVCodec *codec)
{
    AVCodecContext *avctx= (AVCodecContext*)av_malloc(sizeof(AVCodecContext));

    if (!avctx)
        return NULL;

    if(avcodec_get_context_defaults3(avctx, codec) < 0){
        av_free(avctx);
        return NULL;
    }

    return avctx;
}

int attribute_align_arg avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture,
                                              int *got_picture_ptr,
                                              const AVPacket *avpkt)
{
    int ret;
    // copy to ensure we do not change avpkt
    AVPacket tmp = *avpkt;

    if (!avctx->codec)
        return AVERROR(EINVAL);
    if (avctx->codec->type != AVMEDIA_TYPE_VIDEO) {
        av_log(avctx, AV_LOG_ERROR, "Invalid media type for video\n");
        return AVERROR(EINVAL);
    }

    *got_picture_ptr = 0;
    if ((avctx->coded_width || avctx->coded_height) && av_image_check_size(avctx->coded_width, avctx->coded_height, 0, avctx))
        return AVERROR(EINVAL);

    av_frame_unref(picture);

    if ((avctx->codec->capabilities & CODEC_CAP_DELAY) || avpkt->size || (avctx->active_thread_type & FF_THREAD_FRAME)) {

        ret = avctx->codec->decode(avctx, picture, got_picture_ptr,
                                   &tmp);

        if (*got_picture_ptr) {
            avctx->frame_number++;
        } else {
            av_frame_unref(picture);
        }
    } else {
        ret = 0;
    }

    /* many decoders assign whole AVFrames, thus overwriting extended_data;
     * make sure it's set correctly */
    //    av_assert0(!picture->extended_data || picture->extended_data == picture->data);

    return ret;
}

int av_image_check_size(unsigned int w, unsigned int h, int log_offset, void *log_ctx)
{
    if ((int)w>0 && (int)h>0 && (w+128) < (INT_MAX/8) / (h + 128))
        return 0;
    else
        return AVERROR(EINVAL);
}

int ff_set_sar(AVCodecContext *avctx, AVRational sar)
{
    return 0;
}

static int get_buffer_internal(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    int override_dimensions = 1;
    int ret;

    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        if ((ret = av_image_check_size(avctx->width, avctx->height, 0, avctx)) < 0 || avctx->pix_fmt<0) {
            av_log(avctx, AV_LOG_ERROR, "video_get_buffer: image parameters invalid\n");
            return AVERROR(EINVAL);
        }
    }
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (frame->width <= 0 || frame->height <= 0) {
            frame->width  = FFMAX(avctx->width,  FF_CEIL_RSHIFT(avctx->coded_width,  avctx->lowres));
            frame->height = FFMAX(avctx->height, FF_CEIL_RSHIFT(avctx->coded_height, avctx->lowres));
            override_dimensions = 0;
        }
        frame->format              = avctx->pix_fmt;
    }
    ret = avctx->get_buffer2(avctx, frame, flags);

    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO && !override_dimensions) {
        frame->width  = avctx->width;
        frame->height = avctx->height;
    }

    return ret;
}

int ff_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    int ret = get_buffer_internal(avctx, frame, flags);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "get_buffer() failed\n");
    return ret;
}

int ff_thread_get_buffer(AVCodecContext *avctx, ThreadFrame *f, int flags)
{
    f->owner = avctx;
    return ff_get_buffer(avctx, f->f, flags);
}

void ff_thread_release_buffer(AVCodecContext *avctx, ThreadFrame *f)
{
    if (f->f)
        av_frame_unref(f->f);
}

void ff_thread_finish_setup(AVCodecContext *avctx)
{
}

void ff_thread_report_progress(ThreadFrame *f, int progress, int field)
{
}

void ff_thread_await_progress(ThreadFrame *f, int progress, int field)
{
}

int ff_thread_can_start_frame(AVCodecContext *avctx)
{
    return 1;
}

int ff_alloc_entries(AVCodecContext *avctx, int count)
{
    return 0;
}

void ff_reset_entries(AVCodecContext *avctx)
{
}

void ff_thread_await_progress2(AVCodecContext *avctx, int field, int thread, int shift)
{
}

void ff_thread_report_progress2(AVCodecContext *avctx, int field, int thread, int n)
{
}

void av_init_packet(AVPacket *pkt)
{
    pkt->pts                  = AV_NOPTS_VALUE;
    pkt->dts                  = AV_NOPTS_VALUE;
    pkt->pos                  = -1;
    pkt->duration             = 0;
    pkt->convergence_duration = 0;
    pkt->flags                = 0;
    pkt->stream_index         = 0;
    pkt->buf                  = NULL;
    pkt->side_data            = NULL;
    pkt->side_data_elems      = 0;
}
