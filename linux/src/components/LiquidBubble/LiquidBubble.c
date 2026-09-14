#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/lp_motion.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_tokens.h"

/* Hover fills past the brim, so the rolling surface never bares a sliver of
 * glass at the top of the well; holding darkens each stop 35 % toward the next
 * one down the tint. Linux only (PARITY.md D17). */
#define ORB_BRIM 1.12f
#define ORB_FILL_MS LP_MOTION_SLOW_MS
#define ORB_DRAIN_MS LP_MOTION_SLOW_MS
#define ORB_DARKEN 0.35f
#define ORB_PRESS_MS LP_MOTION_FAST_MS
#define ORB_RELEASE_MS LP_MOTION_NORMAL_MS

lp_bubble_spec lp_liquid_bubble_spec(lp_ctx *ctx, float size, enum lp_bubble_tint tint, float phase_s, float fill) {
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_bubble_spec spec = {
        .colors = lp_bubble_tint_colors(tint, &accent.base, &accent.deep, &accent.light),
        .size = size, .phase_s = phase_s, .fill = fill, .now_ms = ctx->now_ms,
        .slosh_deg = ctx->slosh_deg, .slosh_x = ctx->slosh_x, .slosh_y = ctx->slosh_y,
    };
    return spec;
}

void lp_liquid_bubble_respond(lp_ctx *ctx, lp_id id, lp_rect r, lp_bubble_spec *spec) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr || !id) return;
    int moving = 0;
    float hover = lp_hover_progress(ctx, id, ORB_FILL_MS, ORB_DRAIN_MS, &moving);
    float held = lp_press_progress(ctx, id, ORB_PRESS_MS, ORB_RELEASE_MS, &moving);
    float rest = spec->fill >= 0 ? spec->fill : LP_LIQUID_FILL;
    spec->fill = rest + (ORB_BRIM - rest) * hover;
    spec->darken = ORB_DARKEN * held;
    if (moving) lp_want_frame_rect(ctx, r);
}

void lp_liquid_bubble(lp_ctx *ctx, lp_id id, float cx, float cy, float size, enum lp_bubble_tint tint, float phase_s, float fill, const char *glyph, float glyph_alpha) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_bubble_spec spec = lp_liquid_bubble_spec(ctx, size, tint, phase_s, fill);
    spec.glyph = glyph;
    spec.glyph_alpha = glyph_alpha;
    lp_liquid_bubble_respond(ctx, id, LP_RECT(cx - size / 2, cy - size / 2, size, size), &spec);
    lp_bubble_paint(ctx->cr, cx, cy, &spec);
}
