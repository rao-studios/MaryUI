#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_tokens.h"

lp_rect lp_toolbar(lp_ctx *ctx, lp_rect *area) {
    lp_rect bar = lp_rect_cut_top(area, LP_TOOLBAR_H);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_surface_paint(ctx->cr, bar, (lp_surface_opts){ .variant = LP_VARIANT_FLAT, .radius = 0 }, lp_surface_motion_of(ctx));
        static const lp_shadow_layer top[] = { { 1, 0, 1, 0, 0, { 1, 1, 1, 0.78f } } };
        lp_draw_inset_shadows(ctx->cr, bar, 0, top, 1);
        lp_draw_hairline(ctx->cr, bar, LP_EDGE_BOTTOM, LP_EDGE_HAIRLINE);
    }
    return LP_RECT(bar.x + LP_SPACE_3, bar.y, bar.w - 2 * LP_SPACE_3, bar.h);
}
