#include <string.h>

#include "maryui/components/lp_surface.h"
#include "maryui/components/lp_title_bar.h"
#include "maryui/components/lp_traffic_lights.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

void lp_title_bar(lp_ctx *ctx, lp_rect r, const lp_title_bar_model *m, lp_title_bar_result *out) {
    lp_title_bar_result res;
    memset(&res, 0, sizeof res);
    lp_size lights = lp_traffic_lights_size();
    float lx = r.x + LP_SPACE_2, ly = r.y + (r.h - lights.h) / 2;

    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_surface_opts o = { .variant = LP_VARIANT_TITLEBAR, .radius = 0, .sheen = 1, .sheen_alpha = -1, .inactive = !m->active };
        cairo_save(ctx->cr);
        lp_path_rrect4(ctx->cr, r, m->radius_top, m->radius_top, m->radius_bottom, m->radius_bottom);
        cairo_clip(ctx->cr);
        lp_surface_paint(ctx->cr, r, o, ctx->sheen_x, ctx->tilt);
        lp_draw_inset_shadows(ctx->cr, r, 0, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
        cairo_restore(ctx->cr);
        /* the hairline below the bar */
        if (!m->shaded) lp_draw_hairline(ctx->cr, r, LP_EDGE_BOTTOM, LP_EDGE_HAIRLINE);
        /* title: absolutely centred between 84px insets */
        lp_text_style st = lp_text_style_default();
        st.weight = LP_TEXT_WEIGHT_SEMIBOLD;
        st.emboss = 1;
        st.ellipsize = 1;
        st.color = m->active ? LP_INK_PRIMARY : LP_INK_TERTIARY;
        lp_text_draw(ctx->cr, m->title ? m->title : "", LP_RECT(r.x + 84, r.y, r.w - 168, r.h), &st, LP_ALIGN_CENTER);
    }
    lp_traffic_result lights_res;
    lp_traffic_lights(ctx, lx, ly, m->active, m->shaded, m->zoomed, &lights_res);
    res.close = lights_res.close;
    res.shade = lights_res.shade;
    res.zoom = lights_res.zoom;
    res.hovered = lp_hit(ctx, r);
    if (ctx->pass == LP_PASS_EVENT && res.hovered && !lights_res.hovered) {
        if (ctx->in.double_click) res.double_click = 1;
        else if (ctx->in.pressed & LP_BUTTON_LEFT) res.drag_start = 1;
    }
    if (out) *out = res;
}
