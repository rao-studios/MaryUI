/* LiquidBubble — the component over lp_bubble_paint: takes the enclosing
 * window's slosh from the context and the wave's clock from ctx->now_ms. */
#ifndef MARYUI_LP_LIQUID_BUBBLE_H
#define MARYUI_LP_LIQUID_BUBBLE_H

#include "maryui/lp_bubble.h"
#include "maryui/lp_ui.h"

/* Fills in everything the context owns: the tint's colours, the slosh and the
 * clock. The caller sets size, phase, fill and the glyph. */
lp_bubble_spec lp_liquid_bubble_spec(lp_ctx *ctx, float size, enum lp_bubble_tint tint, float phase_s, float fill);
void lp_liquid_bubble(lp_ctx *ctx, float cx, float cy, float size, enum lp_bubble_tint tint, float phase_s, float fill, const char *glyph, float glyph_alpha);

#endif
