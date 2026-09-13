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

static float eased(double elapsed_ms, float duration_ms) {
    float t = (float)(elapsed_ms / duration_ms);
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return lp_cubic_bezier_eval(LP_MOTION_EASE_OUT, t);
}

/* How far into a look an orb is: rising over in_ms while it is on, and falling
 * over out_ms from wherever it had reached once it was last on. Sets *moving
 * while that is still changing. */
static float engaged(double now, int on, double since, int was, double was_since, double was_until,
                     float in_ms, float out_ms, int *moving) {
    if (on) {
        if (now - since < in_ms) *moving = 1;
        return eased(now - since, in_ms);
    }
    if (!was) return 0;
    float reached = eased(was_until - was_since, in_ms);
    float left = 1 - eased(now - was_until, out_ms);
    if (reached > 0 && left > 0) *moving = 1;
    return reached * left;
}

void lp_liquid_bubble_respond(lp_ctx *ctx, lp_id id, lp_rect r, lp_bubble_spec *spec) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr || !id) return;
    int moving = 0;
    float hover = engaged(ctx->now_ms, ctx->hot == id, ctx->hot_since_ms, ctx->last_hot == id, ctx->last_hot_since_ms,
                          ctx->last_hot_until_ms, ORB_FILL_MS, ORB_DRAIN_MS, &moving);
    float held = engaged(ctx->now_ms, ctx->held == id, ctx->held_since_ms, ctx->last_held == id, ctx->last_held_since_ms,
                         ctx->last_held_until_ms, ORB_PRESS_MS, ORB_RELEASE_MS, &moving);
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
