#ifndef __PSD_BLEND_H__
#define __PSD_BLEND_H__

#ifdef __cplusplus
extern "C" {
#endif


#define PSD_BLEND_CHANNEL(b, f, a)				((((b) << 8) + ((f) - (b)) * (a)) >> 8)
#define PSD_BLEND_ALPHA(b, f)					((b) + ((255 - (b)) * (f) >> 8))


// psd_blend_mode_normal
#define PSD_BLEND_NORMAL(b, f, a)							\
do {														\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_dissolve

// psd_blend_mode_darken
#define PSD_BLEND_DARKEN(b, f, a)							\
do {														\
	if(f <= b)												\
		b = PSD_BLEND_CHANNEL(b, f, a);						\
} while(0)

// psd_blend_mode_multiply
#define PSD_BLEND_MULTIPLY(b, f, a)							\
do {														\
	f = f * b >> 8;											\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_color_burn
#define PSD_BLEND_COLOR_BURN(b, f, a)						\
do {														\
	if(f > 0)												\
	{														\
		f = ((255 - b) << 8) / f;							\
		f = f > 255 ? 0 : (255 - f);						\
	}														\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_linear_burn
#define PSD_BLEND_LINEEAR_BURN(b, f, a)						\
do {														\
	f = f < (255 - b) ? 0 : f - (255 - b);					\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_lighten
#define PSD_BLEND_LIGHTEN(b, f, a)							\
do {														\
	if(f >= b)												\
		b = PSD_BLEND_CHANNEL(b, f, a);						\
} while(0)

// psd_blend_mode_screen
#define PSD_BLEND_SCREEN(b, f, a)							\
do {														\
	f = 255 - ((255 - b) * (255 - f) >> 8);					\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_color_dodge
#define PSD_BLEND_COLOR_DODGE(b, f, a)						\
do {														\
	if(f < 255)												\
	{														\
		f = (b << 8) / (255 - f);							\
		if(f > 255)											\
			f = 255;										\
	}														\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_linear_dodge
#define PSD_BLEND_LINEAR_DODGE(b, f, a)						\
do {														\
	f = b + f > 255 ? 255 : b + f;							\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_overlay
#define PSD_BLEND_OVERLAY(b, f, a)							\
do {														\
	if(b < 128)												\
		f = b * f >> 7;										\
	else													\
		f = 255 - ((255 - b) * (255 - f) >> 7);				\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_soft_light
#define PSD_BLEND_SOFTLIGHT(b, f, a)						\
do {														\
	psd_int c1, c2;												\
	c1 = b * f >> 8;										\
	c2 = 255 - ((255 - b) * (255 - f) >> 8);				\
	f = ((255 - b) * c1 >> 8) + (b * c2 >> 8);				\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_hard_light
#define PSD_BLEND_HARDLIGHT(b, f, a)						\
do {														\
	if(f < 128)												\
		f = b * f >> 7;										\
	else													\
		f = 255 - ((255 - f) * (255 - b) >> 7);				\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_vivid_light
#define PSD_BLEND_VIVID_LIGHT(b, f, a)						\
do {														\
	if(f < 255)												\
	{														\
		f = (b * b / (255 - f) + f * f / (255 - b)) >> 1;	\
		if(f > 255)											\
			f = 255;										\
	}														\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_linear_light
#define PSD_BLEND_LINEAR_LIGHT(b, f, a)						\
do {														\
	if(b < 255)												\
	{														\
		f = f * f / (255 - b);								\
		if(f > 255)											\
			f = 255;										\
	}														\
	else													\
		f = 255;											\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_pin_light
#define PSD_BLEND_PIN_LIGHT(b, f, a)						\
do {														\
	if(f >= 128)											\
		f = PSD_MAX(b, (f - 128) * 2);						\
	else													\
		f = PSD_MIN(b, f * 2);								\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_hard_mix
#define PSD_BLEND_HARD_MIX(b, f, a)							\
do {														\
	if(b + f <= 255)										\
		f = 0;												\
	else													\
		f = 255;											\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_difference
#define PSD_BLEND_DIFFERENCE(b, f, a)						\
do {														\
	f = PSD_ABS(b - f);										\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

// psd_blend_mode_exclusion
#define PSD_BLEND_EXCLUSION(b, f, a)						\
do {														\
	f = b + f - (b * f >> 7);								\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_ADDITIVE(b, f, a)							\
do {														\
	f += b;													\
	if(f > 255)												\
		f = 255;											\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_REFLECT(b, f, a)							\
do {														\
	if(f < 255)												\
	{														\
		f = b * b / (255 - f);								\
		if(f > 255)											\
			f = 255;										\
	}														\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_GLOW(b, f, a)								\
do {														\
	if(b < 255)												\
	{														\
		f = f * f / (255 - b);								\
		if(f > 255)											\
			f = 255;										\
	}														\
	else													\
		f = 255;											\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_HEAT(b, f, a)								\
do {														\
	if(b > 0)												\
	{														\
		f = (255 - f) * (255 - f) / b;						\
		f = f > 255 ? 0 : (255 - f);						\
	}														\
	else													\
		f = 0;												\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_FREEZE(b, f, a)							\
do {														\
	if(f > 0)												\
	{														\
		f = (255 - b) * (255 - b) / f;						\
		f = f > 255 ? 0 : (255 - f);						\
	}														\
	else													\
		f = 0;												\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_AVERAGE(b, f, a)							\
do {														\
	f = (b + f) >> 1;										\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_BRIGHTLIGHT(b, f, a)						\
do {														\
	f = sqrt(f * b);										\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)

#define PSD_BLEND_NEGATION(b, f, a)							\
do {														\
	f = 255 - PSD_ABS(255 - b - f);							\
	b = PSD_BLEND_CHANNEL(b, f, a);							\
} while(0)



#ifdef __cplusplus
}
#endif

#endif
