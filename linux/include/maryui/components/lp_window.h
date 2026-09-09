/* Window chrome — the brushed frame around a title bar and a body: the
 * Surface, the emboss, the window shadow (focused or not), and the eight
 * resize handles. The window manager (lp_wm) owns the state; this paints it. */
#ifndef MARYUI_LP_WINDOW_H
#define MARYUI_LP_WINDOW_H

#include "maryui/components/lp_title_bar.h"
#include "maryui/lp_geometry.h"
#include "maryui/lp_ui.h"
#include "maryui/lp_wm.h"

typedef struct lp_window_view {
    const char *title;
    int focused;
    enum lp_window_state state;
    int resizable;
    lp_rect rect;          /* the window's rect in chrome coordinates (the chrome may carry a shadow margin) */
} lp_window_view;

/* How far the window shadow reaches on each side (sizes the chrome buffer). */
void lp_window_shadow_margins(int *left, int *top, int *right, int *bottom);
/* Paints frame + title bar; returns the body rect (empty when shaded). The title bar's result is written to `bar`. */
lp_rect lp_window_chrome(lp_ctx *ctx, const lp_window_view *view, lp_title_bar_result *bar);
/* The eight handle zones around `rect` (chrome coordinates). */
void lp_window_handles(lp_rect rect, lp_rect out[LP_HANDLE_COUNT]);
/* The cursor for a handle. */
enum lp_cursor_shape lp_window_handle_cursor(enum lp_resize_handle h);

#endif
