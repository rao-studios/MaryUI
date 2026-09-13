/* AboutApp — the platinum monogram, the system's name, and its version. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/components/lp_monogram.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"
#include "maryui/maryui.h"

static void about_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    /* column, centred, gap space.1, padding space.4 */
    float mark = 150, gap = LP_SPACE_1;
    lp_text_style title = lp_text_style_default();
    title.font = LP_FONT_DISPLAY; title.size_px = LP_TEXT_XL; title.weight = LP_TEXT_WEIGHT_BOLD; title.emboss = 1;
    lp_text_style subtitle = lp_text_style_default();
    subtitle.size_px = LP_TEXT_SM; subtitle.color = LP_INK_TERTIARY;
    lp_text_style blurb = lp_text_style_default();
    blurb.size_px = LP_TEXT_SM; blurb.color = LP_INK_SECONDARY;
    char sub[160];
    snprintf(sub, sizeof sub, "%s · Liquid Platinum %s", d->branding.name, lp_version());
    float th = lp_text_measure(cr, "Liquid Platinum", &title).h;
    float sh = lp_text_measure(cr, sub, &subtitle).h;
    float bh = sh * 2 + 4;
    float total = mark + LP_SPACE_2 + gap + th + gap + sh + LP_SPACE_2 + bh + LP_SPACE_3 + 14;
    float y = body.y + (body.h - total) / 2;
    lp_monogram_paint(cr, LP_RECT(body.x + (body.w - mark) / 2, y, mark, mark), LP_MONOGRAM_PLATINUM, LP_INK_PRIMARY);
    y += mark + LP_SPACE_2 + gap;
    lp_text_draw(cr, "Liquid Platinum", LP_RECT(body.x, y, body.w, th), &title, LP_ALIGN_CENTER);
    y += th + gap;
    lp_text_draw(cr, sub, LP_RECT(body.x, y, body.w, sh), &subtitle, LP_ALIGN_CENTER);
    y += sh + LP_SPACE_2;
    lp_text_draw(cr, "Brushed platinum that moves like liquid.", LP_RECT(body.x, y, body.w, sh), &blurb, LP_ALIGN_CENTER);
    lp_text_draw(cr, "Every surface is a token, every window a physics target.", LP_RECT(body.x, y + sh + 2, body.w, sh), &blurb, LP_ALIGN_CENTER);
    y += bh + LP_SPACE_3;
    static const enum lp_bubble_tint tints[4] = { LP_TINT_CLOSE, LP_TINT_MINIMIZE, LP_TINT_ZOOM, LP_TINT_ACCENT };
    static const float phases[4] = { 0, 1.5f, 3, 4.5f };
    float row = 4 * 14 + 3 * LP_SPACE_2, x = body.x + (body.w - row) / 2;
    for (int i = 0; i < 4; i++) {
        lp_liquid_bubble(ctx, 0, x + 7, y + 7, 14, tints[i], phases[i], -1, NULL, 0);
        x += 14 + LP_SPACE_2;
    }
}

const lp_app lp_app_about = {
    .id = "about", .title = "About Liquid Platinum", .name = "About", .icon = LP_ICON_INFO,
    .default_rect = { 360, 180, 380, 300 }, .min_size = { 380, 300 }, .singleton = 1, .resizable = 0,
    .create = NULL, .paint = about_paint, .destroy = NULL,
};
