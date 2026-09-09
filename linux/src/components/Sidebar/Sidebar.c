#include <string.h>

#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

lp_rect lp_sidebar(lp_ctx *ctx, lp_rect *area) {
    lp_rect bar = lp_rect_cut_left(area, LP_SIDEBAR_W);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_fill_solid(ctx->cr, bar, LP_SURFACE_SIDEBAR, 0);
        lp_fill_solid(ctx->cr, LP_RECT(bar.x + bar.w - 1, bar.y, 1, bar.h), LP_EDGE_DIVIDER, 0);
    }
    return LP_RECT(bar.x + LP_SPACE_2, bar.y + LP_SPACE_2, bar.w - 2 * LP_SPACE_2, bar.h - 2 * LP_SPACE_2);
}

void lp_sidebar_section(lp_ctx *ctx, lp_rect *cursor, const char *title) {
    if (cursor->y > cursor->h && 0) return;
    lp_rect heading = LP_RECT(cursor->x + LP_SPACE_2, cursor->y, cursor->w - 2 * LP_SPACE_2, 16);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS; st.weight = LP_TEXT_WEIGHT_SEMIBOLD; st.letter_spacing = LP_TEXT_XS * 0.04f;
        st.uppercase = 1; st.color = LP_INK_TERTIARY;
        lp_text_draw(ctx->cr, title, heading, &st, LP_ALIGN_START);
    }
    cursor->y += 16 + LP_SPACE_1;
}

int lp_sidebar_item(lp_ctx *ctx, lp_id id, lp_rect *cursor, lp_icon icon, const char *label, int selected) {
    lp_rect r = LP_RECT(cursor->x, cursor->y, cursor->w, 24);
    cursor->y += 24;
    int clicked = lp_clicked(ctx, id, r);
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return clicked;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    if (selected) {
        lp_fill_vgradient(cr, r, accent.light, accent.base, LP_RADIUS_SM);
        static const lp_shadow_layer top[] = { { 1, 0, 1, 0, 0, { 1, 1, 1, 0.35f } } };
        lp_draw_inset_shadows(cr, r, LP_RADIUS_SM, top, 1);
    } else if (lp_is_hot(ctx, id)) {
        lp_fill_solid(cr, r, LP_RGBA(0, 0, 0, 0.05f), LP_RADIUS_SM);
    }
    float x = r.x + LP_SPACE_2;
    if (icon < LP_ICON_COUNT) { lp_icon_draw(cr, icon, x, r.y + (r.h - 15) / 2, 15, 0, selected ? LP_INK_ON_ACCENT : accent.base); x += 15 + LP_SPACE_2; }
    lp_text_style st = lp_text_style_default();
    st.ellipsize = 1;
    if (selected) { st.color = LP_INK_ON_ACCENT; st.emboss = 1; st.emboss_color = LP_RGBA(0, 0, 0, 0.2f); }
    lp_text_draw(cr, label, LP_RECT(x, r.y, r.x + r.w - LP_SPACE_2 - x, r.h), &st, LP_ALIGN_START);
    return clicked;
}
