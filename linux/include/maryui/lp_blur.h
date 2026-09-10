/* Gaussian blur on float planes and on Cairo ARGB32 surfaces (feGaussianBlur). */
#ifndef MARYUI_LP_BLUR_H
#define MARYUI_LP_BLUR_H

#include <cairo.h>

#include "maryui/lp_types.h"

/* Separable Gaussian with a true kernel (radius 3σ); edges clamp. */
void lp_blur_plane(float *plane, int w, int h, float sigma, float *scratch);
/* Every channel of a premultiplied ARGB32 image surface, in place. */
void lp_blur_surface(cairo_surface_t *surface, float sigma);
/* The same, for an image that is one flat colour: blurs alpha only and rebuilds
 * the premultiplied colour channels from `tint`. A quarter of the work, and
 * identical output. This is what every shadow layer needs. */
void lp_blur_surface_tinted(cairo_surface_t *surface, float sigma, lp_color tint);

#endif
