#include "maryui/components/lp_controls.h"
#include "maryui/lp_icon.h"

void lp_icon_widget(lp_ctx *ctx, lp_icon icon, float x, float y, float size, lp_color color) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_icon_draw(ctx->cr, icon, x, y, size, 0, color);
}
