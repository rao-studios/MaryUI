#include <math.h>
#include <string.h>

#include "maryui/components/lp_controls.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

lp_size lp_checkbox_measure(lp_ctx *ctx, const char *label) {
    lp_text_style st = lp_text_style_default();
    float w = 15;
    if (label && *label) w += LP_SPACE_2 + (ctx->cr ? lp_text_measure(ctx->cr, label, &st).w : 7.0f * (float)strlen(label));
    return (lp_size){ w, 18 };
}

int lp_checkbox(lp_ctx *ctx, lp_id id, float x, float y, int *checked, const char *label, int disabled) {
    lp_size size = lp_checkbox_measure(ctx, label);
    lp_rect r = LP_RECT(x, y, size.w, size.h);
    int changed = 0;
    if (!disabled && lp_clicked(ctx, id, r)) { *checked = !*checked; changed = 1; ctx->dirty = 1; }
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return changed;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_rect box = LP_RECT(x, y + (size.h - 15) / 2, 15, 15);
    static const lp_shadow_layer hair[] = { { 0, 0, 0, 0, 1, { 0, 0, 0, 0.32f } } };
    float radius = lp_radius_flex(ctx, LP_RADIUS_XS);
    lp_draw_outer_shadows(cr, box, radius, hair, 1);
    if (*checked) lp_fill_vgradient(cr, box, accent.light, accent.base, radius);
    else lp_fill_vgradient(cr, box, LP_SURFACE_RAISED_TOP, LP_SURFACE_RAISED_BOTTOM, radius);
    lp_draw_inset_shadows(cr, box, radius, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
    if (ctx->focus == id && !disabled) lp_draw_focus_ring(cr, box, radius, accent.focus_ring, 3);
    if (*checked) lp_icon_draw(cr, LP_ICON_CHECK, box.x + 2, box.y + 2, 11, 2.6f, LP_INK_ON_ACCENT);
    if (label && *label) {
        lp_text_style st = lp_text_style_default();
        st.color = disabled ? LP_INK_DISABLED : LP_INK_PRIMARY;
        lp_text_draw(cr, label, LP_RECT(x + 15 + LP_SPACE_2, y, size.w - 15 - LP_SPACE_2 + 2, size.h), &st, LP_ALIGN_START);
    }
    return changed;
}
