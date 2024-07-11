#ifndef __PSD_FIXED_H__
#define __PSD_FIXED_H__

#ifdef __cplusplus
extern "C" {
#endif


// A signed 26.6 fixed psd_float type used for vectorial pixel coordinates.
typedef psd_int				psd_fixed_26_6;
typedef psd_int				psd_fixed_16_16;
typedef psd_int				psd_fixed_8_24;


#define PSD_FIXED_26_6_ONE				64
#define PSD_FIXED_16_16_ONE				65536
#define PSD_FIXED_8_24_ONE				0x1000000

#define PSD_FIXED_26_6_INT(i)			((i) << 6)
#define PSD_FIXED_26_6_FLOOR(f)			((f) >> 6)
#define PSD_FIXED_26_6_CEIL(f)			(((f) + 63) >> 6)
#define PSD_FIXED_26_6_ROUND(f)			(((f) + 31) >> 6)
#define PSD_FIXED_16_16_INT(i)			((i) << 16)
#define PSD_FIXED_16_16_FLOOR(f)		((f) >> 16)
#define PSD_FIXED_16_16_CEIL(f)			(((f) + 65535) >> 16)
#define PSD_FIXED_16_16_ROUND(f)		(((f) + 32767) >> 16)
#define PSD_FIXED_8_24_INT(i)			((i) << 24)
#define PSD_FIXED_8_24_FLOOR(f)			((f) >> 24)
#define PSD_FIXED_8_24_CEIL(f)			(((f) + 255) >> 24)
#define PSD_FIXED_8_24_ROUND(f)			(((f) + 127) >> 24)


psd_fixed_26_6 psd_fixed_26_6_float(psd_float s);
psd_fixed_26_6 psd_fixed_26_6_int(psd_int i);
psd_int psd_fixed_26_6_floor(psd_fixed_26_6 f);
psd_int psd_fixed_26_6_ceil(psd_fixed_26_6 f);
psd_int psd_fixed_26_6_round(psd_fixed_26_6 f);
psd_fixed_16_16 psd_fixed_16_16_float(psd_float s);
psd_fixed_16_16 psd_fixed_16_16_int(psd_int i);
psd_int psd_fixed_16_16_floor(psd_fixed_16_16 f);
psd_int psd_fixed_16_16_ceil(psd_fixed_16_16 f);
psd_int psd_fixed_16_16_round(psd_fixed_16_16 f);
psd_float psd_fixed_16_16_tofloat(psd_fixed_16_16 f);
psd_fixed_16_16 psd_fixed_16_16_mul(psd_fixed_16_16 first, psd_fixed_16_16 second);
psd_fixed_16_16 psd_fixed_16_16_div(psd_fixed_16_16 first, psd_fixed_16_16 second);
psd_fixed_8_24 psd_fixed_8_24_float(psd_float s);
psd_fixed_8_24 psd_fixed_8_24_int(psd_int i);
psd_int psd_fixed_8_24_floor(psd_fixed_8_24 f);
psd_int psd_fixed_8_24_ceil(psd_fixed_8_24 f);
psd_int psd_fixed_8_24_round(psd_fixed_8_24 f);
psd_float psd_fixed_8_24_tofloat(psd_fixed_8_24 f);


#ifdef __cplusplus
}
#endif

#endif
