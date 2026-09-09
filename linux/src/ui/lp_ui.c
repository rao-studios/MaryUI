#include <math.h>
#include <string.h>

#include "maryui/lp_ui.h"

void lp_ctx_begin(lp_ctx *ctx, enum lp_pass pass, cairo_t *cr, const lp_input *in, lp_rect bounds, double now_ms) {
    ctx->pass = pass;
    ctx->cr = cr;
    if (in) ctx->in = *in;
    else memset(&ctx->in, 0, sizeof ctx->in), ctx->in.mx = ctx->in.my = NAN;
    ctx->bounds = bounds;
    ctx->now_ms = now_ms;
    ctx->next_hot = 0;
    ctx->cursor = LP_CURSOR_ARROW;
    ctx->wants_frame = 0;
    ctx->wants_frame_rect = LP_RECT(0, 0, 0, 0);
    ctx->has_damage = 0;
    ctx->damage = LP_RECT(0, 0, 0, 0);
}

void lp_damage(lp_ctx *ctx, lp_rect r) {
    if (r.w <= 0 || r.h <= 0) return;
    if (!ctx->has_damage) { ctx->damage = r; ctx->has_damage = 1; return; }
    lp_rect a = ctx->damage;
    float x0 = a.x < r.x ? a.x : r.x, y0 = a.y < r.y ? a.y : r.y;
    float x1 = a.x + a.w > r.x + r.w ? a.x + a.w : r.x + r.w, y1 = a.y + a.h > r.y + r.h ? a.y + a.h : r.y + r.h;
    ctx->damage = LP_RECT(x0, y0, x1 - x0, y1 - y0);
}

void lp_ctx_end(lp_ctx *ctx) {
    if (ctx->pass == LP_PASS_EVENT) {
        if (ctx->next_hot != ctx->hot) ctx->dirty = 1;
        ctx->hot = ctx->next_hot;
        if (ctx->in.released & LP_BUTTON_LEFT) {
            if (ctx->active) ctx->dirty = 1;
            ctx->active = 0;
        }
        ctx->in.pressed = ctx->in.released = 0;
        ctx->in.double_click = 0;
        ctx->in.keysym = 0;
        ctx->in.utf8[0] = 0;
        ctx->in.scroll_x = ctx->in.scroll_y = 0;
        ctx->in.drag = 0;
    }
}

lp_id lp_id_hash(const char *s) {
    uint64_t h = 1469598103934665603ull;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ull; }
    return h ? h : 1;
}

lp_id lp_id_index(lp_id base, int index) {
    uint64_t h = base ^ ((uint64_t)(index + 1) * 0x9E3779B97F4A7C15ull);
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    return h ? h : 1;
}

int lp_rect_contains(lp_rect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

int lp_hit(const lp_ctx *ctx, lp_rect r) {
    return !isnan(ctx->in.mx) && lp_rect_contains(r, ctx->in.mx, ctx->in.my);
}

int lp_hot(lp_ctx *ctx, lp_id id, lp_rect r) {
    if (ctx->pass == LP_PASS_EVENT) {
        /* Only the pointer's current position counts; last registered wins (topmost). */
        if (lp_hit(ctx, r)) ctx->next_hot = id;
        return ctx->next_hot == id;
    }
    return ctx->hot == id;
}

int lp_is_active(const lp_ctx *ctx, lp_id id) { return ctx->active == id; }
int lp_is_hot(const lp_ctx *ctx, lp_id id) { return ctx->hot == id; }
void lp_want_frame(lp_ctx *ctx) { lp_want_frame_rect(ctx, ctx->bounds); }

void lp_want_frame_rect(lp_ctx *ctx, lp_rect r) {
    if (ctx->cr) {
        /* Scrolled out of view (or otherwise clipped away): nothing to animate. */
        double x1, y1, x2, y2;
        cairo_clip_extents(ctx->cr, &x1, &y1, &x2, &y2);
        float cx0 = r.x > x1 ? r.x : (float)x1, cy0 = r.y > y1 ? r.y : (float)y1;
        float cx1 = r.x + r.w < x2 ? r.x + r.w : (float)x2, cy1 = r.y + r.h < y2 ? r.y + r.h : (float)y2;
        if (cx1 <= cx0 || cy1 <= cy0) return;
        r = LP_RECT(cx0, cy0, cx1 - cx0, cy1 - cy0);
    }
    if (!ctx->wants_frame) {
        ctx->wants_frame_rect = r;
    } else {
        lp_rect a = ctx->wants_frame_rect;
        float x0 = a.x < r.x ? a.x : r.x, y0 = a.y < r.y ? a.y : r.y;
        float x1 = a.x + a.w > r.x + r.w ? a.x + a.w : r.x + r.w, y1 = a.y + a.h > r.y + r.h ? a.y + a.h : r.y + r.h;
        ctx->wants_frame_rect = LP_RECT(x0, y0, x1 - x0, y1 - y0);
    }
    ctx->wants_frame = 1;
}

int lp_clicked(lp_ctx *ctx, lp_id id, lp_rect r) {
    int hot = lp_hot(ctx, id, r);
    if (ctx->pass != LP_PASS_EVENT) return 0;
    int clicked = 0;
    if (hot && (ctx->in.pressed & LP_BUTTON_LEFT)) {
        ctx->active = id;
        ctx->dirty = 1;
    }
    if (ctx->active == id && (ctx->in.released & LP_BUTTON_LEFT)) {
        if (hot) clicked = 1;
        ctx->dirty = 1;
    }
    return clicked;
}

lp_rect lp_rect_inset(lp_rect r, float dx, float dy) { return LP_RECT(r.x + dx, r.y + dy, r.w - 2 * dx, r.h - 2 * dy); }

lp_rect lp_rect_cut_top(lp_rect *a, float size) {
    lp_rect r = LP_RECT(a->x, a->y, a->w, size);
    a->y += size; a->h -= size;
    return r;
}
lp_rect lp_rect_cut_bottom(lp_rect *a, float size) {
    lp_rect r = LP_RECT(a->x, a->y + a->h - size, a->w, size);
    a->h -= size;
    return r;
}
lp_rect lp_rect_cut_left(lp_rect *a, float size) {
    lp_rect r = LP_RECT(a->x, a->y, size, a->h);
    a->x += size; a->w -= size;
    return r;
}
lp_rect lp_rect_cut_right(lp_rect *a, float size) {
    lp_rect r = LP_RECT(a->x + a->w - size, a->y, size, a->h);
    a->w -= size;
    return r;
}

lp_rect lp_rect_center(lp_rect area, lp_size size) {
    return LP_RECT(area.x + (area.w - size.w) / 2, area.y + (area.h - size.h) / 2, size.w, size.h);
}

static void layout(lp_rect area, float gap, int n, const float *flex, const float *fixed, lp_rect *out, int horizontal) {
    float total = horizontal ? area.w : area.h;
    float used = gap * (n > 0 ? n - 1 : 0), weights = 0;
    for (int i = 0; i < n; i++) {
        float f = flex ? flex[i] : 0;
        if (f > 0) weights += f; else used += fixed ? fixed[i] : 0;
    }
    float free = total - used;
    if (free < 0) free = 0;
    float pos = horizontal ? area.x : area.y;
    for (int i = 0; i < n; i++) {
        float f = flex ? flex[i] : 0;
        float size = f > 0 ? free * f / weights : (fixed ? fixed[i] : 0);
        out[i] = horizontal ? LP_RECT(pos, area.y, size, area.h) : LP_RECT(area.x, pos, area.w, size);
        pos += size + gap;
    }
}

void lp_layout_row(lp_rect area, float gap, int n, const float *flex, const float *fixed, lp_rect *out) { layout(area, gap, n, flex, fixed, out, 1); }
void lp_layout_column(lp_rect area, float gap, int n, const float *flex, const float *fixed, lp_rect *out) { layout(area, gap, n, flex, fixed, out, 0); }
