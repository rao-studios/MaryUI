/* The drag session (lp_drag.h): what is dragged, and the ghost. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_drag.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

int lp_desktop_drag_begin(lp_desktop *d, const char *window_id, const char *src_dir, const char *const *names, int n, lp_icon icon, int folder) {
    if (n <= 0 || !names) return 0;
    if (d->drag.active) lp_desktop_drag_end(d);
    lp_drag *g = &d->drag;
    memset(g, 0, sizeof *g);
    g->names = calloc((size_t)n, sizeof *g->names);
    if (!g->names) return 0;
    for (int i = 0; i < n; i++) snprintf(g->names[i], sizeof g->names[i], "%s", names[i]);
    g->count = n;
    snprintf(g->source_window, sizeof g->source_window, "%s", window_id ? window_id : "");
    snprintf(g->src_dir, sizeof g->src_dir, "%s", src_dir ? src_dir : "");
    g->icon = icon;
    g->folder = folder;
    if (n == 1) snprintf(g->label, sizeof g->label, "%s", names[0]);
    else snprintf(g->label, sizeof g->label, "%d items", n);
    g->active = 1;
    if (d->on_drag) d->on_drag(d, 1);
    return 1;
}

void lp_desktop_drag_end(lp_desktop *d) {
    if (!d->drag.active) return;
    d->drag.active = 0;
    free(d->drag.names);
    d->drag.names = NULL;
    d->drag.count = 0;
    if (d->on_drag) d->on_drag(d, 0);
}

void lp_drag_ghost(lp_ctx *ctx, lp_rect r, const lp_drag *drag) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    cairo_save(cr);
    cairo_push_group(cr);
    lp_rect ic = LP_RECT(r.x + (r.w - 52) / 2, r.y + 4, 52, 52);
    lp_file_icon_paint(cr, ic, drag->icon < LP_ICON_COUNT ? drag->icon : LP_ICON_DOCUMENT, drag->folder, ctx->settings);
    if (drag->count > 1) {
        /* a red count badge on the tile's corner, like the Dock's */
        char n[16];
        snprintf(n, sizeof n, "%d", drag->count);
        lp_text_style bs = lp_text_style_default();
        bs.size_px = LP_TEXT_XS; bs.weight = LP_TEXT_WEIGHT_SEMIBOLD; bs.color = LP_INK_ON_ACCENT;
        lp_size ts = lp_text_measure(cr, n, &bs);
        float bw = ts.w + 10 > 18 ? ts.w + 10 : 18;
        lp_rect badge = LP_RECT(ic.x + ic.w - bw + 6, ic.y - 6, bw, 18);
        lp_fill_solid(cr, badge, LP_RGBA(0.86f, 0.2f, 0.18f, 1), LP_RADIUS_PILL);
        lp_text_draw(cr, n, badge, &bs, LP_ALIGN_CENTER);
    }
    if (drag->copy) {
        /* a green "+" badge: the drop copies */
        lp_rect badge = LP_RECT(ic.x - 6, ic.y + ic.h - 12, 18, 18);
        lp_fill_solid(cr, badge, LP_RGBA(0.24f, 0.62f, 0.3f, 1), LP_RADIUS_PILL);
        lp_icon_draw(cr, LP_ICON_PLUS, badge.x + 3, badge.y + 3, 12, 2.4f, LP_INK_ON_ACCENT);
    }
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM; st.ellipsize = 1; st.color = LP_INK_ON_ACCENT; st.emboss = 1; st.emboss_color = LP_RGBA(0, 0, 0, 0.35f);
    lp_size ls = lp_text_measure(cr, drag->label, &st);
    float lw = ls.w + 12 < r.w ? ls.w + 12 : r.w;
    lp_rect pill = LP_RECT(r.x + (r.w - lw) / 2, ic.y + ic.h + 6, lw, 18);
    lp_fill_solid(cr, pill, accent.base, LP_RADIUS_PILL);
    lp_text_draw(cr, drag->label, lp_rect_inset(pill, 6, 0), &st, LP_ALIGN_CENTER);
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, 0.8);
    cairo_restore(cr);
}
