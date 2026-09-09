/* Rect math for the window manager (web/src/lib/geometry.ts): clamping
 * windows to the desktop, resizing from any of the eight handles, and the
 * FLIP transform that lets a window appear to travel between two rects. */
#ifndef MARYUI_LP_GEOMETRY_H
#define MARYUI_LP_GEOMETRY_H

#include "maryui/lp_types.h"

enum lp_resize_handle { LP_HANDLE_N, LP_HANDLE_NE, LP_HANDLE_E, LP_HANDLE_SE, LP_HANDLE_S, LP_HANDLE_SW, LP_HANDLE_W, LP_HANDLE_NW, LP_HANDLE_COUNT };
extern const char *const LP_RESIZE_HANDLE_NAMES[LP_HANDLE_COUNT];

float lp_clamp(float n, float lo, float hi);
int lp_rects_equal(lp_rect a, lp_rect b);

/* Keeps a window grabbable: its top edge never rises above the desktop, at
 * least min_visible px stay on screen horizontally, and the title bar never
 * sinks below the bottom edge. Defaults: min_visible 40, title_height 28. */
lp_rect lp_clamp_to_bounds(lp_rect rect, lp_rect bounds, float min_visible, float title_height);

/* Drags `handle` by (dx, dy): the opposite edge stays pinned, the minimum
 * size holds, and no edge leaves `bounds` when one is given. */
lp_rect lp_resize_from_handle(lp_rect rect, enum lp_resize_handle handle, float dx, float dy, lp_size min, const lp_rect *bounds);

/* The rect a zoomed window occupies: the whole desktop. */
lp_rect lp_fit_rect(lp_rect bounds);
/* Shrinks a rect to fit inside `bounds`, keeping its position when possible. */
lp_rect lp_constrain_rect(lp_rect rect, lp_rect bounds, lp_size min);

typedef struct lp_flip { float tx, ty, sx, sy; } lp_flip;
/* With the origin at 0 0, this transform applied to an element laid out at
 * `to` makes it appear exactly at `from`. */
lp_flip lp_flip_transform(lp_rect from, lp_rect to);
lp_point lp_rect_center_point(lp_rect rect);

#endif
