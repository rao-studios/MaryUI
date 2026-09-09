/* The one animation loop behind everything that moves fluidly
 * (web/src/lib/motionEngine.ts). lp_motion_engine steps every registered
 * target while any of them is still moving; an idle desktop schedules no
 * frames at all. lp_window_motion is the per-window target: it turns pointer
 * velocity into the sheen position, the tilt, the jelly deformation, the FLIP
 * flight and the liquid slosh, and reports them in lp_window_motion_out for
 * the compositor to apply to scene nodes (the web writes a `transform` and a
 * handful of `--lp-*` variables instead). Nothing here knows about Wayland.
 *
 * The constants live in lp_motion_live, a mutable copy of the motion tokens
 * so the Gallery's Motion tab can retune the feel live. */
#ifndef MARYUI_LP_MOTION_H
#define MARYUI_LP_MOTION_H

#include "maryui/lp_slosh.h"
#include "maryui/lp_spring.h"
#include "maryui/lp_types.h"
#include "maryui/lp_velocity.h"

/* MARK: - Parameters */

typedef struct lp_motion_params {
    lp_spring_params sheen, tilt, jelly, fly;
    lp_slosh_params slosh;
    float jelly_max_scale, jelly_max_skew, tilt_max, velocity_ref;
    float light_x;  /* where the room light sits, as a fraction of viewport width */
} lp_motion_params;

lp_motion_params lp_motion_params_from_tokens(void);
/* Live, mutable motion constants. Mutate fields in place; targets read them every frame. */
extern lp_motion_params lp_motion_live;
void lp_motion_reset_params(void);

/* CSS cubic-bezier(x1, y1, x2, y2) easing: the eased value for progress t in [0, 1]. */
float lp_cubic_bezier_eval(lp_cubic_bezier b, float t);

/* MARK: - Engine */

typedef struct lp_motion_target lp_motion_target;
struct lp_motion_target {
    /* Advance by dt seconds. Return non-zero while still moving. */
    int (*step)(lp_motion_target *target, float dt, double now_ms);
    void *user;
    lp_motion_target *next;  /* engine-owned */
    int attached;            /* engine-owned */
};

/* Frames longer than this are stepped as one 30 fps frame (the web's Math.min(dt, 1/30)). */
#define LP_MOTION_MAX_DT (1.0f / 30)

typedef struct lp_motion_engine {
    lp_motion_target *targets;
    int running;               /* a frame is scheduled (the web's `frame !== null`) */
    double last_ms;
    void (*wake)(void *user);  /* schedule a frame; called once when the idle loop starts */
    void *user;
    unsigned wakes, idles, frames;  /* counters for MARYUI_DEBUG=frames */
} lp_motion_engine;

void lp_motion_engine_init(lp_motion_engine *e, void (*wake)(void *user), void *user);
void lp_motion_engine_add(lp_motion_engine *e, lp_motion_target *target);
void lp_motion_engine_remove(lp_motion_engine *e, lp_motion_target *target);
/* Starts the loop if it is idle. Safe to call on every pointer event. */
void lp_motion_engine_wake(lp_motion_engine *e, double now_ms);
/* The frame callback: steps every target while the loop runs. Returns non-zero
 * while any target is still moving, i.e. when another frame should follow. */
int lp_motion_engine_tick(lp_motion_engine *e, double now_ms);

/* MARK: - Window motion */

typedef struct lp_window_motion_out {
    float tx, ty;                 /* translation of the chrome in px (drag delta + flight) */
    float sx, sy;                 /* total scale about the origin (jelly × flight) */
    float jelly_sx, jelly_sy;     /* the jelly part alone (clients keep their pixels: the compositor may skip it) */
    float fly_sx, fly_sy;         /* the flight part alone */
    float skew_deg;               /* skewX; computed for parity, wlr_scene cannot apply it */
    float origin_x, origin_y;     /* transform origin in window-rect coordinates */
    float sheen_x, tilt_deg, vx, slosh_deg, slosh_y_px;  /* the --lp-* variables */
    int identity;                 /* the transform is exactly the identity (settled) */
} lp_window_motion_out;

typedef struct lp_window_motion {
    lp_pointer_tracker tracker;
    lp_spring sheen, tilt, skew, sx, sy, fly_x, fly_y, fly_sx, fly_sy;
    lp_slosh_state slosh, slosh_y;
    int dragging, resizing, flying;
    float drag_dx, drag_dy;
    float last_vx;
    float origin_x, origin_y;
    int wrote_identity;
    lp_window_motion_out out;
} lp_window_motion;

void lp_window_motion_init(lp_window_motion *m);
/* grab_x/grab_y: the pointer in window-rect coordinates; the deformation scales about it. */
void lp_window_motion_begin_drag(lp_window_motion *m, float grab_x, float grab_y);
/* dx/dy: the clamped drag delta from the rect the window is laid out at; t_ms/x/y: the pointer sample. */
void lp_window_motion_move_drag(lp_window_motion *m, float dx, float dy, double t_ms, float x, float y);
/* Ends the drag. The caller has already committed the final layout, so the delta returns to zero at once. */
void lp_window_motion_end_drag(lp_window_motion *m);
void lp_window_motion_begin_resize(lp_window_motion *m);
void lp_window_motion_end_resize(lp_window_motion *m);
/* The window is now laid out at `to`; make it appear at `from` and spring home. */
void lp_window_motion_fly_from(lp_window_motion *m, lp_rect from, lp_rect to, int reduced_motion);
/* One frame. live_rect is the rect the window is laid out at (without the drag
 * delta), viewport_w the desktop width. Fills m->out; returns non-zero while moving. */
int lp_window_motion_step(lp_window_motion *m, float dt, double now_ms, lp_rect live_rect, float viewport_w,
                          const lp_motion_params *p, int reduced_motion);
/* Normalized horizontal velocity from the last frame; the traffic lights smear with it. */
float lp_window_motion_velocity_x(const lp_window_motion *m);

#endif
