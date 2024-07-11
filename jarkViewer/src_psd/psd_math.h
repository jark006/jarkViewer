#ifndef __PSD_MATH_H__
#define __PSD_MATH_H__


#define PSD_MIN(a, b)  			(((a) < (b)) ? (a) : (b))
#define PSD_MAX(a, b)  			(((a) > (b)) ? (a) : (b))
#define PSD_CONSTRAIN(value, lo, hi) (PSD_MIN(PSD_MAX((value), (lo)), (hi)))
#define PSD_ABS(a)				((a) > 0 ? (a) : -(a))
#define PSD_SIGN(a)				((a) == 0 ? 0 : ((a) > 0 ? 1 : -1))

#define PSD_PI					3.1415927f
#define PSD_PI_4				(3.1415927f / 4)


#endif
