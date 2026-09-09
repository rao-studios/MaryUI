#include <math.h>

#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_tokens.h"

#define MAX_DEPTH 8
struct scroll_frame { lp_rect viewport; lp_size content; lp_scroll_state *state; lp_id id; };
static struct scroll_frame stack[MAX_DEPTH];
static int depth = 0;

lp_rect lp_scroll_begin(lp_ctx *ctx, lp_id id, lp_rect viewport, lp_size content, lp_scroll_state *state) {
    float max_y = fmaxf(0, content.h - viewport.h), max_x = fmaxf(0, content.w - viewport.w);
    if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, viewport) && (ctx->in.scroll_y != 0 || ctx->in.scroll_x != 0)) {
        state->y += ctx->in.scroll_y;
        state->x += ctx->in.scroll_x;
        ctx->dirty = 1;
    }
    if (state->y > max_y) state->y = max_y;
    if (state->y < 0) state->y = 0;
    if (state->x > max_x) state->x = max_x;
    if (state->x < 0) state->x = 0;
    if (depth < MAX_DEPTH) stack[depth++] = (struct scroll_frame){ viewport, content, state, id };
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        cairo_save(ctx->cr);
        cairo_rectangle(ctx->cr, viewport.x, viewport.y, viewport.w, viewport.h);
        cairo_clip(ctx->cr);
    }
    return LP_RECT(viewport.x - state->x, viewport.y - state->y, content.w, content.h);
}

void lp_scroll_end(lp_ctx *ctx) {
    if (depth == 0) return;
    lp_rect v = stack[depth - 1].viewport;
    lp_size c = stack[depth - 1].content;
    lp_scroll_state *s = stack[depth - 1].state;
    depth--;
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    /* the thin platinum thumb: 10px lane, 2px transparent border, platinum.4 → platinum.6 */
    if (c.h > v.h + 1) {
        float lane = 10, track_h = v.h;
        float thumb_h = fmaxf(20, track_h * v.h / c.h);
        float thumb_y = v.y + (track_h - thumb_h) * (s->y / fmaxf(1, c.h - v.h));
        lp_fill_vgradient(cr, LP_RECT(v.x + v.w - lane + 2, thumb_y + 2, lane - 4, thumb_h - 4), LP_PLATINUM_4, LP_PLATINUM_6, LP_RADIUS_PILL);
    }
    if (c.w > v.w + 1) {
        float lane = 10, track_w = v.w;
        float thumb_w = fmaxf(20, track_w * v.w / c.w);
        float thumb_x = v.x + (track_w - thumb_w) * (s->x / fmaxf(1, c.w - v.w));
        lp_fill_vgradient(cr, LP_RECT(thumb_x + 2, v.y + v.h - lane + 2, thumb_w - 4, lane - 4), LP_PLATINUM_4, LP_PLATINUM_6, LP_RADIUS_PILL);
    }
    cairo_restore(cr);
}
