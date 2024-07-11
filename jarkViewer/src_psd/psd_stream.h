#ifndef __PSD_STREAM_H__
#define __PSD_STREAM_H__

#ifdef __cplusplus
extern "C" {
#endif


#define PSD_STREAM_MAX_READ_LENGTH		4096

#define PSD_CHAR_TO_SHORT(str)			((*(str) << 8) | *((str) + 1))
#define PSD_CHAR_TO_INT(str)			((*(str) << 24) | (*((str) + 1) << 16) | (*((str) + 2) << 8) | *((str) + 3))


psd_int psd_stream_get(psd_context * context, psd_uchar * buffer, psd_int length);
psd_int psd_stream_get_null(psd_context * context, psd_int length);
psd_bool psd_stream_get_bool(psd_context * context);
psd_uchar psd_stream_get_char(psd_context * context);
psd_short psd_stream_get_short(psd_context * context);
psd_int psd_stream_get_int(psd_context * context);
psd_float psd_stream_get_float(psd_context * context);
psd_double psd_stream_get_double(psd_context * context);
psd_argb_color psd_stream_get_space_color(psd_context * context);
psd_blend_mode psd_stream_get_blend_mode(psd_context * context);
void psd_stream_free(psd_context * context);
void psd_stream_init_file(psd_context * context);
void psd_stream_init_memory(psd_context * context);

#ifdef __cplusplus
}
#endif

#endif
