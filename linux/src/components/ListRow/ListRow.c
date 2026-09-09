#include <math.h>
#include <stdio.h>

#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

void lp_list_columns(float width, int n, float *xs, float *ws) {
    /* grid-template-columns: minmax(160px, 2fr) repeat(n, minmax(90px, 1fr)); gap space.3; padding 0 space.3 */
    float inner = width - 2 * LP_SPACE_3 - LP_SPACE_3 * n;
    float unit = inner / (2 + n);
    float name = fmaxf(160, 2 * unit), col = fmaxf(90, unit);
    if (name + col * n > inner) { col = fmaxf(60, (inner - name) / (n > 0 ? n : 1)); }
    xs[0] = LP_SPACE_3;
    ws[0] = name;
    for (int i = 1; i <= n; i++) { xs[i] = xs[i - 1] + ws[i - 1] + LP_SPACE_3; ws[i] = col; }
}

int lp_list_header(lp_ctx *ctx, lp_id id, lp_rect r, const char *const *columns, int n, int sort_col, int descending) {
    float xs[16], ws[16];
    lp_list_columns(r.w, n - 1, xs, ws);
    int clicked = -1;
    for (int i = 0; i < n && i < 16; i++) {
        lp_rect cell = LP_RECT(r.x + xs[i] - LP_SPACE_2, r.y, ws[i] + LP_SPACE_2, r.h);
        if (lp_clicked(ctx, lp_id_index(id, i), cell)) clicked = i;
    }
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return clicked;
    cairo_t *cr = ctx->cr;
    lp_fill_vgradient(cr, r, LP_PLATINUM_1, LP_PLATINUM_2, 0);
    lp_draw_hairline(cr, r, LP_EDGE_BOTTOM, LP_EDGE_DIVIDER);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS; st.weight = LP_TEXT_WEIGHT_MEDIUM; st.color = LP_INK_SECONDARY; st.ellipsize = 1;
    for (int i = 0; i < n && i < 16; i++) {
        if (i == sort_col) {
            char label[80];
            snprintf(label, sizeof label, "%s %s", columns[i], descending ? "▼" : "▲");
            lp_text_draw(cr, label, LP_RECT(r.x + xs[i], r.y, ws[i], r.h), &st, LP_ALIGN_START);
        } else {
            lp_text_draw(cr, columns[i], LP_RECT(r.x + xs[i], r.y, ws[i], r.h), &st, LP_ALIGN_START);
        }
    }
    return clicked;
}

int lp_list_row(lp_ctx *ctx, lp_id id, lp_rect r, lp_icon icon, const char *name, const char *const *columns, int n, int selected, int even) {
    int result = lp_clicked(ctx, id, r) ? 1 : 0;
    if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, r) && ctx->in.double_click) result = 2;
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return result;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    if (selected) lp_fill_vgradient(cr, r, accent.light, accent.base, 0);
    else if (even) lp_fill_solid(cr, r, LP_RGBA(0, 0, 0, 0.035f), 0);
    float xs[16], ws[16];
    lp_list_columns(r.w, n, xs, ws);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM; st.ellipsize = 1;
    if (selected) { st.color = LP_INK_ON_ACCENT; st.emboss = 1; st.emboss_color = LP_RGBA(0, 0, 0, 0.2f); }
    float x = r.x + xs[0];
    if (icon < LP_ICON_COUNT) { lp_icon_draw(cr, icon, x, r.y + (r.h - 14) / 2, 14, 0, selected ? LP_INK_ON_ACCENT : accent.base); x += 14 + LP_SPACE_2; }
    lp_text_draw(cr, name, LP_RECT(x, r.y, r.x + xs[0] + ws[0] - x, r.h), &st, LP_ALIGN_START);
    lp_text_style cs = st;
    if (!selected) cs.color = LP_INK_SECONDARY;
    for (int i = 0; i < n && i + 1 < 16; i++) lp_text_draw(cr, columns[i], LP_RECT(r.x + xs[i + 1], r.y, ws[i + 1], r.h), &cs, LP_ALIGN_START);
    return result;
}
