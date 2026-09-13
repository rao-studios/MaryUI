#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_tokens.h"

int lp_toggle(lp_ctx *ctx, lp_id id, float x, float y, int *checked, int disabled) {
    lp_rect r = LP_RECT(x, y, LP_TOGGLE_W, LP_TOGGLE_H);
    int changed = 0;
    if (!disabled && lp_clicked(ctx, id, r)) { *checked = !*checked; changed = 1; ctx->dirty = 1; }
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return changed;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    cairo_save(cr);
    if (disabled) cairo_push_group(cr);
    if (*checked) lp_fill_vgradient(cr, r, accent.deep, accent.base, LP_RADIUS_PILL);
    else lp_fill_vgradient(cr, r, LP_PLATINUM_5, LP_PLATINUM_4, LP_RADIUS_PILL);
    lp_draw_inset_shadows(cr, r, LP_RADIUS_PILL, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
    if (ctx->focus == id && !disabled) lp_draw_focus_ring(cr, r, LP_RADIUS_PILL, accent.focus_ring, 3);
    /* knob: 16px at (3,3), slid 16px when on, with a small drop shadow */
    float kx = x + 3 + (*checked ? 16 : 0), ky = y + 3;
    static const lp_shadow_layer knob_shadow[] = { { 0, 0, 1, 2, 0, { 0, 0, 0, 0.35f } } };
    lp_draw_outer_shadows(cr, LP_RECT(kx, ky, 16, 16), 8, knob_shadow, 1);
    lp_liquid_bubble(ctx, disabled ? 0 : id, kx + 8, ky + 8, 16, *checked ? LP_TINT_ACCENT : LP_TINT_PLATINUM, 0, *checked ? 0.72f : 0.5f, NULL, 0);
    if (disabled) { cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, 0.55); }
    cairo_restore(cr);
    return changed;
}
