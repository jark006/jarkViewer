#ifndef __PSD_COLOR_H__
#define __PSD_COLOR_H__

#ifdef __cplusplus
extern "C" {
#endif


#define PSD_ARGB_TO_COLOR(a, r, g, b)		(((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define PSD_RGB_TO_COLOR(r, g, b)			(((0xFF) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define PSD_GET_ALPHA_COMPONENT(color)		((color) >> 24)
#define PSD_GET_RED_COMPONENT(color)		(((color) & 0x00FF0000) >> 16)
#define PSD_GET_GREEN_COMPONENT(color)		(((color) & 0x0000FF00) >> 8)
#define PSD_GET_BLUE_COMPONENT(color)		((color) & 0x000000FF)
#define PSD_GET_COLOR_INTENSITY(r, g, b)	(((r) * 77 + (g) * 151 + (b) * 28) >> 8)

// Some predefined color constants
#define psd_color_black      	0xFF000000
#define psd_color_dimGray       0xFF3F3F3F
#define psd_color_gray          0xFF7F7F7F
#define psd_color_lightGray     0xFFBFBFBF
#define psd_color_white         0xFFFFFFFF
#define psd_color_maroon        0xFF7F0000
#define psd_color_green         0xFF007F00
#define psd_color_olive         0xFF7F7F00
#define psd_color_navy          0xFF00007F
#define psd_color_Purple        0xFF7F007F
#define psd_color_teal          0xFF007F7F
#define psd_color_red           0xFFFF0000
#define psd_color_lime          0xFF00FF00
#define psd_color_blue          0xFF0000FF
#define psd_color_yellow        0xFFFFFF00
#define psd_color_fuchsia       0xFFFF00FF
#define psd_color_aqua         	0xFF00FFFF

// Some semi-transparent color constants
#define psd_color_trwhite       0x7FFFFFFF
#define psd_color_trblack       0x7F000000
#define psd_color_trred         0x7FFF0000
#define psd_color_trgreen       0x7F00FF00
#define psd_color_trblue        0x7F0000FF

#define psd_color_clear			0x00FFFFFF


psd_argb_color psd_argb_to_color(psd_color_component alpha, psd_color_component red, 
	psd_color_component green, psd_color_component blue);
psd_argb_color psd_rgb_to_color(psd_color_component red, psd_color_component green, 
	psd_color_component blue);
psd_argb_color psd_cmyk_to_color(psd_double cyan, psd_double magenta, psd_double yellow, psd_double black);
psd_argb_color psd_acmyk_to_color(psd_color_component alpha, psd_double cyan, 
	psd_double magenta, psd_double yellow, psd_double black);
psd_argb_color psd_intcmyk_to_color(psd_int cyan, psd_int magenta, psd_int yellow, psd_int black);
psd_argb_color psd_intacmyk_to_color(psd_color_component alpha, psd_int cyan, psd_int magenta, psd_int yellow, psd_int black);
psd_argb_color psd_lab_to_color(psd_int lightness, psd_int a, psd_int b);
psd_argb_color psd_alab_to_color(psd_color_component alpha, psd_int lightness, psd_int a, psd_int b);
psd_argb_color psd_xyz_to_color(psd_double x, psd_double y, psd_double z);
psd_argb_color psd_axyz_to_color(psd_color_component alpha, psd_double x, psd_double y, psd_double z);
psd_argb_color psd_hsb_to_color(psd_int hue, psd_double saturation, psd_double brightness);
psd_argb_color psd_ahsb_to_color(psd_color_component alpha, psd_int hue, psd_double saturation, psd_double brightness);
psd_color_component psd_get_alpha_component(psd_argb_color color);
psd_color_component psd_get_red_component(psd_argb_color color);
psd_color_component psd_get_green_component(psd_argb_color color);
psd_color_component psd_get_blue_component(psd_argb_color color);
psd_status psd_color_space_to_argb(psd_argb_color * dst_color, psd_color_space color_space, psd_ushort color_component[4]);
void psd_color_memset(psd_argb_color * bits, psd_argb_color color, psd_int length);
void psd_rgb_to_inthsb(psd_int red, psd_int green, psd_int blue, psd_int * hue, psd_int * saturation, psd_int * brightness);
void psd_inthsb_to_rgb(psd_int hue, psd_int saturation, psd_int brightness, psd_int * red, psd_int * green, psd_int * blue);
psd_int psd_rgb_get_brightness(psd_int red, psd_int green, psd_int blue);
void psd_rgb_to_intcmyk(psd_int red, psd_int green, psd_int blue, 
	psd_int * cyan, psd_int * magenta, psd_int * yellow, psd_int * black);
void psd_intcmyk_to_rgb(psd_int cyan, psd_int magenta, psd_int yellow, psd_int black, 
	psd_int * red, psd_int * green, psd_int * blue);


#ifdef __cplusplus
}
#endif

#endif
