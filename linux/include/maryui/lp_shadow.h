/* Outer box-shadows as cached 9-slice sprites. A Gaussian-blurred rounded box
 * is translation-invariant along its straight edges, so one small render
 * per (layers, radius) stretches exactly to any size — the window and menu
 * shadows cost blits, not blurs, on every repaint. */
#ifndef MARYUI_LP_SHADOW_H
#define MARYUI_LP_SHADOW_H

#include <cairo.h>

#include "maryui/lp_types.h"

typedef struct lp_shadow_sprite {
    cairo_surface_t *surface;
    int extent;   /* how far the shadow reaches outside the box on every side */
    int corner;   /* size of the non-stretching corner cell, outside extent included */
    int box;      /* the reference box's side */
    const lp_shadow_layer *layers;
    int count;
    float radius;
} lp_shadow_sprite;

/* Extent of the outer layers on each side, for sizing chrome buffers. */
void lp_shadow_extents(const lp_shadow_layer *layers, int n, int *left, int *top, int *right, int *bottom);
/* The cached sprite for these outer layers around a box with this corner radius. */
const lp_shadow_sprite *lp_shadow_get(const lp_shadow_layer *layers, int n, float radius);
/* Paints the shadow around r (under it too: the caller paints the box on top). */
void lp_shadow_paint(cairo_t *cr, const lp_shadow_sprite *sprite, lp_rect r);
/* The two together. */
void lp_draw_shadow_9slice(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n);

#endif
