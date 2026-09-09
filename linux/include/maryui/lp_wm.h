/* The window manager's pure state machine (web/src/desktop/wm/reducer.ts).
 * Invariants: focusing a window gives it a strictly increasing z and leaves
 * every other record untouched; a moved window keeps its title bar reachable
 * inside the bounds; resizing respects the minimum size; zooming remembers
 * the previous rect and restores it exactly; closing hands focus to the
 * top-most remaining window. Records live in insertion order. */
#ifndef MARYUI_LP_WM_H
#define MARYUI_LP_WM_H

#include <stdint.h>

#include "maryui/lp_types.h"

#define LP_WM_MAX_WINDOWS 64
#define LP_TITLE_HEIGHT 28.0f
#define LP_DEFAULT_MIN_SIZE ((lp_size){ 240, 160 })

enum lp_window_state { LP_WIN_NORMAL, LP_WIN_SHADED, LP_WIN_ZOOMED };

typedef struct lp_window_record {
    char id[12];          /* "w1", "w2", … */
    char app_id[32];
    char title[128];
    lp_rect rect;         /* layout when normal or shaded; the pre-zoom rect is prev_rect while zoomed */
    lp_rect prev_rect;
    int has_prev;
    enum lp_window_state state;
    int z;
    lp_size min_size;
    int resizable;
    void *user;           /* the host's per-window object; the reducer never touches it */
} lp_window_record;

typedef struct lp_wm_state {
    lp_window_record windows[LP_WM_MAX_WINDOWS];
    int count;            /* insertion order */
    int focused;          /* index into windows, or -1 */
    int next_z, next_id;
    lp_rect bounds;       /* the desktop area: the output minus the menu bar */
} lp_wm_state;

/* OPEN's spec. NAN rect fields and zero sizes take the defaults. */
typedef struct lp_open_spec {
    const char *app_id;
    const char *title;
    lp_rect rect;
    lp_size min_size;
    int resizable;
    int singleton;        /* reuse an existing window of this app */
} lp_open_spec;
lp_open_spec lp_open_spec_default(const char *app_id, const char *title);

enum lp_wm_action_type {
    LP_WM_OPEN, LP_WM_CLOSE, LP_WM_FOCUS, LP_WM_FOCUS_NEXT, LP_WM_MOVE, LP_WM_RESIZE,
    LP_WM_TOGGLE_SHADE, LP_WM_TOGGLE_ZOOM, LP_WM_SET_TITLE, LP_WM_SET_BOUNDS,
};

typedef struct lp_wm_action {
    enum lp_wm_action_type type;
    const char *id;       /* CLOSE, FOCUS, MOVE, RESIZE, TOGGLE_*, SET_TITLE */
    lp_open_spec spec;    /* OPEN */
    float x, y;           /* MOVE */
    lp_rect rect;         /* RESIZE */
    const char *title;    /* SET_TITLE */
    lp_rect bounds;       /* SET_BOUNDS */
} lp_wm_action;

/* Change mask bits above the per-window bits (bit i = windows[i] changed). */
#define LP_WM_CHANGED_ORDER (1ull << 62)
#define LP_WM_CHANGED_FOCUS (1ull << 61)
#define LP_WM_CHANGED_BOUNDS (1ull << 60)
#define LP_WM_CHANGED_OPENED (1ull << 59)
#define LP_WM_CHANGED_CLOSED (1ull << 58)

void lp_wm_init(lp_wm_state *s, lp_rect bounds);
/* Applies an action; 0 when the state is unchanged. */
uint64_t lp_wm_reduce(lp_wm_state *s, const lp_wm_action *a);
int lp_wm_find(const lp_wm_state *s, const char *id);
/* The highest-z window other than `except` (-1 for none); -1 when there is none. */
int lp_wm_top_most(const lp_wm_state *s, int except);
const lp_window_record *lp_wm_focused(const lp_wm_state *s);

#endif
