#include <math.h>
#include <string.h>

#include "maryui/components/lp_menu_bar.h"
#include "maryui/components/lp_monogram.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static const lp_shadow_layer BAR_OUTER[] = {
    { 0, 0, 1, 0, 0, { 0, 0, 0, 0.32f } },   /* 0 1px 0 edge.hairline */
    { 0, 0, 2, 8, 0, { 0, 0, 0, 0.18f } },   /* 0 2px 8px rgba(0,0,0,.18) */
};

void lp_menu_bar(lp_ctx *ctx, lp_rect r, const lp_menu_bar_model *model, lp_menu_bar_result *out) {
    lp_menu_bar_result res;
    memset(&res, 0, sizeof res);
    res.pressed = -1;
    res.hovered = -1;
    lp_accent accent = lp_settings_accent(ctx->settings);
    cairo_t *cr = ctx->pass == LP_PASS_DRAW ? ctx->cr : NULL;

    /* Layout: padding 0 8px; triggers height 100% - 4px with 2px margins. */
    lp_text_style label = lp_text_style_default();
    label.weight = LP_TEXT_WEIGHT_MEDIUM;
    label.emboss = 1;
    float x = r.x + LP_SPACE_2;
    for (int i = 0; i < model->count && i < LP_MENU_BAR_MAX; i++) {
        float pad = i == 0 ? LP_SPACE_3 : LP_SPACE_2;
        float w = i == 0 ? 18 + 2 * pad : 0;
        if (i > 0) {
            lp_size sz = cr ? lp_text_measure(cr, model->labels[i], &label) : (lp_size){ (float)strlen(model->labels[i]) * 7.5f, 16 };
            w = ceilf(sz.w) + 2 * pad;
        }
        res.triggers[i] = LP_RECT(x, r.y + 2, w, r.h - 4);
        x += w;
    }
    for (int i = 0; i < model->count; i++) {
        lp_id id = lp_id_index(LP_ID("menubar.trigger"), i);
        if (lp_hot(ctx, id, res.triggers[i])) res.hovered = i;
        if (ctx->pass == LP_PASS_EVENT && res.hovered == i && (ctx->in.pressed & LP_BUTTON_LEFT)) {
            res.pressed = i;
            ctx->dirty = 1;
        }
    }
    if (out) *out = res;
    if (!cr) return;

    /* Surface bar with the sheen pinned at 0.35 (the room light), then the shadows below it. */
    lp_draw_outer_shadows(cr, r, 0, BAR_OUTER, 2);
    lp_surface_paint(cr, r, (lp_surface_opts){ .variant = LP_VARIANT_BAR, .radius = 0, .sheen = 1, .sheen_alpha = -1 }, LP_SHEEN_LIGHT_X, 0);

    for (int i = 0; i < model->count; i++) {
        lp_rect t = res.triggers[i];
        int open = model->open_index == i;
        if (open) {
            lp_fill_vgradient(cr, t, accent.light, accent.base, LP_RADIUS_XS);
        }
        if (i == 0) {
            lp_rect box = LP_RECT(t.x + (t.w - 18) / 2, t.y + (t.h - 18) / 2, 18, 18);
            lp_monogram_paint(cr, box, LP_MONOGRAM_FLAT, open ? LP_INK_ON_ACCENT : LP_INK_PRIMARY);
        } else {
            lp_text_style s = label;
            if (open) { s.color = LP_INK_ON_ACCENT; s.emboss_color = LP_RGBA(0, 0, 0, 0.2f); }
            lp_text_draw(cr, model->labels[i], t, &s, LP_ALIGN_CENTER);
        }
    }

    /* Right: status, then the clock (13px/500, tabular). */
    lp_text_style clock = label;
    clock.tabular_nums = 1;
    lp_rect right = LP_RECT(r.x, r.y, r.w - LP_SPACE_2, r.h);
    if (model->clock) {
        lp_size sz = lp_text_measure(cr, model->clock, &clock);
        lp_rect cbox = LP_RECT(right.x + right.w - sz.w, r.y, sz.w, r.h);
        lp_text_draw(cr, model->clock, cbox, &clock, LP_ALIGN_START);
        right.w -= sz.w + LP_SPACE_3;
    }
    if (model->status) {
        lp_size sz = lp_text_measure(cr, model->status, &label);
        lp_rect sbox = LP_RECT(right.x + right.w - sz.w, r.y, sz.w, r.h);
        lp_text_draw(cr, model->status, sbox, &label, LP_ALIGN_START);
    }
}
