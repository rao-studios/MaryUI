#include <math.h>

#include "maryui/components/lp_controls.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_tokens.h"

void lp_progress(lp_ctx *ctx, lp_rect r, float value) {
    if (ctx->pass == LP_PASS_EVENT) { if (value < 0) lp_want_frame_rect(ctx, r); return; }
    if (!ctx->cr) return;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_fill_solid(cr, r, LP_PLATINUM_4, LP_RADIUS_PILL);
    cairo_save(cr);
    lp_path_rrect(cr, r, LP_RADIUS_PILL);
    cairo_clip(cr);
    if (value < 0) {
        /* barber pole: repeating-linear-gradient(-55deg, platinum.1 0 8px, platinum.4 8px 16px), travelling 22.6px per 1.1s */
        double phase = fmod(ctx->now_ms / 1100.0, 1.0) * 22.6;
        cairo_save(cr);
        cairo_translate(cr, r.x + phase, r.y);
        cairo_rotate(cr, -55 * M_PI / 180.0);
        cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 16, 0);
        lp_color a = LP_PLATINUM_1, b = LP_PLATINUM_4;
        cairo_pattern_add_color_stop_rgba(p, 0, a.r, a.g, a.b, 1);
        cairo_pattern_add_color_stop_rgba(p, 0.5, a.r, a.g, a.b, 1);
        cairo_pattern_add_color_stop_rgba(p, 0.5, b.r, b.g, b.b, 1);
        cairo_pattern_add_color_stop_rgba(p, 1, b.r, b.g, b.b, 1);
        cairo_pattern_set_extend(p, CAIRO_EXTEND_REPEAT);
        cairo_set_source(cr, p);
        cairo_paint(cr);
        cairo_pattern_destroy(p);
        cairo_restore(cr);
        lp_want_frame_rect(ctx, r);
    } else {
        float pct = value > 1 ? 1 : value;
        lp_rect fill = LP_RECT(r.x, r.y, r.w * pct, r.h);
        if (fill.w > 0) {
            cairo_pattern_t *p = cairo_pattern_create_linear(0, r.y, 0, r.y + r.h);
            cairo_pattern_add_color_stop_rgba(p, 0, accent.light.r, accent.light.g, accent.light.b, 1);
            cairo_pattern_add_color_stop_rgba(p, 0.55, accent.base.r, accent.base.g, accent.base.b, 1);
            cairo_pattern_add_color_stop_rgba(p, 1, accent.deep.r, accent.deep.g, accent.deep.b, 1);
            lp_path_rrect(cr, fill, LP_RADIUS_PILL);
            cairo_set_source(cr, p);
            cairo_fill(cr);
            cairo_pattern_destroy(p);
            lp_fill_solid(cr, LP_RECT(fill.x, fill.y, fill.w, 1), LP_RGBA(1, 1, 1, 0.4f), 0);
            /* the glint: a 40%-wide band sweeping -40% → 140% every 2.8s */
            double t = fmod(ctx->now_ms / 2800.0, 1.0);
            float gx = fill.x + (float)(-0.4 + 1.8 * t) * fill.w;
            cairo_pattern_t *g = cairo_pattern_create_linear(gx, 0, gx + fill.w * 0.4f, 0);
            cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, 0);
            cairo_pattern_add_color_stop_rgba(g, 0.5, 1, 1, 1, 0.35);
            cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, 0);
            lp_path_rrect(cr, fill, LP_RADIUS_PILL);
            cairo_set_source(cr, g);
            cairo_fill(cr);
            cairo_pattern_destroy(g);
            /* The glint only travels while the bar is partly filled; a full or
              * empty bar is static and must not keep the desktop awake. */
            if (pct > 0.001f && pct < 0.999f) lp_want_frame_rect(ctx, r);
        }
    }
    cairo_restore(cr);
    lp_draw_inset_shadows(cr, r, LP_RADIUS_PILL, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
}
