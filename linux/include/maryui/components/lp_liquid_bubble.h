/* LiquidBubble — the component over lp_bubble_paint: takes the enclosing
 * window's slosh from the context and the swirl time from ctx->now_ms. */
#ifndef MARYUI_LP_LIQUID_BUBBLE_H
#define MARYUI_LP_LIQUID_BUBBLE_H

#include "maryui/lp_bubble.h"
#include "maryui/lp_ui.h"

void lp_liquid_bubble(lp_ctx *ctx, float cx, float cy, float size, enum lp_bubble_tint tint, float phase_s, float fill, const char *glyph, float glyph_alpha);

#endif
