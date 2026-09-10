/* The immediate-mode core. A chrome's paint function is called twice: an
 * EVENT pass on input (no cairo; widgets hit-test and update hot/active/focus,
 * returning what happened) and a DRAW pass when something is dirty (the same
 * code, with cr set). Widget identity is an lp_id the caller derives from a
 * string (LP_ID) and an index. */
#ifndef MARYUI_LP_UI_H
#define MARYUI_LP_UI_H

#include <cairo.h>
#include <stdint.h>

#include "maryui/lp_radius.h"
#include "maryui/lp_types.h"

enum lp_pass { LP_PASS_EVENT, LP_PASS_DRAW };

enum lp_cursor_shape {
    LP_CURSOR_ARROW,
    LP_CURSOR_TEXT,
    LP_CURSOR_POINTER,
    LP_CURSOR_NS,
    LP_CURSOR_EW,
    LP_CURSOR_NWSE,
    LP_CURSOR_NESW,
    LP_CURSOR_COUNT,
};

#define LP_BUTTON_LEFT 1
#define LP_BUTTON_RIGHT 2
#define LP_BUTTON_MIDDLE 4

#define LP_MOD_SHIFT 1
#define LP_MOD_CTRL 4
#define LP_MOD_ALT 8
#define LP_MOD_LOGO 64

typedef struct lp_input {
    float mx, my;             /* pointer in chrome coordinates; NAN when outside */
    int buttons;              /* held */
    int pressed, released;    /* edges this event */
    int double_click;
    float scroll_x, scroll_y; /* pixels this event */
    uint32_t keysym;          /* xkb keysym of a key event, else 0 */
    uint32_t mods;            /* LP_MOD_* */
    int key_pressed;          /* 1 press, 0 release */
    char utf8[8];             /* the key's text, if any */
    uint32_t time_ms;
    int drag;                 /* a drag session is over the chrome: LP_DRAG_* */
} lp_input;

#define LP_DRAG_HOVER 1       /* the pointer moved over the chrome while dragging */
#define LP_DRAG_DROP 2        /* released here */
#define LP_DRAG_LEAVE 3       /* the pointer left the chrome (mx/my are NAN) */

typedef struct lp_ctx {
    enum lp_pass pass;
    cairo_t *cr;              /* NULL in the EVENT pass */
    lp_input in;
    lp_id hot, active, focus, next_hot;
    lp_rect bounds;           /* the chrome's rectangle in its own coordinates */
    int dirty;                /* set by widgets whose look changed: the whole chrome repaints */
    lp_rect damage;           /* lp_damage: a part of the chrome to repaint instead (union) */
    int has_damage;
    enum lp_cursor_shape cursor;
    /* The --lp-* motion variables of the enclosing window. */
    float sheen_x, tilt, vx, slosh_deg, slosh_y;
    float vx_lag;             /* vx - vx_lag is the shear the goo smears with */
    float speed;              /* normalized speed, 0..1: the grain's glint rides it */
    float slosh_x;            /* the liquid's lateral bank */
    float grain_x, grain_y;   /* how far the brushed skin lags the frame */
    lp_corners corners;       /* the window's four live radii */
    float radius_k;           /* how far they are from rest, 0..1 */
    int active_window;        /* the window is focused (inactive chrome drains its colours) */
    double now_ms;
    int wants_frame;          /* an ambient animation asks for another frame */
    lp_rect wants_frame_rect; /* where it lives (union); the host repaints only this much */
    const struct lp_settings *settings;
} lp_ctx;

void lp_ctx_begin(lp_ctx *ctx, enum lp_pass pass, cairo_t *cr, const lp_input *in, lp_rect bounds, double now_ms);
void lp_ctx_end(lp_ctx *ctx);

lp_id lp_id_hash(const char *s);
lp_id lp_id_index(lp_id base, int index);
#define LP_ID(str) lp_id_hash(str)

/* A radius that swells while the window's corners are in motion: the whole
 * frame goes liquid together, not just its outline. radius.flex-* tokens. */
float lp_radius_flex(const lp_ctx *ctx, float radius);

int lp_hit(const lp_ctx *ctx, lp_rect r);
/* Registers r as the hot area for id when the pointer is over it; returns hot. */
int lp_hot(lp_ctx *ctx, lp_id id, lp_rect r);
/* The button idiom: hot on hover, active while pressed inside, "clicked" on release inside. */
int lp_clicked(lp_ctx *ctx, lp_id id, lp_rect r);
int lp_is_active(const lp_ctx *ctx, lp_id id);
int lp_is_hot(const lp_ctx *ctx, lp_id id);
/* Marks a rectangle for repaint without dirtying the whole chrome (hover rings, marquees). */
void lp_damage(lp_ctx *ctx, lp_rect r);
/* Asks the host for another frame; the whole chrome is repainted. */
void lp_want_frame(lp_ctx *ctx);
/* Asks for another frame for r only (a progress glint, a rolling bubble). */
void lp_want_frame_rect(lp_ctx *ctx, lp_rect r);

/* Layout helpers: cut a strip off an area, split an area into columns/rows. */
lp_rect lp_rect_inset(lp_rect r, float dx, float dy);
lp_rect lp_rect_cut_top(lp_rect *area, float size);
lp_rect lp_rect_cut_bottom(lp_rect *area, float size);
lp_rect lp_rect_cut_left(lp_rect *area, float size);
lp_rect lp_rect_cut_right(lp_rect *area, float size);
int lp_rect_contains(lp_rect r, float x, float y);
lp_rect lp_rect_center(lp_rect area, lp_size size);
/* n children along the axis with `gap`: flex[i] > 0 shares the free space, flex[i] == 0 takes fixed[i] px. */
void lp_layout_row(lp_rect area, float gap, int n, const float *flex, const float *fixed, lp_rect *out);
void lp_layout_column(lp_rect area, float gap, int n, const float *flex, const float *fixed, lp_rect *out);

#endif
