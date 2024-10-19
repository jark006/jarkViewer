/*
 * libbpg
 * 
 * Copyright (c) 2014 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <math.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif


#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>


/* The following global defines are used:
   - USE_VAR_BIT_DEPTH : support of bit depth > 8 bits
   - USE_PRED : support of animations 
*/
   
#ifndef EMSCRIPTEN
#define USE_RGB48 /* support all pixel formats */
//#define DEBUG
#endif

#include <assert.h>
#include "libbpg.h"

#define BPG_HEADER_MAGIC 0x425047fb

#define ITAPS2 4 
#define ITAPS (2 * ITAPS2) /* number of taps of the interpolation filter */

#ifdef USE_VAR_BIT_DEPTH
typedef uint16_t PIXEL;
#else
typedef uint8_t PIXEL;
#endif

#define MAX_DATA_SIZE ((1 << 30) - 1)

typedef struct {
    int c_shift;
    int c_rnd;
    int c_one;
    int y_one, y_offset;
    int c_r_cr, c_g_cb, c_g_cr, c_b_cb;
    int c_center;
    int bit_depth;
    int limited_range;
} ColorConvertState;

typedef void ColorConvertFunc(ColorConvertState *s, 
                              uint8_t *dst, const PIXEL *y_ptr,
                              const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                              int n, int incr);

struct BPGDecoderContext {
    AVCodecContext *dec_ctx;
    AVCodecContext *alpha_dec_ctx;
    AVFrame *frame;
    AVFrame *alpha_frame;
    int w, h;
    BPGImageFormatEnum format;
    uint8_t c_h_phase; /* only used for 422 and 420 */
    uint8_t has_alpha; /* true if alpha or W plane */
    uint8_t bit_depth;
    uint8_t has_w_plane;
    uint8_t limited_range;
    uint8_t premultiplied_alpha;
    uint8_t has_animation;
    BPGColorSpaceEnum color_space;
    uint8_t keep_extension_data; /* true if the extension data must be
                                    kept during parsing */
    uint8_t decode_animation; /* true if animation decoding is enabled */
    BPGExtensionData *first_md;

    /* animation */
    uint16_t loop_count;
    uint16_t frame_delay_num;
    uint16_t frame_delay_den;
    uint8_t *input_buf;
    int input_buf_pos;
    int input_buf_len;

    /* the following is used for format conversion */
    uint8_t output_inited;
    BPGDecoderOutputFormat out_fmt;
    uint8_t is_rgba;
    uint8_t is_16bpp;
    uint8_t is_cmyk;
    int y; /* current line */
    int w2, h2;
    const uint8_t *y_buf, *cb_buf, *cr_buf, *a_buf;
    int y_linesize, cb_linesize, cr_linesize, a_linesize;
    PIXEL *cb_buf2, *cr_buf2, *cb_buf3[ITAPS], *cr_buf3[ITAPS];
    int16_t *c_buf4;
    ColorConvertState cvt;
    ColorConvertFunc *cvt_func;
};

/* return < 0 if error, otherwise the consumed length */
static int get_ue32(uint32_t *pv, const uint8_t *buf, int len)
{
    const uint8_t *p;
    uint32_t v;
    int a;

    if (len <= 0) 
        return -1;
    p = buf;
    a = *p++;
    len--;
    if (a < 0x80) {
        *pv = a;
        return 1;
    } else if (a == 0x80) {
        /* we don't accept non canonical encodings */
        return -1;
    }
    v = a & 0x7f;
    for(;;) {
        if (len <= 0)
            return -1;
        a = *p++;
        len--;
        v = (v << 7) | (a & 0x7f);
        if (!(a & 0x80))
            break;
    }
    *pv = v;
    return int(p - buf);
}

static int get_ue(uint32_t *pv, const uint8_t *buf, int len)
{
    int ret;
    ret = get_ue32(pv, buf, len);
    if (ret < 0)
        return ret;
    /* limit the maximum size to avoid overflows in buffer
       computations */
    if (*pv > MAX_DATA_SIZE)
        return -1;
    return ret;
}

static int build_msps(uint8_t **pbuf, int *pbuf_len,
                      const uint8_t *input_data, int input_data_len1,
                      int width, int height, int chroma_format_idc,
                      int bit_depth)
{
    int input_data_len = input_data_len1;
    int idx, msps_len, ret, buf_len, i;
    uint32_t len;
    uint8_t *buf, *msps_buf;

    *pbuf = NULL;

    /* build the modified SPS header to please libavcodec */
    ret = get_ue(&len, input_data, input_data_len);
    if (ret < 0)
        return -1;
    input_data += ret;
    input_data_len -= ret;
    
    if (len > input_data_len)
        return -1;

    msps_len = 1 + 4 + 4 + 1 + len;
    msps_buf = (uint8_t*)av_malloc(msps_len);
    idx = 0;
    msps_buf[idx++] = chroma_format_idc;
    msps_buf[idx++] = (width >> 24);
    msps_buf[idx++] = (width >> 16);
    msps_buf[idx++] = (width >> 8);
    msps_buf[idx++] = (width >> 0);
    msps_buf[idx++] = (height >> 24);
    msps_buf[idx++] = (height >> 16);
    msps_buf[idx++] = (height >> 8);
    msps_buf[idx++] = (height >> 0);
    msps_buf[idx++] = bit_depth - 8;
    memcpy(msps_buf + idx, input_data, len);
    idx += len;
    assert(idx == msps_len);
    input_data += len;
    input_data_len -= len;
    
    buf_len = 4 + 2 + msps_len * 2;
    buf = (uint8_t*)av_malloc(buf_len);

    idx = 0;
    /* NAL header */
    buf[idx++] = 0x00;
    buf[idx++] = 0x00;
    buf[idx++] = 0x00;
    buf[idx++] = 0x01; 
    buf[idx++] = (48 << 1); /* application specific NAL unit type */
    buf[idx++] = 1;

    /* add the modified SPS with the correct escape codes */
    i = 0;
    while (i < msps_len) {
        if ((i + 1) < msps_len && msps_buf[i] == 0 && msps_buf[i + 1] == 0) {
            buf[idx++] = 0x00;
            buf[idx++] = 0x00;
            buf[idx++] = 0x03;
            i += 2;
        } else {
            buf[idx++] = msps_buf[i++];
        }
    }
    /* the last byte cannot be 0 */
    if (idx == 0 || buf[idx - 1] == 0x00)
        buf[idx++] = 0x80;
    av_free(msps_buf);
    
    *pbuf_len = idx;
    *pbuf = buf;
    return input_data_len1 - input_data_len;
}

/* return the position of the end of the NAL or -1 if error */
static int find_nal_end(const uint8_t *buf, int buf_len, int has_startcode)
{
    int idx;

    idx = 0;
    if (has_startcode) {
        if (buf_len >= 4 &&
            buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1) {
            idx = 4;
        } else if (buf_len >= 3 &&
                   buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {
            idx = 3;
        } else {
            return -1;
        }
    }
    /* NAL header */
    if (idx + 2 > buf_len)
        return -1;
    /* find the last byte */
    for(;;) {
        if (idx + 2 >= buf_len) {
            idx = buf_len;
            break;
        }
        if (buf[idx] == 0 && buf[idx + 1] == 0 && buf[idx + 2] == 1)
            break;
        if (idx + 3 < buf_len &&
            buf[idx] == 0 && buf[idx + 1] == 0 && buf[idx + 2] == 0 && buf[idx + 3] == 1)
            break;
        idx++;
    }
    return idx;
}

typedef struct {
    uint8_t *buf;
    int size;
    int len;
} DynBuf;

static void dyn_buf_init(DynBuf *s)
{
    s->buf = NULL;
    s->size = 0;
    s->len = 0;
}

static int dyn_buf_resize(DynBuf *s, int size)
{
    int new_size;
    uint8_t *new_buf;

    if (size <= s->size)
        return 0;
    new_size = (s->size * 3) / 2;
    if (new_size < size)
        new_size = size;
    new_buf = (uint8_t*)av_realloc(s->buf, new_size);
    if (!new_buf) 
        return -1;
    s->buf = new_buf;
    s->size = new_size;
    return 0;
}

static int dyn_buf_push(DynBuf *s, const uint8_t *data, int len)
{
    if (dyn_buf_resize(s, s->len + len) < 0)
        return -1;
    memcpy(s->buf + s->len, data, len);
    s->len += len;
    return 0;
}


extern AVCodec ff_hevc_decoder;


static int hevc_decode_init1(DynBuf *pbuf, AVFrame **pframe,
                             AVCodecContext **pc, 
                             const uint8_t *buf, int buf_len,
                             int width, int height, int chroma_format_idc,
                             int bit_depth)
{
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *frame;
    uint8_t *nal_buf;
    int nal_len, ret, ret1;

    ret = build_msps(&nal_buf, &nal_len, buf, buf_len,
                     width, height, chroma_format_idc, bit_depth);
    if (ret < 0)
        return -1;
    ret1 = dyn_buf_push(pbuf, nal_buf, nal_len);
    av_free(nal_buf);
    if (ret1 < 0)
        return -1;
    
    codec = &ff_hevc_decoder;

    c = avcodec_alloc_context3(codec);
    if (!c) 
        return -1;
    frame = av_frame_alloc();
    if (!frame) 
        return -1;
    /* for testing: use the MD5 or CRC in SEI to check the decoded bit
       stream. */
    c->err_recognition |= AV_EF_CRCCHECK; 
    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        av_frame_free(&frame);
        return -1;
    }
    *pc = c;
    *pframe = frame;
    return ret;
}

static int hevc_write_frame(AVCodecContext *avctx,
                            AVFrame *frame,
                            uint8_t *buf, int buf_len)
{
    AVPacket avpkt;
    int len, got_frame;

    av_init_packet(&avpkt);
    avpkt.data = (uint8_t *)buf;
    avpkt.size = buf_len;
    /* avoid using uninitialized data */
    memset(buf + buf_len, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    len = avcodec_decode_video2(avctx, frame, &got_frame, &avpkt);
    if (len < 0 || !got_frame)
        return -1;
    else
        return 0;
}

static int hevc_decode_frame_internal(BPGDecoderContext *s,
                                      DynBuf *abuf, DynBuf *cbuf,
                                      const uint8_t *buf, int buf_len1,
                                      int first_nal)
{
    int nal_len, start, nal_buf_len, ret, nuh_layer_id, buf_len, has_alpha;
    int nut, frame_start_found[2];
    DynBuf *pbuf;
    uint8_t *nal_buf;

    has_alpha = (s->alpha_dec_ctx != NULL);
    buf_len = buf_len1;
    frame_start_found[0] = 0;
    frame_start_found[1] = 0;
    while (buf_len > 0) {
        if (buf_len < (first_nal ? 3 : 0) + 2)
            goto fail;
        if (first_nal)
            start = 0;
        else
            start = 3 + (buf[2] == 0);
        if (buf_len < start + 3)
            goto fail;
        nuh_layer_id = ((buf[start] & 1) << 5) | (buf[start + 1] >> 3);
        nut = (buf[start] >> 1) & 0x3f;
#if 0
        printf("nal: type=%d layer_id=%d fs=%d %d\n", 
               nut, nuh_layer_id, frame_start_found[0], frame_start_found[1]);
#endif
        /* Note: we assume the alpha and color data are correctly
           interleaved */
        if ((nut >= 32 && nut <= 35) || nut == 39 || nut >= 41) {
            if (frame_start_found[0] && frame_start_found[has_alpha])
                break;
        } else if ((nut <= 9 || (nut >= 16 && nut <= 21)) &&  
                   start + 2 < buf_len && (buf[start + 2] & 0x80)) {
            /* first slice segment */
            if (frame_start_found[0] && frame_start_found[has_alpha])
                break;
            if (has_alpha && nuh_layer_id == 1)
                frame_start_found[1] = 1;
            else
                frame_start_found[0] = 1;
        }
        
        nal_len = find_nal_end(buf, buf_len, !first_nal);
        if (nal_len < 0)
            goto fail;
        nal_buf_len = nal_len - start + 3;
        if (has_alpha && nuh_layer_id == 1)
            pbuf = abuf;
        else
            pbuf = cbuf;
        if (dyn_buf_resize(pbuf, pbuf->len + nal_buf_len) < 0)
            goto fail;
        nal_buf = pbuf->buf + pbuf->len;
        nal_buf[0] = 0x00;
        nal_buf[1] = 0x00;
        nal_buf[2] = 0x01;
        memcpy(nal_buf + 3, buf + start, nal_len - start);
        if (has_alpha && nuh_layer_id == 1)
            nal_buf[4] &= 0x7;
        pbuf->len += nal_buf_len;
        buf += nal_len;
        buf_len -= nal_len;
        first_nal = 0;
    }
    
    if (s->alpha_dec_ctx) {
        if (dyn_buf_resize(abuf, abuf->len + FF_INPUT_BUFFER_PADDING_SIZE) < 0)
            goto fail;
        ret = hevc_write_frame(s->alpha_dec_ctx, s->alpha_frame, abuf->buf, abuf->len);
        if (ret < 0)
            goto fail;
    }

    if (dyn_buf_resize(cbuf, cbuf->len + FF_INPUT_BUFFER_PADDING_SIZE) < 0)
        goto fail;
    ret = hevc_write_frame(s->dec_ctx, s->frame, cbuf->buf, cbuf->len);
    if (ret < 0)
        goto fail;
    ret = buf_len1 - buf_len;
 done:
    return ret;
 fail:
    ret = -1;
    goto done;
}

/* decode the first frame */
static int hevc_decode_start(BPGDecoderContext *s,
                             const uint8_t *buf, int buf_len1,
                             int width, int height, int chroma_format_idc,
                             int bit_depth, int has_alpha)
{
    int ret, buf_len;
    DynBuf abuf_s, *abuf = &abuf_s;
    DynBuf cbuf_s, *cbuf = &cbuf_s;

    dyn_buf_init(abuf);
    dyn_buf_init(cbuf);

    buf_len = buf_len1;
    if (has_alpha) {
        ret = hevc_decode_init1(abuf, &s->alpha_frame, &s->alpha_dec_ctx,
                                buf, buf_len, width, height, 0, bit_depth);
        if (ret < 0)
            goto fail;
        buf += ret;
        buf_len -= ret;
    }
    
    ret = hevc_decode_init1(cbuf, &s->frame, &s->dec_ctx,
                            buf, buf_len, width, height, chroma_format_idc, 
                            bit_depth);
    if (ret < 0)
        goto fail;
    buf += ret;
    buf_len -= ret;
    
    ret = hevc_decode_frame_internal(s, abuf, cbuf, buf, buf_len, 1);
    av_free(abuf->buf);
    av_free(cbuf->buf);
    if (ret < 0)
        goto fail;
    buf_len -= ret;
    return buf_len1 - buf_len;
 fail:
    return -1;
}

#ifdef USE_PRED
static int hevc_decode_frame(BPGDecoderContext *s,
                             const uint8_t *buf, int buf_len)
{
    int ret;
    DynBuf abuf_s, *abuf = &abuf_s;
    DynBuf cbuf_s, *cbuf = &cbuf_s;

    dyn_buf_init(abuf);
    dyn_buf_init(cbuf);
    ret = hevc_decode_frame_internal(s, abuf, cbuf, buf, buf_len, 0);
    av_free(abuf->buf);
    av_free(cbuf->buf);
    return ret;
}
#endif

static void hevc_decode_end(BPGDecoderContext *s)
{
    if (s->alpha_dec_ctx) {
        avcodec_close(s->alpha_dec_ctx);
        av_free(s->alpha_dec_ctx);
        s->alpha_dec_ctx = NULL;
    }
    if (s->dec_ctx) {
        avcodec_close(s->dec_ctx);
        av_free(s->dec_ctx);
        s->dec_ctx = NULL;
    }
}

uint8_t *bpg_decoder_get_data(BPGDecoderContext *img, int *pline_size, int plane)
{
    int c_count;
    if (img->format == BPG_FORMAT_GRAY) 
        c_count = 1;
    else
        c_count = 3;
    if (plane < c_count) {
        *pline_size = img->frame->linesize[plane];
        return img->frame->data[plane];
    } else if (img->has_alpha && plane == c_count) {
        *pline_size = img->alpha_frame->linesize[0];
        return img->alpha_frame->data[0];
    } else {
        *pline_size = 0;
        return NULL;
    }
}

int bpg_decoder_get_info(BPGDecoderContext *img, BPGImageInfo *p)
{
    if (!img->frame)
        return -1;
    p->width = img->w;
    p->height = img->h;
    p->format = img->format;
    p->has_alpha = img->has_alpha && !img->has_w_plane;
    p->premultiplied_alpha = img->premultiplied_alpha;
    p->has_w_plane = img->has_w_plane;
    p->limited_range = img->limited_range;
    p->color_space = img->color_space;
    p->bit_depth = img->bit_depth;
    p->has_animation = img->has_animation;
    p->loop_count = img->loop_count;
    return 0;
}

static inline int clamp_pix(int a, int pixel_max)
{
    if (a < 0)
        return 0;
    else if (a > pixel_max)
        return pixel_max;
    else
        return a;
}

static inline int clamp8(int a)
{
    if (a < 0)
        return 0;
    else if (a > 255)
        return 255;
    else
        return a;
}

/* 8 tap Lanczos interpolator (phase=0, symmetric) */
#define IP0C0 40
#define IP0C1 (-11)
#define IP0C2 4
#define IP0C3 (-1)

/* 7 tap Lanczos interpolator (phase=0.5) */
#define IP1C0 (-1)
#define IP1C1 4
#define IP1C2 (-10)
#define IP1C3 57
#define IP1C4 18
#define IP1C5 (-6)
#define IP1C6 2

/* interpolate by a factor of two assuming chroma is aligned with the
   luma samples. */
static void interp2p0_simple(PIXEL *dst, const PIXEL *src, int n, int bit_depth)
{
    int pixel_max;

    pixel_max = (1 << bit_depth) - 1;
    while (n >= 2) {
        dst[0] = src[0];
        dst[1] = clamp_pix(((src[-3] + src[4]) * IP0C3 + 
                            (src[-2] + src[3]) * IP0C2 + 
                            (src[-1] + src[2]) * IP0C1 + 
                            (src[0] + src[1]) * IP0C0 + 32) >> 6, pixel_max);
        dst += 2;
        src++;
        n -= 2;
    }
    if (n) {
        dst[0] = src[0];
    }
}

static void interp2p0_simple16(PIXEL *dst, const int16_t *src, int n, int bit_depth)
{
    int shift1, offset1, shift0, offset0, pixel_max;

    pixel_max = (1 << bit_depth) - 1;
    shift0 = 14 - bit_depth;
    offset0 = (1 << shift0) >> 1;
    shift1 = 20 - bit_depth;
    offset1 = 1 << (shift1 - 1);

    while (n >= 2) {
        dst[0] = clamp_pix((src[0] + offset0) >> shift0, pixel_max);
        dst[1] = clamp_pix(((src[-3] + src[4]) * IP0C3 + 
                            (src[-2] + src[3]) * IP0C2 + 
                            (src[-1] + src[2]) * IP0C1 + 
                            (src[0] + src[1]) * IP0C0 + offset1) >> shift1,
                           pixel_max);
        dst += 2;
        src++;
        n -= 2;
    }
    if (n) {
        dst[0] = clamp_pix((src[0] + offset0) >> shift0, pixel_max);
    }
}

/* interpolate by a factor of two assuming chroma is between the luma
   samples. */
static void interp2p1_simple(PIXEL *dst, const PIXEL *src, int n, int bit_depth)
{
    int pixel_max, a0, a1, a2, a3, a4, a5, a6;

    pixel_max = (1 << bit_depth) - 1;

    a1 = src[-3];
    a2 = src[-2];
    a3 = src[-1];
    a4 = src[0];
    a5 = src[1];
    a6 = src[2];

    while (n >= 2) {
        a0 = a1;
        a1 = a2;
        a2 = a3;
        a3 = a4;
        a4 = a5;
        a5 = a6;
        a6 = src[3];
        dst[0] = clamp_pix((a0 * IP1C6 + a1 * IP1C5 + a2 * IP1C4 + a3 * IP1C3 + 
                            a4 * IP1C2 + a5 * IP1C1 + a6 * IP1C0 + 32) >> 6, 
                           pixel_max);
        dst[1] = clamp_pix((a0 * IP1C0 + a1 * IP1C1 + a2 * IP1C2 + a3 * IP1C3 +
                            a4 * IP1C4 + a5 * IP1C5 + a6 * IP1C6 + 32) >> 6, 
                           pixel_max);
        dst += 2;
        src++;
        n -= 2;
    }
    if (n) {
        a0 = a1;
        a1 = a2;
        a2 = a3;
        a3 = a4;
        a4 = a5;
        a5 = a6;
        a6 = src[3];
        dst[0] = clamp_pix((a0 * IP1C6 + a1 * IP1C5 + a2 * IP1C4 + a3 * IP1C3 + 
                            a4 * IP1C2 + a5 * IP1C1 + a6 * IP1C0 + 32) >> 6, 
                           pixel_max);
    }
}

static void interp2p1_simple16(PIXEL *dst, const int16_t *src, int n, 
                               int bit_depth)
{
    int shift, offset, pixel_max, a0, a1, a2, a3, a4, a5, a6;

    pixel_max = (1 << bit_depth) - 1;
    shift = 20 - bit_depth;
    offset = 1 << (shift - 1);

    a1 = src[-3];
    a2 = src[-2];
    a3 = src[-1];
    a4 = src[0];
    a5 = src[1];
    a6 = src[2];

    while (n >= 2) {
        a0 = a1;
        a1 = a2;
        a2 = a3;
        a3 = a4;
        a4 = a5;
        a5 = a6;
        a6 = src[3];
        dst[0] = clamp_pix((a0 * IP1C6 + a1 * IP1C5 + a2 * IP1C4 + a3 * IP1C3 +
                            a4 * IP1C2 + a5 * IP1C1 + a6 * IP1C0 + offset) >> shift,
                           pixel_max);
        dst[1] = clamp_pix((a0 * IP1C0 + a1 * IP1C1 + a2 * IP1C2 + a3 * IP1C3 +
                            a4 * IP1C4 + a5 * IP1C5 + a6 * IP1C6 + offset) >> shift,
                           pixel_max);
        dst += 2;
        src++;
        n -= 2;
    }
    if (n) {
        a0 = a1;
        a1 = a2;
        a2 = a3;
        a3 = a4;
        a4 = a5;
        a5 = a6;
        a6 = src[3];
        dst[0] = clamp_pix((a0 * IP1C6 + a1 * IP1C5 + a2 * IP1C4 + a3 * IP1C3 +
                            a4 * IP1C2 + a5 * IP1C1 + a6 * IP1C0 + offset) >> shift, 
                           pixel_max);
    }
}

/* tmp_buf is a temporary buffer of length (n2 + 2 * ITAPS2 - 1) */
static void interp2_h(PIXEL *dst, const PIXEL *src, int n, int bit_depth,
                      int phase, PIXEL *tmp_buf)
{
    PIXEL *src1 = tmp_buf, v;
    int i, n2;

    /* add extra pixels and do the interpolation (XXX: could go faster) */
    n2 = (n + 1) / 2;
    memcpy(src1 + ITAPS2 - 1, src, n2 * sizeof(PIXEL));

    v = src[0];
    for(i = 0; i < ITAPS2 - 1; i++)
        src1[i] = v;

    v = src[n2 - 1];
    for(i = 0; i < ITAPS2; i++)
        src1[ITAPS2 - 1 + n2 + i] = v;
    if (phase == 0)
        interp2p0_simple(dst, src1 + ITAPS2 - 1, n, bit_depth);
    else
        interp2p1_simple(dst, src1 + ITAPS2 - 1, n, bit_depth);
}

/* y_pos is the position of the sample '0' in the 'src' circular
   buffer. tmp_buf is a temporary buffer of length (n2 + 2 * ITAPS2 - 1) */
static void interp2_vh(PIXEL *dst, PIXEL **src, int n, int y_pos,
                       int16_t *tmp_buf, int bit_depth, int frac_pos,
                       int c_h_phase)
{
    const PIXEL *src0, *src1, *src2, *src3, *src4, *src5, *src6;
    int i, n2, shift, rnd;
    int16_t v;

    src0 = src[(y_pos - 3) & 7];
    src1 = src[(y_pos - 2) & 7];
    src2 = src[(y_pos - 1) & 7];
    src3 = src[(y_pos + 0) & 7];
    src4 = src[(y_pos + 1) & 7];
    src5 = src[(y_pos + 2) & 7];
    src6 = src[(y_pos + 3) & 7];

    /* vertical interpolation first */
    shift = bit_depth - 8;
    rnd = (1 << shift) >> 1;
    n2 = (n + 1) / 2;
    if (frac_pos == 0) {
        for(i = 0; i < n2; i++) {
            tmp_buf[ITAPS2 - 1 + i] = 
                (src0[i] * IP1C6 + src1[i] * IP1C5 + 
                 src2[i] * IP1C4 + src3[i] * IP1C3 + 
                 src4[i] * IP1C2 + src5[i] * IP1C1 + 
                 src6[i] * IP1C0 + rnd) >> shift;
        }
    } else {
        for(i = 0; i < n2; i++) {
            tmp_buf[ITAPS2 - 1 + i] = 
                (src0[i] * IP1C0 + src1[i] * IP1C1 + 
                 src2[i] * IP1C2 + src3[i] * IP1C3 + 
                 src4[i] * IP1C4 + src5[i] * IP1C5 + 
                 src6[i] * IP1C6 + rnd) >> shift;
        }
    }

    /* then horizontal interpolation */
    v = tmp_buf[ITAPS2 - 1];
    for(i = 0; i < ITAPS2 - 1; i++)
        tmp_buf[i] = v;
    v = tmp_buf[ITAPS2 - 1 + n2 - 1];
    for(i = 0; i < ITAPS2; i++)
        tmp_buf[ITAPS2 - 1 + n2 + i] = v;
    if (c_h_phase == 0)
        interp2p0_simple16(dst, tmp_buf + ITAPS2 - 1, n, bit_depth);
    else
        interp2p1_simple16(dst, tmp_buf + ITAPS2 - 1, n, bit_depth);
}

static void ycc_to_rgb24(ColorConvertState *s, uint8_t *dst, const PIXEL *y_ptr,
                         const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                         int n, int incr)
{
    uint8_t *q = dst;
    int y_val, cb_val, cr_val, x;
    int c_r_cr, c_g_cb, c_g_cr, c_b_cb, rnd, shift, center, c_one;

    c_r_cr = s->c_r_cr;
    c_g_cb = s->c_g_cb;
    c_g_cr = s->c_g_cr;
    c_b_cb = s->c_b_cb;
    c_one = s->y_one;
    rnd = s->y_offset;
    shift = s->c_shift;
    center = s->c_center;
    for(x = 0; x < n; x++) {
        y_val = y_ptr[x] * c_one;
        cb_val = cb_ptr[x] - center;
        cr_val = cr_ptr[x] - center;
        q[0] = clamp8((y_val + c_r_cr * cr_val + rnd) >> shift);
        q[1] = clamp8((y_val - c_g_cb * cb_val - c_g_cr * cr_val + rnd) >> shift);
        q[2] = clamp8((y_val + c_b_cb * cb_val + rnd) >> shift);
        q += incr;
    }
}

static void ycgco_to_rgb24(ColorConvertState *s, 
                           uint8_t *dst, const PIXEL *y_ptr,
                           const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                           int n, int incr)
{
    uint8_t *q = dst;
    int y_val, cb_val, cr_val, x;
    int rnd, shift, center, c_one;

    c_one = s->y_one;
    rnd = s->y_offset;
    shift = s->c_shift;
    center = s->c_center;
    for(x = 0; x < n; x++) {
        y_val = y_ptr[x];
        cb_val = cb_ptr[x] - center;
        cr_val = cr_ptr[x] - center;
        q[0] = clamp8(((y_val - cb_val + cr_val) * c_one + rnd) >> shift);
        q[1] = clamp8(((y_val + cb_val) * c_one + rnd) >> shift);
        q[2] = clamp8(((y_val - cb_val - cr_val) * c_one + rnd) >> shift);
        q += incr;
    }
}

/* c = c * alpha */
static void alpha_combine8(ColorConvertState *s, 
                           uint8_t *dst, const PIXEL *a_ptr, int n, int incr)
{
    uint8_t *q = dst;
    int x, a_val, shift, rnd;

    shift = s->bit_depth;
    rnd = 1 << (shift - 1);
    for(x = 0; x < n; x++) {
        a_val = a_ptr[x];
        /* XXX: not accurate enough */
        q[0] = (q[0] * a_val + rnd) >> shift;
        q[1] = (q[1] * a_val + rnd) >> shift;
        q[2] = (q[2] * a_val + rnd) >> shift;
        q += incr;
    }
}

static uint32_t divide8_table[256];

#define DIV8_BITS 16

static void alpha_divide8_init(void)
{
    int i;
    for(i = 1; i < 256; i++) {
        /* Note: the 128 is added to have 100% correct results for all
           the values */
        divide8_table[i] = ((255 << DIV8_BITS) + (i / 2) + 128) / i;
    }
}

static inline unsigned int comp_divide8(unsigned int val, unsigned int alpha,
                                        unsigned int alpha_inv)
{
    if (val >= alpha)
        return 255;
    return (val * alpha_inv + (1 << (DIV8_BITS - 1))) >> DIV8_BITS;
}

/* c = c / alpha */
static void alpha_divide8(uint8_t *dst, int n)
{
    static int inited;
    uint8_t *q = dst;
    int x;
    unsigned int a_val, a_inv;

    if (!inited) {
        inited = 1;
        alpha_divide8_init();
    }

    for(x = 0; x < n; x++) {
        a_val = q[3];
        if (a_val == 0) {
            q[0] = 255;
            q[1] = 255;
            q[2] = 255;
        } else {
            a_inv = divide8_table[a_val];
            q[0] = comp_divide8(q[0], a_val, a_inv);
            q[1] = comp_divide8(q[1], a_val, a_inv);
            q[2] = comp_divide8(q[2], a_val, a_inv);
        }
        q += 4;
    }
}

static void gray_to_rgb24(ColorConvertState *s, 
                          uint8_t *dst, const PIXEL *y_ptr,
                          const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                          int n, int incr)
{
    uint8_t *q = dst;
    int x, y_val, c, rnd, shift;

    if (s->bit_depth == 8 && !s->limited_range) {
        for(x = 0; x < n; x++) {
            y_val = y_ptr[x];
            q[0] = y_val;
            q[1] = y_val;
            q[2] = y_val;
            q += incr;
        }
    } else {
        c = s->y_one;
        rnd = s->y_offset;
        shift = s->c_shift;
        for(x = 0; x < n; x++) {
            y_val = clamp8((y_ptr[x] * c + rnd) >> shift);
            q[0] = y_val;
            q[1] = y_val;
            q[2] = y_val;
            q += incr;
        }
    }
}

static void rgb_to_rgb24(ColorConvertState *s, uint8_t *dst, const PIXEL *y_ptr,
                         const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                         int n, int incr)
{
    uint8_t *q = dst;
    int x, c, rnd, shift;

    if (s->bit_depth == 8 && !s->limited_range) {
        for(x = 0; x < n; x++) {
            q[0] = cr_ptr[x];
            q[1] = y_ptr[x];
            q[2] = cb_ptr[x];
            q += incr;
        }
    } else {
        c = s->y_one;
        rnd = s->y_offset;
        shift = s->c_shift;
        for(x = 0; x < n; x++) {
            q[0] = clamp8((cr_ptr[x] * c + rnd) >> shift);
            q[1] = clamp8((y_ptr[x] * c + rnd) >> shift);
            q[2] = clamp8((cb_ptr[x] * c + rnd) >> shift);
            q += incr;
        }
    }
}

static void put_dummy_gray8(uint8_t *dst, int n, int incr)
{
    int x;
    for(x = 0; x < n; x++) {
        dst[0] = 0xff;
        dst += incr;
    }
}

static void gray_to_gray8(ColorConvertState *s, 
                          uint8_t *dst, const PIXEL *y_ptr,
                          int n, int incr)
{
    uint8_t *q = dst;
    int x, y_val, c, rnd, shift;

    if (s->bit_depth == 8) {
        for(x = 0; x < n; x++) {
            y_val = y_ptr[x];
            q[0] = y_val;
            q += incr;
        }
    } else {
        c = s->c_one;
        rnd = s->c_rnd;
        shift = s->c_shift;
        for(x = 0; x < n; x++) {
            y_val = (y_ptr[x] * c + rnd) >> shift;
            q[0] = y_val;
            q += incr;
        }
    }
}

static ColorConvertFunc *cs_to_rgb24[BPG_CS_COUNT] = {
    ycc_to_rgb24,
    rgb_to_rgb24,
    ycgco_to_rgb24,
    ycc_to_rgb24,
    ycc_to_rgb24,
};

#ifdef USE_RGB48

/* 16 bit output */

static inline int clamp16(int a)
{
    if (a < 0)
        return 0;
    else if (a > 65535)
        return 65535;
    else
        return a;
}

static void ycc_to_rgb48(ColorConvertState *s, uint8_t *dst, const PIXEL *y_ptr,
                         const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                         int n, int incr)
{
    uint16_t *q = (uint16_t *)dst;
    int y_val, cb_val, cr_val, x;
    int c_r_cr, c_g_cb, c_g_cr, c_b_cb, rnd, shift, center, c_one;

    c_r_cr = s->c_r_cr;
    c_g_cb = s->c_g_cb;
    c_g_cr = s->c_g_cr;
    c_b_cb = s->c_b_cb;
    c_one = s->y_one;
    rnd = s->y_offset;
    shift = s->c_shift;
    center = s->c_center;
    for(x = 0; x < n; x++) {
        y_val = y_ptr[x] * c_one;
        cb_val = cb_ptr[x] - center;
        cr_val = cr_ptr[x] - center;
        q[0] = clamp16((y_val + c_r_cr * cr_val + rnd) >> shift);
        q[1] = clamp16((y_val - c_g_cb * cb_val - c_g_cr * cr_val + rnd) >> shift);
        q[2] = clamp16((y_val + c_b_cb * cb_val + rnd) >> shift);
        q += incr;
    }
}

static void ycgco_to_rgb48(ColorConvertState *s, 
                           uint8_t *dst, const PIXEL *y_ptr,
                           const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                           int n, int incr)
{
    uint16_t *q = (uint16_t *)dst;
    int y_val, cb_val, cr_val, x;
    int rnd, shift, center, c_one;

    c_one = s->y_one;
    rnd = s->y_offset;
    shift = s->c_shift;
    center = s->c_center;
    for(x = 0; x < n; x++) {
        y_val = y_ptr[x];
        cb_val = cb_ptr[x] - center;
        cr_val = cr_ptr[x] - center;
        q[0] = clamp16(((y_val - cb_val + cr_val) * c_one + rnd) >> shift);
        q[1] = clamp16(((y_val + cb_val) * c_one + rnd) >> shift);
        q[2] = clamp16(((y_val - cb_val - cr_val) * c_one + rnd) >> shift);
        q += incr;
    }
}

static void gray_to_rgb48(ColorConvertState *s, 
                          uint8_t *dst, const PIXEL *y_ptr,
                          const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                          int n, int incr)
{
    uint16_t *q = (uint16_t *)dst;
    int x, y_val, c, rnd, shift;

    c = s->y_one;
    rnd = s->y_offset;
    shift = s->c_shift;
    for(x = 0; x < n; x++) {
        y_val = clamp16((y_ptr[x] * c + rnd) >> shift);
        q[0] = y_val;
        q[1] = y_val;
        q[2] = y_val;
        q += incr;
    }
}

static void gray_to_gray16(ColorConvertState *s, 
                           uint16_t *dst, const PIXEL *y_ptr,
                           int n, int incr)
{
    uint16_t *q = dst;
    int x, y_val, c, rnd, shift;

    c = s->c_one;
    rnd = s->c_rnd;
    shift = s->c_shift;
    for(x = 0; x < n; x++) {
        y_val = (y_ptr[x] * c + rnd) >> shift;
        q[0] = y_val;
        q += incr;
    }
}

static void luma_to_gray16(ColorConvertState *s, 
                           uint16_t *dst, const PIXEL *y_ptr,
                           int n, int incr)
{
    uint16_t *q = dst;
    int x, y_val, c, rnd, shift;

    c = s->y_one;
    rnd = s->y_offset;
    shift = s->c_shift;
    for(x = 0; x < n; x++) {
        y_val = clamp16((y_ptr[x] * c + rnd) >> shift);
        q[0] = y_val;
        q += incr;
    }
}

static void rgb_to_rgb48(ColorConvertState *s, 
                         uint8_t *dst, const PIXEL *y_ptr,
                         const PIXEL *cb_ptr, const PIXEL *cr_ptr,
                         int n, int incr)
{
    luma_to_gray16(s, (uint16_t *)dst + 1, y_ptr, n, incr);
    luma_to_gray16(s, (uint16_t *)dst + 2, cb_ptr, n, incr);
    luma_to_gray16(s, (uint16_t *)dst + 0, cr_ptr, n, incr);
}

static void put_dummy_gray16(uint16_t *dst, int n, int incr)
{
    int x;
    for(x = 0; x < n; x++) {
        dst[0] = 0xffff;
        dst += incr;
    }
}

/* c = c * alpha */
static void alpha_combine16(ColorConvertState *s, 
                            uint16_t *dst, const PIXEL *a_ptr, int n, int incr)
{
    uint16_t *q = dst;
    int x, a_val, shift, rnd;

    shift = s->bit_depth;
    rnd = 1 << (shift - 1);
    for(x = 0; x < n; x++) {
        a_val = a_ptr[x];
        /* XXX: not accurate enough */
        q[0] = (q[0] * a_val + rnd) >> shift;
        q[1] = (q[1] * a_val + rnd) >> shift;
        q[2] = (q[2] * a_val + rnd) >> shift;
        q += incr;
    }
}

#define DIV16_BITS 15

static unsigned int comp_divide16(unsigned int val, unsigned int alpha,
                                  unsigned int alpha_inv)
{
    if (val >= alpha)
        return 65535;
    return (val * alpha_inv + (1 << (DIV16_BITS - 1))) >> DIV16_BITS;
}

/* c = c / alpha */
static void alpha_divide16(uint16_t *dst, int n)
{
    uint16_t *q = dst;
    int x;
    unsigned int a_val, a_inv;

    for(x = 0; x < n; x++) {
        a_val = q[3];
        if (a_val == 0) {
            q[0] = 65535;
            q[1] = 65535;
            q[2] = 65535;
        } else {
            a_inv = ((65535 << DIV16_BITS) + (a_val / 2)) / a_val;
            q[0] = comp_divide16(q[0], a_val, a_inv);
            q[1] = comp_divide16(q[1], a_val, a_inv);
            q[2] = comp_divide16(q[2], a_val, a_inv);
        }
        q += 4;
    }
}

static void gray_one_minus8(uint8_t *dst, int n, int incr)
{
    int x;
    for(x = 0; x < n; x++) {
        dst[0] = 255 - dst[0];
        dst += incr;
    }
}

static void gray_one_minus16(uint16_t *dst, int n, int incr)
{
    int x;
    for(x = 0; x < n; x++) {
        dst[0] = 65535 - dst[0];
        dst += incr;
    }
}

static ColorConvertFunc *cs_to_rgb48[BPG_CS_COUNT] = {
    ycc_to_rgb48,
    rgb_to_rgb48,
    ycgco_to_rgb48,
    ycc_to_rgb48,
    ycc_to_rgb48,
};
#endif

static void convert_init(ColorConvertState *s, 
                         int in_bit_depth, int out_bit_depth,
                         BPGColorSpaceEnum color_space,
                         int limited_range)
{
    int c_shift, in_pixel_max, out_pixel_max;
    double mult, k_r, k_b, mult_y, mult_c;

    c_shift = 30 - out_bit_depth;
    in_pixel_max = (1 << in_bit_depth) - 1;
    out_pixel_max = (1 << out_bit_depth) - 1;
    mult = (double)out_pixel_max * (1 << c_shift) / (double)in_pixel_max;
    if (limited_range) {
        mult_y = (double)out_pixel_max * (1 << c_shift) / 
            (double)(219 << (in_bit_depth - 8));
        mult_c = (double)out_pixel_max * (1 << c_shift) / 
            (double)(224 << (in_bit_depth - 8));
    } else {
        mult_y = mult;
        mult_c = mult;
    }
    switch(color_space) {
    case BPG_CS_YCbCr:
        k_r = 0.299;
        k_b = 0.114;
        goto convert_ycc;
    case BPG_CS_YCbCr_BT709:
        k_r = 0.2126; 
        k_b = 0.0722;
        goto convert_ycc;
    case BPG_CS_YCbCr_BT2020:
        k_r = 0.2627;
        k_b = 0.0593;
    convert_ycc:
        s->c_r_cr = lrint(2*(1-k_r) * mult_c);
        s->c_g_cb = lrint(2*k_b*(1-k_b)/(1-k_b-k_r) * mult_c);
        s->c_g_cr = lrint(2*k_r*(1-k_r)/(1-k_b-k_r) * mult_c);
        s->c_b_cb = lrint(2*(1-k_b) * mult_c);
        break;
    default:
        break;
    }
    s->c_one = lrint(mult);
    s->c_shift = c_shift;
    s->c_rnd = (1 << (c_shift - 1));
    s->c_center = 1 << (in_bit_depth - 1);
    if (limited_range) {
        s->y_one = lrint(mult_y);
        s->y_offset = -(16 << (in_bit_depth - 8)) * s->y_one + s->c_rnd;
    } else {
        s->y_one = s->c_one;
        s->y_offset = s->c_rnd;
    }
    s->bit_depth = in_bit_depth;
    s->limited_range = limited_range;
}

static int bpg_decoder_output_init(BPGDecoderContext *s,
                                   BPGDecoderOutputFormat out_fmt)
{
    int i;

#ifdef USE_RGB48
    if ((unsigned)out_fmt > BPG_OUTPUT_FORMAT_CMYK64)
        return -1;
#else
    if ((unsigned)out_fmt > BPG_OUTPUT_FORMAT_RGBA32)
        return -1;
#endif
    s->is_rgba = (out_fmt == BPG_OUTPUT_FORMAT_RGBA32 ||
                    out_fmt == BPG_OUTPUT_FORMAT_RGBA64);
    s->is_16bpp = (out_fmt == BPG_OUTPUT_FORMAT_RGB48 ||
                   out_fmt == BPG_OUTPUT_FORMAT_RGBA64 ||
                   out_fmt == BPG_OUTPUT_FORMAT_CMYK64);
    s->is_cmyk = (out_fmt == BPG_OUTPUT_FORMAT_CMYK32 ||
                  out_fmt == BPG_OUTPUT_FORMAT_CMYK64);
    
    if (s->format == BPG_FORMAT_420 || s->format == BPG_FORMAT_422) {
        s->w2 = (s->w + 1) / 2;
        s->h2 = (s->h + 1) / 2;
        s->cb_buf2 = (PIXEL*)av_malloc(s->w * sizeof(PIXEL));
        s->cr_buf2 = (PIXEL*)av_malloc(s->w * sizeof(PIXEL));
        /* Note: too large if 422 and sizeof(PIXEL) = 1 */
        s->c_buf4 = (int16_t*)av_malloc((s->w2 + 2 * ITAPS2 - 1) * sizeof(int16_t));

        if (s->format == BPG_FORMAT_420) {
            for(i = 0; i < ITAPS; i++) {
                s->cb_buf3[i] = (PIXEL*)av_malloc(s->w2 * sizeof(PIXEL));
                s->cr_buf3[i] = (PIXEL*)av_malloc(s->w2 * sizeof(PIXEL));
            }
        }
    }
    convert_init(&s->cvt, s->bit_depth, s->is_16bpp ? 16 : 8,
                 s->color_space, s->limited_range);

    if (s->format == BPG_FORMAT_GRAY) {
#ifdef USE_RGB48
        if (s->is_16bpp) {
            s->cvt_func = gray_to_rgb48;
        } else 
#endif
        {
            s->cvt_func = gray_to_rgb24;
        }
    } else {
#ifdef USE_RGB48
        if (s->is_16bpp) {
            s->cvt_func = cs_to_rgb48[s->color_space];
        } else
#endif
        {
            s->cvt_func = cs_to_rgb24[s->color_space];
        }
    }
    return 0;
}

static void bpg_decoder_output_end(BPGDecoderContext *s)
{
    int i;

    av_free(s->cb_buf2);
    av_free(s->cr_buf2);
    for(i = 0; i < ITAPS; i++) {
        av_free(s->cb_buf3[i]);
        av_free(s->cr_buf3[i]);
    }
    av_free(s->c_buf4);
}

int bpg_decoder_start(BPGDecoderContext *s, BPGDecoderOutputFormat out_fmt)
{
    int ret, c_idx;

    if (!s->frame)
        return -1;
    
    if (!s->output_inited) {
        /* first frame is already decoded */
        ret = bpg_decoder_output_init(s, out_fmt);
        if (ret)
            return ret;
        s->output_inited = 1;
        s->out_fmt = out_fmt;
    } else {
#ifdef USE_PRED
        if (s->has_animation && s->decode_animation) {
            if (out_fmt != s->out_fmt)
                return -1;
            if (s->input_buf_pos >= s->input_buf_len) {
                return -1;
            } else {
                ret = hevc_decode_frame(s, s->input_buf + s->input_buf_pos, 
                                        s->input_buf_len - s->input_buf_pos);
                if (ret < 0)
                    return -1;
                s->input_buf_pos += ret;
            }
        } else 
#endif
        {
            return -1;
        }
    }
    s->y_buf = bpg_decoder_get_data(s, &s->y_linesize, 0);
    if (s->format != BPG_FORMAT_GRAY) {
        s->cb_buf = bpg_decoder_get_data(s, &s->cb_linesize, 1);
        s->cr_buf = bpg_decoder_get_data(s, &s->cr_linesize, 2);
        c_idx = 3;
    } else {
        c_idx = 1;
    }
    if (s->has_alpha)
        s->a_buf = bpg_decoder_get_data(s, &s->a_linesize, c_idx);
    else
        s->a_buf = NULL;
    s->y = 0;
    return 0;
}

void bpg_decoder_get_frame_duration(BPGDecoderContext *s, int *pnum, int *pden)
{
#ifdef USE_PRED
    if (s->frame && s->has_animation) {
        *pnum = s->frame_delay_num * (s->frame->pts);
        *pden = s->frame_delay_den;
    } else 
#endif
    {
        *pnum = 0;
        *pden = 1;
    }
}

int bpg_decoder_get_line(BPGDecoderContext *s, void *rgb_line1)
{
    uint8_t *rgb_line = (uint8_t*)rgb_line1;
    int w, y, pos, y2, y1, incr, y_frac;
    PIXEL *y_ptr, *cb_ptr, *cr_ptr, *a_ptr;

    y = s->y;
    if ((unsigned)y >= s->h) 
        return -1;
    w = s->w;
    
    y_ptr = (PIXEL *)(s->y_buf + y * s->y_linesize);
    incr = 3 + (s->is_rgba || s->is_cmyk);
    switch(s->format) {
    case BPG_FORMAT_GRAY:
        s->cvt_func(&s->cvt, rgb_line, y_ptr, NULL, NULL, w, incr);
        break;
    case BPG_FORMAT_420:
        if (y == 0) {
            int i;
            /* init the vertical interpolation buffer */
            for(i = 0; i < ITAPS; i++) {
                y1 = i;
                if (y1 > ITAPS2)
                    y1 -= ITAPS;
                if (y1 < 0)
                    y1 = 0;
                else if (y1 >= s->h2)
                    y1 = s->h2 - 1;
                cb_ptr = (PIXEL *)(s->cb_buf + y1 * s->cb_linesize);
                cr_ptr = (PIXEL *)(s->cr_buf + y1 * s->cr_linesize);
                memcpy(s->cb_buf3[i], cb_ptr, s->w2 * sizeof(PIXEL));
                memcpy(s->cr_buf3[i], cr_ptr, s->w2 * sizeof(PIXEL));
            }
        }
        y2 = y >> 1;
        pos = y2 % ITAPS;
        y_frac = y & 1;
        interp2_vh(s->cb_buf2, s->cb_buf3, w, pos, s->c_buf4,
                   s->bit_depth, y_frac, s->c_h_phase);
        interp2_vh(s->cr_buf2, s->cr_buf3, w, pos, s->c_buf4,
                   s->bit_depth, y_frac, s->c_h_phase);
        if (y_frac) {
            /* add a new line in the circular buffer */
            pos = (pos + ITAPS2 + 1) % ITAPS;
            y1 = y2 + ITAPS2 + 1;
            if (y1 >= s->h2)
                y1 = s->h2 - 1;
            cb_ptr = (PIXEL *)(s->cb_buf + y1 * s->cb_linesize);
            cr_ptr = (PIXEL *)(s->cr_buf + y1 * s->cr_linesize);
            memcpy(s->cb_buf3[pos], cb_ptr, s->w2 * sizeof(PIXEL));
            memcpy(s->cr_buf3[pos], cr_ptr, s->w2 * sizeof(PIXEL));
        }
        s->cvt_func(&s->cvt, rgb_line, y_ptr, s->cb_buf2, s->cr_buf2, w, incr);
        break;
    case BPG_FORMAT_422:
        cb_ptr = (PIXEL *)(s->cb_buf + y * s->cb_linesize);
        cr_ptr = (PIXEL *)(s->cr_buf + y * s->cr_linesize);
        interp2_h(s->cb_buf2, cb_ptr, w, s->bit_depth, s->c_h_phase, 
                  (PIXEL *)s->c_buf4);
        interp2_h(s->cr_buf2, cr_ptr, w, s->bit_depth, s->c_h_phase,
                  (PIXEL *)s->c_buf4);
        s->cvt_func(&s->cvt, rgb_line, y_ptr, s->cb_buf2, s->cr_buf2, w, incr);
        break;
    case BPG_FORMAT_444:
        cb_ptr = (PIXEL *)(s->cb_buf + y * s->cb_linesize);
        cr_ptr = (PIXEL *)(s->cr_buf + y * s->cr_linesize);
        s->cvt_func(&s->cvt, rgb_line, y_ptr, cb_ptr, cr_ptr, w, incr);
        break;
    default:
        return -1;
    }

    /* alpha output or CMYK handling */
#ifdef USE_RGB48
    if (s->is_cmyk) {
        int i;
        /* convert RGBW to CMYK */
        if (s->is_16bpp) {
            if (!s->has_w_plane)
                put_dummy_gray16((uint16_t *)rgb_line + 3, w, 4);
            for(i = 0; i < 4; i++)
                gray_one_minus16((uint16_t *)rgb_line + i, w, 4);
        } else {
            if (!s->has_w_plane)
                put_dummy_gray8(rgb_line + 3, w, 4);
            for(i = 0; i < 4; i++)
                gray_one_minus8(rgb_line + i, w, 4);
        }
    } else
#endif
    if (s->has_w_plane) {
        a_ptr = (PIXEL *)(s->a_buf + y * s->a_linesize);
#ifdef USE_RGB48
        if (s->is_16bpp) {
            alpha_combine16(&s->cvt, (uint16_t *)rgb_line, a_ptr, w, incr);
            if (s->is_rgba)
                put_dummy_gray16((uint16_t *)rgb_line + 3, w, 4);
        } else
#endif
        {
            alpha_combine8(&s->cvt, rgb_line, a_ptr, w, incr);
            if (s->is_rgba)
                put_dummy_gray8(rgb_line + 3, w, 4);
        }
    } else if (s->is_rgba) {
#ifdef USE_RGB48
        if (s->is_16bpp) {
            if (s->has_alpha) {
                a_ptr = (PIXEL *)(s->a_buf + y * s->a_linesize);
                gray_to_gray16(&s->cvt, 
                               (uint16_t *)rgb_line + 3, a_ptr, w, 4);
                if (s->premultiplied_alpha)
                    alpha_divide16((uint16_t *)rgb_line, w);
            } else {
                put_dummy_gray16((uint16_t *)rgb_line + 3, w, 4);
            }
        } else
#endif
        {
            if (s->has_alpha) {
                a_ptr = (PIXEL *)(s->a_buf + y * s->a_linesize);
                gray_to_gray8(&s->cvt, rgb_line + 3, a_ptr, w, 4);
                if (s->premultiplied_alpha)
                    alpha_divide8((uint8_t *)rgb_line, w);
            } else {
                put_dummy_gray8(rgb_line + 3, w, 4);
            }
            }
    }

    /* go to next line */
    s->y++;
    return 0;
}

BPGDecoderContext *bpg_decoder_open(void)
{
    BPGDecoderContext *s;

    s = (BPGDecoderContext*)av_mallocz(sizeof(BPGDecoderContext));
    if (!s)
        return NULL;
    return s;
}

typedef struct {
    uint32_t width, height;
    BPGImageFormatEnum format;
    uint8_t has_alpha;
    uint8_t bit_depth;
    uint8_t has_w_plane;
    uint8_t premultiplied_alpha;
    uint8_t limited_range;
    uint8_t has_animation;
    uint16_t loop_count;
    uint16_t frame_delay_num;
    uint16_t frame_delay_den;
    BPGColorSpaceEnum color_space;
    uint32_t hevc_data_len;
    BPGExtensionData *first_md;
} BPGHeaderData;

static int bpg_decode_header(BPGHeaderData *h,
                             const uint8_t *buf, int buf_len,
                             int header_only, int load_extensions)
{
    int idx, flags1, flags2, has_extension, ret, alpha1_flag, alpha2_flag;
    uint32_t extension_data_len;

    if (buf_len < 6)
        return -1;
    /* check magic */
    if (buf[0] != ((BPG_HEADER_MAGIC >> 24) & 0xff) ||
        buf[1] != ((BPG_HEADER_MAGIC >> 16) & 0xff) ||
        buf[2] != ((BPG_HEADER_MAGIC >> 8) & 0xff) ||
        buf[3] != ((BPG_HEADER_MAGIC >> 0) & 0xff))
        return -1;
    idx = 4;
    flags1 = buf[idx++];
    h->format = (BPGImageFormatEnum)(flags1 >> 5);
    if (h->format > 5)
        return -1;
    alpha1_flag = (flags1 >> 4) & 1;
    h->bit_depth = (flags1 & 0xf) + 8;
    if (h->bit_depth > 14)
        return -1;
    flags2 = buf[idx++];
    h->color_space = (BPGColorSpaceEnum)((flags2 >> 4) & 0xf);
    has_extension = (flags2 >> 3) & 1;
    alpha2_flag = (flags2 >> 2) & 1;
    h->limited_range = (flags2 >> 1) & 1;
    h->has_animation = flags2 & 1;
    h->loop_count = 0;
    h->frame_delay_num = 0;
    h->frame_delay_den = 0;
    h->has_alpha = 0;
    h->has_w_plane = 0;
    h->premultiplied_alpha = 0;
    
    if (alpha1_flag) {
        h->has_alpha = 1;
        h->premultiplied_alpha = alpha2_flag;
    } else if (alpha2_flag) {
        h->has_alpha = 1;
        h->has_w_plane = 1;
    }

    if (h->color_space >= BPG_CS_COUNT || 
        (h->format == BPG_FORMAT_GRAY && h->color_space != 0) ||
        (h->has_w_plane && h->format == BPG_FORMAT_GRAY))
        return -1;
    ret = get_ue(&h->width, buf + idx, buf_len - idx);
    if (ret < 0)
        return -1;
    idx += ret;
    ret = get_ue(&h->height, buf + idx, buf_len - idx);
    if (ret < 0)
        return -1;
    idx += ret;
    if (h->width == 0 || h->height == 0)
        return -1;
    if (header_only)
        return idx;

    ret = get_ue(&h->hevc_data_len, buf + idx, buf_len - idx);
    if (ret < 0)
        return -1;
    idx += ret;
           
    extension_data_len = 0;
    if (has_extension) {
        ret = get_ue(&extension_data_len, buf + idx, buf_len - idx);
        if (ret < 0)
            return -1;
        idx += ret;
    }

    h->first_md = NULL;
    if (has_extension) {
        int ext_end;

        ext_end = idx + extension_data_len;
        if (ext_end > buf_len)
            return -1;
        if (load_extensions || h->has_animation) {
            BPGExtensionData *md, **plast_md;
            uint32_t tag, buf_len;

            plast_md = &h->first_md;
            while (idx < ext_end) {
                ret = get_ue32(&tag, buf + idx, ext_end - idx);
                if (ret < 0) 
                    goto fail;
                idx += ret;

                ret = get_ue(&buf_len, buf + idx, ext_end - idx);
                if (ret < 0) 
                    goto fail;
                idx += ret;
                
                if (idx + buf_len > ext_end) {
                fail:
                    bpg_decoder_free_extension_data(h->first_md);
                    return -1;
                }
                if (h->has_animation && tag == BPG_EXTENSION_TAG_ANIM_CONTROL) {
                    int idx1;
                    uint32_t loop_count, frame_delay_num, frame_delay_den;

                    idx1 = idx;
                    ret = get_ue(&loop_count, buf + idx1, ext_end - idx1);
                    if (ret < 0) 
                        goto fail;
                    idx1 += ret;
                    ret = get_ue(&frame_delay_num, buf + idx1, ext_end - idx1);
                    if (ret < 0) 
                        goto fail;
                    idx1 += ret;
                    ret = get_ue(&frame_delay_den, buf + idx1, ext_end - idx1);
                    if (ret < 0) 
                        goto fail;
                    idx1 += ret;
                    if (frame_delay_num == 0 || frame_delay_den == 0 ||
                        (uint16_t)frame_delay_num != frame_delay_num ||
                        (uint16_t)frame_delay_den != frame_delay_den ||
                        (uint16_t)loop_count != loop_count)
                        goto fail;
                    h->loop_count = loop_count;
                    h->frame_delay_num = frame_delay_num;
                    h->frame_delay_den = frame_delay_den;
                }
                if (load_extensions) {
                    md = (BPGExtensionData*)av_malloc(sizeof(BPGExtensionData));
                    md->tag = (BPGExtensionTagEnum)tag;
                    md->buf_len = buf_len;
                    md->next = NULL;
                    *plast_md = md;
                    plast_md = &md->next;
                    
                    md->buf = (uint8_t*)av_malloc(md->buf_len);
                    memcpy(md->buf, buf + idx, md->buf_len);
                }
                idx += buf_len;
            }
        } else
        {
            /* skip extension data */
            idx += extension_data_len;
        }
    }

    /* must have animation control extension for animations */
    if (h->has_animation && h->frame_delay_num == 0)
        goto fail;

    if (h->hevc_data_len == 0)
        h->hevc_data_len = buf_len - idx;
    
    return idx;
}

int bpg_decoder_decode(BPGDecoderContext *img, const uint8_t *buf, int buf_len)
{
    int idx, has_alpha, bit_depth, color_space, ret;
    uint32_t width, height;
    BPGHeaderData h_s, *h = &h_s;

    idx = bpg_decode_header(h, buf, buf_len, 0, img->keep_extension_data);
    if (idx < 0)
        return idx;
    width = h->width;
    height = h->height;
    has_alpha = h->has_alpha;
    color_space = h->color_space;
    bit_depth = h->bit_depth;
    
    img->w = width;
    img->h = height;
    img->format = h->format;
    if (h->format == BPG_FORMAT_422_VIDEO) {
        img->format = BPG_FORMAT_422;
        img->c_h_phase = 0;
    } else if (h->format == BPG_FORMAT_420_VIDEO) {
        img->format = BPG_FORMAT_420;
        img->c_h_phase = 0;
    } else {
        img->format = h->format;
        img->c_h_phase = 1;
    }
    img->has_alpha = has_alpha;
    img->premultiplied_alpha = h->premultiplied_alpha;
    img->has_w_plane = h->has_w_plane;
    img->limited_range = h->limited_range;
    img->color_space = (BPGColorSpaceEnum)color_space;
    img->bit_depth = bit_depth;
    img->has_animation = h->has_animation;
    img->loop_count = h->loop_count;
    img->frame_delay_num = h->frame_delay_num;
    img->frame_delay_den = h->frame_delay_den;

    img->first_md = h->first_md;

    if (idx + h->hevc_data_len > buf_len)
        goto fail;

    /* decode the first frame */
    ret = hevc_decode_start(img, buf + idx, buf_len - idx,
                            width, height, img->format, bit_depth, has_alpha);
    if (ret < 0)
        goto fail;
    idx += ret;

#ifdef USE_PRED
    /* XXX: add an option to avoid decoding animations ? */
    img->decode_animation = 1;
    if (img->has_animation && img->decode_animation) { 
        int len;
        /* keep trailing bitstream to decode the next frames */
        len = buf_len - idx;
        img->input_buf = (uint8_t*)av_malloc(len);
        if (!img->input_buf)
            goto fail;
        memcpy(img->input_buf, buf + idx, len);
        img->input_buf_len = len;
        img->input_buf_pos = 0;
    } else 
#endif
    {
        hevc_decode_end(img);
    }
    if (img->frame->width < img->w || img->frame->height < img->h)
        goto fail;
    img->y = -1;
    return 0;

 fail:
    av_frame_free(&img->frame);
    av_frame_free(&img->alpha_frame);
    bpg_decoder_free_extension_data(img->first_md);
    img->first_md = NULL;
    return -1;
}

void bpg_decoder_close(BPGDecoderContext *s)
{
    bpg_decoder_output_end(s);
    av_free(s->input_buf);
    hevc_decode_end(s);
    av_frame_free(&s->frame);
    av_frame_free(&s->alpha_frame);
    bpg_decoder_free_extension_data(s->first_md);
    av_free(s);
}

void bpg_decoder_free_extension_data(BPGExtensionData *first_md)
{
#ifndef EMSCRIPTEN
    BPGExtensionData *md, *md_next;
    
    for(md = first_md; md != NULL; md = md_next) {
        md_next = md->next;
        av_free(md->buf);
        av_free(md);
    }
#endif
}

#ifndef EMSCRIPTEN
void bpg_decoder_keep_extension_data(BPGDecoderContext *s, int enable)
{
    s->keep_extension_data = enable;
}

BPGExtensionData *bpg_decoder_get_extension_data(BPGDecoderContext *s)
{
    return s->first_md;
}

int bpg_decoder_get_info_from_buf(BPGImageInfo *p, 
                                  BPGExtensionData **pfirst_md,
                                  const uint8_t *buf, int buf_len)
{
    BPGHeaderData h_s, *h = &h_s;
    int parse_extension;

    parse_extension = (pfirst_md != NULL);
    if (bpg_decode_header(h, buf, buf_len, 
                          !parse_extension, parse_extension) < 0)
        return -1;
    p->width = h->width;
    p->height = h->height;
    p->format = h->format;
    p->has_alpha = h->has_alpha && !h->has_w_plane;
    p->premultiplied_alpha = h->premultiplied_alpha;
    p->has_w_plane = h->has_w_plane;
    p->limited_range = h->limited_range;
    p->color_space = h->color_space;
    p->bit_depth = h->bit_depth;
    p->has_animation = h->has_animation;
    p->loop_count = h->loop_count;
    if (pfirst_md)
        *pfirst_md = h->first_md;
    return 0;
}
#endif
