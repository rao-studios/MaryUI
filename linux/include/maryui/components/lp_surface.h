/* Surface — the brushed platinum every other component sits on. Three
 * layers: the variant's vertical gradient with the emboss, the brush grain
 * under OVERLAY, and (optionally) the sliding sheen. Compose it; never paint
 * metal by hand. Mirrors web/src/components/Surface. */
#ifndef MARYUI_LP_SURFACE_H
#define MARYUI_LP_SURFACE_H

#include "maryui/lp_ui.h"

/* LP_VARIANT_* rather than LP_SURFACE_*: the latter are the colour tokens. */
enum lp_surface_variant {
    LP_VARIANT_RAISED,   /* buttons, thumbs */
    LP_VARIANT_FLAT,     /* window chrome */
    LP_VARIANT_WELL,     /* inset fields */
    LP_VARIANT_BAR,      /* the menu bar */
    LP_VARIANT_TITLEBAR,
    LP_VARIANT_BODY,     /* content area */
};

typedef struct lp_surface_opts {
    enum lp_surface_variant variant;
    float radius;
    int sheen;            /* paint the specular band (title bars, the menu bar) */
    float sheen_alpha;    /* < 0: the token (or the inactive token when !ctx->active_window) */
    int inactive;         /* the title bar's inactive stops */
    float grain_x, grain_y;  /* how far the brushed skin lags the frame */
    float speed;          /* 0..1; the grain glints as the window picks up speed */
} lp_surface_opts;

/* The two gradient stops a variant uses (surface.* tokens). */
void lp_surface_stops(enum lp_surface_variant v, int inactive, lp_color *top, lp_color *bottom, float *brush);

/* Draws the surface into r (no-op in the EVENT pass). */
void lp_surface(lp_ctx *ctx, lp_rect r, lp_surface_opts opts);
/* Lower level: with an explicit cairo context and motion variables. */
void lp_surface_paint(cairo_t *cr, lp_rect r, lp_surface_opts opts, float sheen_x, float tilt_deg);

#endif
