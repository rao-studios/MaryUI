#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/lp_settings.h"

void lp_liquid_bubble(lp_ctx *ctx, float cx, float cy, float size, enum lp_bubble_tint tint, float phase_s, float fill, const char *glyph, float glyph_alpha) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_bubble_spec spec = {
        .colors = lp_bubble_tint_colors(tint, &accent.base, &accent.deep, &accent.light),
        .size = size, .phase_s = phase_s, .fill = fill, .now_ms = ctx->now_ms,
        .slosh_deg = ctx->slosh_deg, .slosh_y = ctx->slosh_y, .glyph = glyph, .glyph_alpha = glyph_alpha,
    };
    lp_bubble_paint(ctx->cr, cx, cy, &spec);
}
