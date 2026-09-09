/* LiquidBubble's painter: a well of coloured liquid behind glass. The liquid
 * swirls on its own and tilts with the window it lives in (the engine's
 * slosh variables). Anatomy: shell (the well) → slosh frame → liquid back +
 * front → gloss → glyph. */
#ifndef MARYUI_LP_BUBBLE_H
#define MARYUI_LP_BUBBLE_H

#include <cairo.h>

#include "maryui/lp_types.h"

enum lp_bubble_tint { LP_TINT_CLOSE, LP_TINT_MINIMIZE, LP_TINT_ZOOM, LP_TINT_INACTIVE, LP_TINT_ACCENT, LP_TINT_PLATINUM };

typedef struct lp_bubble_colors { lp_color base, deep, light; } lp_bubble_colors;
/* The tint's three colours (accent from the settings' accent). */
lp_bubble_colors lp_bubble_tint_colors(enum lp_bubble_tint tint, const lp_color *accent_base, const lp_color *accent_deep, const lp_color *accent_light);

typedef struct lp_bubble_spec {
    lp_bubble_colors colors;
    float size;         /* diameter, px */
    float phase_s;      /* swirl phase offset, seconds */
    float fill;         /* 0..1; < 0 takes liquid.fill */
    double now_ms;      /* drives the swirl; 0 freezes it */
    float slosh_deg, slosh_y;
    const char *glyph;  /* "×", "–", "+", "−" or NULL */
    float glyph_alpha;  /* 0 hidden … 1 shown */
} lp_bubble_spec;

void lp_bubble_paint(cairo_t *cr, float cx, float cy, const lp_bubble_spec *spec);

#endif
