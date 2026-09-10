#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_tokens.h"

void lp_surface_stops(enum lp_surface_variant v, int inactive, lp_color *top, lp_color *bottom, float *brush) {
    *brush = LP_BRUSH_OPACITY;
    switch (v) {
    case LP_VARIANT_RAISED: *top = LP_SURFACE_RAISED_TOP; *bottom = LP_SURFACE_RAISED_BOTTOM; break;
    case LP_VARIANT_TITLEBAR:
        if (inactive) { *top = LP_SURFACE_TITLEBAR_INACTIVE_TOP; *bottom = LP_SURFACE_TITLEBAR_INACTIVE_BOTTOM; }
        else { *top = LP_SURFACE_TITLEBAR_TOP; *bottom = LP_SURFACE_TITLEBAR_BOTTOM; }
        break;
    case LP_VARIANT_BAR: *top = LP_SURFACE_MENUBAR_TOP; *bottom = LP_SURFACE_MENUBAR_BOTTOM; break;
    case LP_VARIANT_WELL: *top = LP_SURFACE_WELL; *bottom = LP_SURFACE_WELL; *brush = LP_BRUSH_OPACITY * 0.35f; break;
    case LP_VARIANT_BODY: *top = LP_SURFACE_BODY; *bottom = LP_SURFACE_BODY; *brush = LP_BRUSH_OPACITY * 0.25f; break;
    case LP_VARIANT_FLAT:
    default: *top = LP_SURFACE_WINDOW_TOP; *bottom = LP_SURFACE_WINDOW_BOTTOM; break;
    }
}

lp_surface_motion lp_surface_motion_of(const lp_ctx *ctx) {
    return (lp_surface_motion){ ctx->sheen_x, ctx->tilt, ctx->grain_x, ctx->grain_y, ctx->speed };
}

void lp_surface_paint(cairo_t *cr, lp_rect r, lp_surface_opts o, lp_surface_motion m) {
    lp_color top, bottom;
    float brush;
    lp_surface_stops(o.variant, o.inactive, &top, &bottom, &brush);
    lp_fill_vgradient(cr, r, top, bottom, o.radius);
    /* Speed catches the grain: up to brush.glint more of it at full tilt. */
    lp_draw_brush(cr, r, o.radius, brush * (1 + m.speed * LP_BRUSH_GLINT), m.grain_x, m.grain_y);
    if (o.sheen) {
        float alpha = o.sheen_alpha >= 0 ? o.sheen_alpha : (o.inactive ? LP_SHEEN_ALPHA_INACTIVE : LP_SHEEN_ALPHA);
        lp_draw_sheen(cr, r, o.radius, m.sheen_x, m.tilt_deg, alpha);
    }
    if (o.variant == LP_VARIANT_WELL) lp_draw_inset_shadows(cr, r, o.radius, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
    else if (o.variant != LP_VARIANT_BODY) lp_draw_inset_shadows(cr, r, o.radius, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
}

void lp_surface(lp_ctx *ctx, lp_rect r, lp_surface_opts opts) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    if (!ctx->active_window && opts.variant == LP_VARIANT_TITLEBAR) opts.inactive = 1;
    lp_surface_paint(ctx->cr, r, opts, lp_surface_motion_of(ctx));
}
