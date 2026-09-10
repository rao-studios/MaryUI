#include <math.h>

#include "maryui/components/lp_surface.h"
#include "maryui/components/lp_window.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_shadow.h"
#include "maryui/lp_tokens.h"

void lp_window_shadow_margins(int *left, int *top, int *right, int *bottom) {
    int l1, t1, r1, b1, l2, t2, r2, b2;
    lp_shadow_extents(LP_SHADOW_WINDOW, LP_SHADOW_WINDOW_COUNT, &l1, &t1, &r1, &b1);
    lp_shadow_extents(LP_SHADOW_WINDOW_FOCUSED, LP_SHADOW_WINDOW_FOCUSED_COUNT, &l2, &t2, &r2, &b2);
    if (left) *left = (l1 > l2 ? l1 : l2) + 2;
    if (top) *top = (t1 > t2 ? t1 : t2) + 2;
    if (right) *right = (r1 > r2 ? r1 : r2) + 2;
    if (bottom) *bottom = (b1 > b2 ? b1 : b2) + 2;
}

lp_rect lp_window_chrome(lp_ctx *ctx, const lp_window_view *v, lp_title_bar_result *bar) {
    int shaded = v->state == LP_WIN_SHADED, zoomed = v->state == LP_WIN_ZOOMED;
    /* A moving window's corners are four independent springs (lp_radius.h); a
     * zoomed one is flush with the desktop and has none. A live corner is
     * clamped to radius.window-min..max, so all-zero means the context never
     * carried any — an offline render, say — and the resting radius applies. */
    lp_corners live = ctx->corners;
    if (live.tl <= 0 && live.tr <= 0 && live.br <= 0 && live.bl <= 0) live = lp_rest_corners(LP_RADIUS_WINDOW);
    lp_corners radii = zoomed ? lp_rest_corners(0) : live;
    lp_rect frame = v->rect;
    if (shaded) frame.h = LP_TITLE_HEIGHT;
    lp_rect title = LP_RECT(frame.x, frame.y, frame.w, LP_TITLE_HEIGHT);
    lp_rect body = shaded ? LP_RECT(frame.x, frame.y + LP_TITLE_HEIGHT, frame.w, 0) : LP_RECT(frame.x, frame.y + LP_TITLE_HEIGHT, frame.w, frame.h - LP_TITLE_HEIGHT);

    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        cairo_t *cr = ctx->cr;
        /* The shadow keeps the resting radius. Feeding it the live corners
         * meant a new sprite — a 56px-blur render — on every frame of a drag,
         * and under a 56px blur the difference is invisible anyway. */
        float shadow_radius = zoomed ? 0 : LP_RADIUS_WINDOW;
        if (v->focused) lp_draw_shadow_9slice(cr, frame, shadow_radius, LP_SHADOW_WINDOW_FOCUSED, LP_SHADOW_WINDOW_FOCUSED_COUNT);
        else lp_draw_shadow_9slice(cr, frame, shadow_radius, LP_SHADOW_WINDOW, LP_SHADOW_WINDOW_COUNT);
        if (!shaded) {
            cairo_save(cr);
            lp_path_rrect4(cr, frame, radii.tl, radii.tr, radii.br, radii.bl);
            cairo_clip(cr);
            lp_surface_paint(cr, frame, (lp_surface_opts){ .variant = LP_VARIANT_FLAT, .radius = 0, .sheen = 0 }, lp_surface_motion_of(ctx));
            cairo_restore(cr);
        }
    }
    lp_title_bar_model m = { .title = v->title, .active = v->focused, .shaded = shaded, .zoomed = zoomed,
        .radii = { radii.tl, radii.tr, shaded ? radii.br : 0, shaded ? radii.bl : 0 } };
    lp_title_bar(ctx, title, &m, bar);
    return body;
}

void lp_window_handles(lp_rect r, lp_rect out[LP_HANDLE_COUNT]) {
    float g = LP_SIZE_RESIZE_GRIP, c = LP_SIZE_RESIZE_CORNER;
    out[LP_HANDLE_N] = LP_RECT(r.x + c, r.y - g / 2, r.w - 2 * c, g);
    out[LP_HANDLE_S] = LP_RECT(r.x + c, r.y + r.h - g / 2, r.w - 2 * c, g);
    out[LP_HANDLE_W] = LP_RECT(r.x - g / 2, r.y + c, g, r.h - 2 * c);
    out[LP_HANDLE_E] = LP_RECT(r.x + r.w - g / 2, r.y + c, g, r.h - 2 * c);
    out[LP_HANDLE_NW] = LP_RECT(r.x - 3, r.y - 3, c, c);
    out[LP_HANDLE_NE] = LP_RECT(r.x + r.w - c + 3, r.y - 3, c, c);
    out[LP_HANDLE_SW] = LP_RECT(r.x - 3, r.y + r.h - c + 3, c, c);
    out[LP_HANDLE_SE] = LP_RECT(r.x + r.w - c + 3, r.y + r.h - c + 3, c, c);
}

enum lp_cursor_shape lp_window_handle_cursor(enum lp_resize_handle h) {
    switch (h) {
    case LP_HANDLE_N: case LP_HANDLE_S: return LP_CURSOR_NS;
    case LP_HANDLE_E: case LP_HANDLE_W: return LP_CURSOR_EW;
    case LP_HANDLE_NW: case LP_HANDLE_SE: return LP_CURSOR_NWSE;
    default: return LP_CURSOR_NESW;
    }
}
