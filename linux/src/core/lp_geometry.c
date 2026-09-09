#include <math.h>

#include "maryui/lp_geometry.h"

const char *const LP_RESIZE_HANDLE_NAMES[LP_HANDLE_COUNT] = { "n", "ne", "e", "se", "s", "sw", "w", "nw" };

float lp_clamp(float n, float lo, float hi) { return n < lo ? lo : (n > hi ? hi : n); }

int lp_rects_equal(lp_rect a, lp_rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

lp_rect lp_clamp_to_bounds(lp_rect rect, lp_rect bounds, float min_visible, float title_height) {
    if (min_visible <= 0) min_visible = 40;
    if (title_height <= 0) title_height = 28;
    float x = lp_clamp(rect.x, bounds.x - rect.w + min_visible, bounds.x + bounds.w - min_visible);
    float y = lp_clamp(rect.y, bounds.y, fmaxf(bounds.y, bounds.y + bounds.h - title_height));
    rect.x = x;
    rect.y = y;
    return rect;
}

static int has_w(enum lp_resize_handle h) { return h == LP_HANDLE_W || h == LP_HANDLE_NW || h == LP_HANDLE_SW; }
static int has_e(enum lp_resize_handle h) { return h == LP_HANDLE_E || h == LP_HANDLE_NE || h == LP_HANDLE_SE; }
static int has_n(enum lp_resize_handle h) { return h == LP_HANDLE_N || h == LP_HANDLE_NE || h == LP_HANDLE_NW; }
static int has_s(enum lp_resize_handle h) { return h == LP_HANDLE_S || h == LP_HANDLE_SE || h == LP_HANDLE_SW; }

lp_rect lp_resize_from_handle(lp_rect rect, enum lp_resize_handle handle, float dx, float dy, lp_size min, const lp_rect *bounds) {
    float left = rect.x, top = rect.y, right = rect.x + rect.w, bottom = rect.y + rect.h;
    if (has_w(handle)) left = fminf(left + dx, right - min.w);
    if (has_e(handle)) right = fmaxf(right + dx, left + min.w);
    if (has_n(handle)) top = fminf(top + dy, bottom - min.h);
    if (has_s(handle)) bottom = fmaxf(bottom + dy, top + min.h);
    if (bounds) {
        left = fmaxf(left, bounds->x);
        top = fmaxf(top, bounds->y);
        right = fminf(right, bounds->x + bounds->w);
        bottom = fminf(bottom, bounds->y + bounds->h);
        if (right - left < min.w) {
            if (has_w(handle)) left = right - min.w; else right = left + min.w;
        }
        if (bottom - top < min.h) {
            if (has_n(handle)) top = bottom - min.h; else bottom = top + min.h;
        }
    }
    return LP_RECT(left, top, right - left, bottom - top);
}

lp_rect lp_fit_rect(lp_rect bounds) { return bounds; }

lp_rect lp_constrain_rect(lp_rect rect, lp_rect bounds, lp_size min) {
    float w = lp_clamp(rect.w, fminf(min.w, bounds.w), bounds.w);
    float h = lp_clamp(rect.h, fminf(min.h, bounds.h), bounds.h);
    float x = lp_clamp(rect.x, bounds.x, fmaxf(bounds.x, bounds.x + bounds.w - w));
    float y = lp_clamp(rect.y, bounds.y, fmaxf(bounds.y, bounds.y + bounds.h - h));
    return LP_RECT(x, y, w, h);
}

lp_flip lp_flip_transform(lp_rect from, lp_rect to) {
    return (lp_flip){ from.x - to.x, from.y - to.y, to.w == 0 ? 1 : from.w / to.w, to.h == 0 ? 1 : from.h / to.h };
}

lp_point lp_rect_center_point(lp_rect rect) { return (lp_point){ rect.x + rect.w / 2, rect.y + rect.h / 2 }; }
