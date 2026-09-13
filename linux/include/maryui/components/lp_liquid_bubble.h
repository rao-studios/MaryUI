/* LiquidBubble — the component over lp_bubble_paint: takes the enclosing
 * window's slosh from the context and the wave's clock from ctx->now_ms, and
 * gives every orb a person can point at the same response. */
#ifndef MARYUI_LP_LIQUID_BUBBLE_H
#define MARYUI_LP_LIQUID_BUBBLE_H

#include "maryui/lp_bubble.h"
#include "maryui/lp_ui.h"

/* Fills in everything the context owns: the tint's colours, the slosh and the
 * clock. The caller sets size, phase, fill and the glyph. */
lp_bubble_spec lp_liquid_bubble_spec(lp_ctx *ctx, float size, enum lp_bubble_tint tint, float phase_s, float fill);
/* The orb idiom. While id is hot the well fills to the brim; while it is held
 * down under the pointer the liquid darkens a shade. Each eases in, and eases
 * back out from wherever it had reached. Call it in the DRAW pass on a spec
 * from lp_liquid_bubble_spec, before painting; r is the orb's rectangle, which
 * it asks frames for while either look is moving. id 0 is a decorative orb and
 * is left as it is. */
void lp_liquid_bubble_respond(lp_ctx *ctx, lp_id id, lp_rect r, lp_bubble_spec *spec);
/* Spec, response and paint in one. id is the widget the orb belongs to, or 0. */
void lp_liquid_bubble(lp_ctx *ctx, lp_id id, float cx, float cy, float size, enum lp_bubble_tint tint, float phase_s, float fill, const char *glyph, float glyph_alpha);

#endif
