/* Gaussian blur on float planes and on Cairo ARGB32 surfaces (feGaussianBlur). */
#ifndef MARYUI_LP_BLUR_H
#define MARYUI_LP_BLUR_H

#include <cairo.h>

/* Separable Gaussian with a true kernel (radius 3σ); edges clamp. */
void lp_blur_plane(float *plane, int w, int h, float sigma, float *scratch);
/* Every channel of a premultiplied ARGB32 image surface, in place. */
void lp_blur_surface(cairo_surface_t *surface, float sigma);

#endif
