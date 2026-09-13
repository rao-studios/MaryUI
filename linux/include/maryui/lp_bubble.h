/* LiquidBubble's painter: a well of coloured liquid behind pale glass. The
 * liquid rolls on its own and banks with the window it lives in (the engine's
 * slosh variables). Anatomy: shell (the glass well) → slosh frame → liquid
 * back + front → gloss → rim → glyph.
 *
 * The waterline is the boundary between the pale glass above and the colour
 * below, not a painted line: a well painted in the tint's deep colour put a
 * dark cap over every bead and muddied the identity of all three lights. */
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
    float phase_s;      /* wave phase offset, seconds */
    float fill;         /* 0..1; < 0 takes liquid.fill */
    double now_ms;      /* drives the wave; 0 freezes it */
    float slosh_deg, slosh_x, slosh_y;
    /* Everything the slosh moves is scaled by size/12, so an 18px bead banks
     * half again as far as the 12px one the numbers were tuned against. */
    const char *glyph;  /* "×", "–", "+", "−" or NULL */
    float glyph_alpha;  /* 0 hidden … 1 shown */
    float glyph_scale;  /* glyph size as a fraction of the diameter; 0 takes 0.72 */
    float glyph_ink;    /* glyph opacity before glyph_alpha; 0 takes 0.55 */
    int liquid_only;    /* paint the liquid alone: no glass, no gloss, no rim, no glyph.
                         * This is the mass GooGroup's filter merges. */
    float darken;       /* 0..1: slides the liquid's colours one step down the tint (a hovered traffic light) */
} lp_bubble_spec;

void lp_bubble_paint(cairo_t *cr, float cx, float cy, const lp_bubble_spec *spec);

#endif
