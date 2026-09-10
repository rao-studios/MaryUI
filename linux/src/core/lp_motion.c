#include <math.h>
#include <string.h>

#include "maryui/lp_geometry.h"
#include "maryui/lp_motion.h"
#include "maryui/lp_tokens.h"

/* MARK: - Parameters */

lp_motion_params lp_motion_params_from_tokens(void) {
    return (lp_motion_params){
        .sheen = LP_MOTION_SPRING_SHEEN, .tilt = LP_MOTION_SPRING_TILT, .jelly = LP_MOTION_SPRING_JELLY, .fly = LP_MOTION_SPRING_FLY,
        .radius = LP_MOTION_SPRING_RADIUS, .vx_lag = LP_MOTION_SPRING_VX_LAG,
        .slosh = { LP_MOTION_SLOSH_FREQUENCY, LP_MOTION_SLOSH_DAMPING, LP_MOTION_SLOSH_GAIN, LP_MOTION_SLOSH_SHEAR_GAIN,
                   LP_MOTION_SLOSH_SHEAR_GAMMA, LP_MOTION_SLOSH_MAX, LP_MOTION_SLOSH_MAX_ACCEL },
        .corners = { LP_RADIUS_WINDOW, LP_RADIUS_WINDOW_MIN, LP_RADIUS_WINDOW_MAX, LP_RADIUS_FLEX_SPREAD,
                     LP_RADIUS_FLEX_VELOCITY_REF, LP_RADIUS_FLEX_GAMMA },
        .jelly_max_scale = LP_MOTION_JELLY_MAX_SCALE, .jelly_max_skew = LP_MOTION_JELLY_MAX_SKEW, .tilt_max = LP_MOTION_TILT_MAX,
        .velocity_ref = LP_MOTION_VELOCITY_REF, .light_x = LP_SHEEN_LIGHT_X,
        .radius_detune = LP_MOTION_RADIUS_DETUNE, .corner_impulse = LP_RADIUS_FLEX_IMPULSE,
        .slosh_velocity_ref = LP_MOTION_SLOSH_VELOCITY_REF, .grain_lag = LP_BRUSH_LAG,
        .grain_follow_ms = LP_MOTION_GRAIN_FOLLOW_MS,
    };
}

lp_motion_params lp_motion_live = {
    .sheen = LP_MOTION_SPRING_SHEEN, .tilt = LP_MOTION_SPRING_TILT, .jelly = LP_MOTION_SPRING_JELLY, .fly = LP_MOTION_SPRING_FLY,
    .radius = LP_MOTION_SPRING_RADIUS, .vx_lag = LP_MOTION_SPRING_VX_LAG,
    .slosh = { LP_MOTION_SLOSH_FREQUENCY, LP_MOTION_SLOSH_DAMPING, LP_MOTION_SLOSH_GAIN, LP_MOTION_SLOSH_SHEAR_GAIN,
               LP_MOTION_SLOSH_SHEAR_GAMMA, LP_MOTION_SLOSH_MAX, LP_MOTION_SLOSH_MAX_ACCEL },
    .corners = { LP_RADIUS_WINDOW, LP_RADIUS_WINDOW_MIN, LP_RADIUS_WINDOW_MAX, LP_RADIUS_FLEX_SPREAD,
                 LP_RADIUS_FLEX_VELOCITY_REF, LP_RADIUS_FLEX_GAMMA },
    .jelly_max_scale = LP_MOTION_JELLY_MAX_SCALE, .jelly_max_skew = LP_MOTION_JELLY_MAX_SKEW, .tilt_max = LP_MOTION_TILT_MAX,
    .velocity_ref = LP_MOTION_VELOCITY_REF, .light_x = LP_SHEEN_LIGHT_X,
    .radius_detune = LP_MOTION_RADIUS_DETUNE, .corner_impulse = LP_RADIUS_FLEX_IMPULSE,
    .slosh_velocity_ref = LP_MOTION_SLOSH_VELOCITY_REF, .grain_lag = LP_BRUSH_LAG,
    .grain_follow_ms = LP_MOTION_GRAIN_FOLLOW_MS,
};

void lp_motion_reset_params(void) { lp_motion_live = lp_motion_params_from_tokens(); }

/* MARK: - Easing */

static float bezier_axis(float p1, float p2, float t) {
    float u = 1.0f - t;
    return 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t;
}

static float bezier_axis_slope(float p1, float p2, float t) {
    float u = 1.0f - t;
    return 3.0f * u * u * p1 + 6.0f * u * t * (p2 - p1) + 3.0f * t * t * (1.0f - p2);
}

float lp_cubic_bezier_eval(lp_cubic_bezier b, float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    /* Solve x(t) = x: Newton from t = x, bisection when the slope is flat. */
    float t = x;
    for (int i = 0; i < 8; i++) {
        float err = bezier_axis(b.x1, b.x2, t) - x;
        if (fabsf(err) < 1e-6f) break;
        float slope = bezier_axis_slope(b.x1, b.x2, t);
        if (fabsf(slope) < 1e-6f) break;
        t -= err / slope;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    if (fabsf(bezier_axis(b.x1, b.x2, t) - x) > 1e-4f) {
        float lo = 0.0f, hi = 1.0f;
        for (int i = 0; i < 24; i++) {
            t = 0.5f * (lo + hi);
            if (bezier_axis(b.x1, b.x2, t) < x) lo = t; else hi = t;
        }
    }
    return bezier_axis(b.y1, b.y2, t);
}

/* MARK: - Engine */

void lp_motion_engine_init(lp_motion_engine *e, void (*wake)(void *user), void *user) {
    memset(e, 0, sizeof *e);
    e->wake = wake;
    e->user = user;
}

void lp_motion_engine_add(lp_motion_engine *e, lp_motion_target *target) {
    if (target->attached) return;
    target->attached = 1;
    target->next = e->targets;
    e->targets = target;
}

void lp_motion_engine_remove(lp_motion_engine *e, lp_motion_target *target) {
    if (!target->attached) return;
    for (lp_motion_target **p = &e->targets; *p; p = &(*p)->next) {
        if (*p == target) { *p = target->next; break; }
    }
    target->next = NULL;
    target->attached = 0;
}

void lp_motion_engine_wake(lp_motion_engine *e, double now_ms) {
    if (e->running) return;
    e->running = 1;
    e->last_ms = now_ms;
    e->wakes++;
    if (e->wake) e->wake(e->user);
}

int lp_motion_engine_tick(lp_motion_engine *e, double now_ms) {
    if (!e->running) return 0;
    double elapsed = (now_ms - e->last_ms) / 1000.0;
    float dt = elapsed < 0 ? 0.0f : elapsed > LP_MOTION_MAX_DT ? LP_MOTION_MAX_DT : (float)elapsed;
    e->last_ms = now_ms;
    e->frames++;
    int active = 0;
    /* A target may remove itself while stepping: read `next` first. */
    for (lp_motion_target *t = e->targets, *next; t; t = next) {
        next = t->next;
        active = t->step(t, dt, now_ms) || active;
    }
    e->running = active;
    if (!active) e->idles++;
    return active;
}

/* MARK: - Window motion */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void lp_window_motion_init(lp_window_motion *m) {
    memset(m, 0, sizeof *m);
    m->sheen = lp_spring_make(0.5f, 0.5f);
    m->tilt = lp_spring_make(0, 0);
    m->skew = lp_spring_make(0, 0);
    m->sx = lp_spring_make(1, 1);
    m->sy = lp_spring_make(1, 1);
    m->fly_x = lp_spring_make(0, 0);
    m->fly_y = lp_spring_make(0, 0);
    m->fly_sx = lp_spring_make(1, 1);
    m->fly_sy = lp_spring_make(1, 1);
    for (int i = 0; i < LP_CORNER_COUNT; i++) m->corners[i] = lp_spring_make(LP_RADIUS_WINDOW, LP_RADIUS_WINDOW);
    m->grain_x = m->grain_y = 0;
    m->vx_lag = lp_spring_make(0, 0);
    m->vy_lag = lp_spring_make(0, 0);
    m->slosh = lp_slosh_make();
    m->slosh_y = lp_slosh_make();
    m->out = (lp_window_motion_out){ .sx = 1, .sy = 1, .jelly_sx = 1, .jelly_sy = 1, .fly_sx = 1, .fly_sy = 1, .sheen_x = 0.5f,
                                     .corners = lp_rest_corners(LP_RADIUS_WINDOW), .identity = 1 };
}

static void write_transform(lp_window_motion *m) {
    lp_window_motion_out *o = &m->out;
    o->tx = m->drag_dx + m->fly_x.value;
    o->ty = m->drag_dy + m->fly_y.value;
    o->jelly_sx = m->sx.value;
    o->jelly_sy = m->sy.value;
    o->fly_sx = m->fly_sx.value;
    o->fly_sy = m->fly_sy.value;
    o->sx = o->jelly_sx * o->fly_sx;
    o->sy = o->jelly_sy * o->fly_sy;
    o->skew_deg = m->skew.value;
    o->origin_x = m->origin_x;
    o->origin_y = m->origin_y;
    o->identity = 0;
}

static void write_identity(lp_window_motion *m) {
    lp_window_motion_out *o = &m->out;
    o->tx = o->ty = 0;
    o->sx = o->sy = o->jelly_sx = o->jelly_sy = o->fly_sx = o->fly_sy = 1;
    o->skew_deg = 0;
    o->identity = 1;
}

void lp_window_motion_nudge_corners(lp_window_motion *m, float impulse, int reduced_motion) {
    if (reduced_motion) return;
    for (int i = 0; i < LP_CORNER_COUNT; i++) m->corners[i].velocity += impulse * (i % 2 == 0 ? 1.0f : -1.0f);
}

void lp_window_motion_begin_drag(lp_window_motion *m, float grab_x, float grab_y) {
    m->dragging = 1;
    m->drag_dx = m->drag_dy = 0;
    lp_pointer_tracker_reset(&m->tracker);
    m->origin_x = grab_x;
    m->origin_y = grab_y;
    lp_window_motion_nudge_corners(m, lp_motion_live.corner_impulse, 0);
}

void lp_window_motion_move_drag(lp_window_motion *m, float dx, float dy, double t_ms, float x, float y) {
    m->drag_dx = dx;
    m->drag_dy = dy;
    lp_pointer_tracker_push(&m->tracker, t_ms, x, y);
}

void lp_window_motion_end_drag(lp_window_motion *m) {
    /* The frame is about to jump to its committed position while the delta drops
     * to zero. Carry the grain's lag across with it, or the skin would snap. */
    m->grain_x -= m->drag_dx;
    m->grain_y -= m->drag_dy;
    m->dragging = 0;
    m->drag_dx = m->drag_dy = 0;
    lp_pointer_tracker_reset(&m->tracker);
    lp_window_motion_nudge_corners(m, -lp_motion_live.corner_impulse, 0);
    write_transform(m);
}

void lp_window_motion_begin_resize(lp_window_motion *m) { m->resizing = 1; }
void lp_window_motion_end_resize(lp_window_motion *m) { m->resizing = 0; }

void lp_window_motion_fly_from(lp_window_motion *m, lp_rect from, lp_rect to, int reduced_motion) {
    lp_flip t = lp_flip_transform(from, to);
    if (reduced_motion) return;
    m->fly_x.value = t.tx;
    m->fly_y.value = t.ty;
    m->fly_sx.value = t.sx;
    m->fly_sy.value = t.sy;
    m->fly_x.velocity = m->fly_y.velocity = m->fly_sx.velocity = m->fly_sy.velocity = 0;
    m->flying = 1;
    m->origin_x = m->origin_y = 0;
    write_transform(m);
}

static int corners_settled(const lp_window_motion *m) {
    for (int i = 0; i < LP_CORNER_COUNT; i++) {
        if (!lp_spring_settled(&m->corners[i], 0.02f, 0.2f)) return 0;
    }
    return 1;
}

/* Publishing a radius forces a chrome repaint, so the corners only move in
 * quarter-pixels — the same throttle the web puts on writing --lp-r-*. */
static void write_corners(lp_window_motion *m, const lp_motion_params *p) {
    lp_corners next = { m->corners[LP_CORNER_TL].value, m->corners[LP_CORNER_TR].value,
                        m->corners[LP_CORNER_BR].value, m->corners[LP_CORNER_BL].value };
    if (m->wrote_corners_valid) {
        int moved = 0;
        for (int i = 0; i < LP_CORNER_COUNT; i++) {
            if (fabsf(lp_corner_at(next, i) - lp_corner_at(m->wrote_corners, i)) >= 0.25f) { moved = 1; break; }
        }
        if (!moved) return;
    }
    m->wrote_corners = next;
    m->wrote_corners_valid = 1;
    m->out.corners = next;
    float spread = 0;
    for (int i = 0; i < LP_CORNER_COUNT; i++) {
        float d = fabsf(lp_corner_at(next, i) - p->corners.rest);
        if (d > spread) spread = d;
    }
    m->out.radius_k = spread / fmaxf(p->corners.max - p->corners.rest, 1.0f);
}

int lp_window_motion_step(lp_window_motion *m, float dt, double now_ms, lp_rect rect, float viewport_w,
                          const lp_motion_params *p, int reduced) {
    lp_motion_sample s = m->dragging ? lp_pointer_tracker_sample(&m->tracker, now_ms) : (lp_motion_sample){ 0, 0, 0, 0 };
    float vw = viewport_w > 0 ? viewport_w : 1;

    m->sheen.target = clampf((p->light_x * vw - (rect.x + m->drag_dx)) / fmaxf(rect.w, 1), -0.3f, 1.3f);
    float n = clampf(s.vx / p->velocity_ref, -1, 1);
    float mag = clampf(hypotf(s.vx, s.vy) / p->velocity_ref, 0, 1);
    int deform = m->dragging && !m->resizing && !reduced;
    m->tilt.target = deform ? n * p->tilt_max : 0;
    m->skew.target = deform ? n * p->jelly_max_skew : 0;
    m->sx.target = deform ? 1 + mag * p->jelly_max_scale : 1;
    m->sy.target = deform ? 1 - 0.6f * mag * p->jelly_max_scale : 1;

    /* The corners answer to how the window is *seen* to move, so a zoom's flight
     * deforms them exactly as a drag does. */
    float seen_vx = s.vx + m->fly_x.velocity, seen_vy = s.vy + m->fly_y.velocity;
    lp_corners targets = reduced ? lp_rest_corners(p->corners.rest) : lp_corner_targets(seen_vx, seen_vy, p->corners);
    for (int i = 0; i < LP_CORNER_COUNT; i++) m->corners[i].target = lp_corner_at(targets, i);
    float ny = clampf(s.vy / p->velocity_ref, -1, 1);
    m->vx_lag.target = n;
    m->vy_lag.target = ny;

    if (reduced) {
        lp_spring_snap(&m->sheen);
        lp_spring_snap(&m->tilt);
        lp_spring_snap(&m->skew);
        lp_spring_snap(&m->sx);
        lp_spring_snap(&m->sy);
        lp_spring_snap(&m->fly_x);
        lp_spring_snap(&m->fly_y);
        lp_spring_snap(&m->fly_sx);
        lp_spring_snap(&m->fly_sy);
        m->slosh = lp_slosh_make();
        m->slosh_y = lp_slosh_make();
        for (int i = 0; i < LP_CORNER_COUNT; i++) lp_spring_snap(&m->corners[i]);
        m->grain_x = m->drag_dx;
        m->grain_y = m->drag_dy;
        lp_spring_snap(&m->vx_lag);
        lp_spring_snap(&m->vy_lag);
        m->flying = 0;
    } else {
        lp_spring_step(&m->sheen, dt, p->sheen);
        lp_spring_step(&m->tilt, dt, p->tilt);
        lp_spring_step(&m->skew, dt, p->jelly);
        lp_spring_step(&m->sx, dt, p->jelly);
        lp_spring_step(&m->sy, dt, p->jelly);
        lp_spring_step(&m->fly_x, dt, p->fly);
        lp_spring_step(&m->fly_y, dt, p->fly);
        lp_spring_step(&m->fly_sx, dt, p->fly);
        lp_spring_step(&m->fly_sy, dt, p->fly);
        /* The lag springs follow velocity normalized against the jelly's reference;
         * the liquid shears against its own, lower one. Both normalizations are
         * linear, so rescaling here is exact and saves a second pair of springs. */
        float shear_scale = p->velocity_ref / fmaxf(p->slosh_velocity_ref, 1.0f);
        lp_slosh_drive dx = { s.ax, (n - m->vx_lag.value) * shear_scale };
        lp_slosh_drive dy = { s.ay, (ny - m->vy_lag.value) * shear_scale };
        m->slosh = lp_slosh_step(m->slosh, dx, dt, p->slosh);
        m->slosh_y = lp_slosh_step(m->slosh_y, dy, dt, p->slosh);
        for (int i = 0; i < LP_CORNER_COUNT; i++) {
            lp_spring_params cp = { lp_detuned_frequency(p->radius.frequency, i, p->radius_detune), p->radius.damping };
            lp_spring_step(&m->corners[i], dt, cp);
        }
        /* The grain chases the frame's own translation: while a drag moves it
         * trails by a constant amount, and when the drag stops it glides home.
         * A spring here sprang back past the frame, which read as elastic. */
        m->grain_x = lp_follow(m->grain_x, m->drag_dx, dt, p->grain_follow_ms);
        m->grain_y = lp_follow(m->grain_y, m->drag_dy, dt, p->grain_follow_ms);
        lp_spring_step(&m->vx_lag, dt, p->vx_lag);
        lp_spring_step(&m->vy_lag, dt, p->vx_lag);
    }

    m->out.sheen_x = m->sheen.value;
    m->out.tilt_deg = m->tilt.value;
    m->out.vx = n;
    m->out.slosh_deg = -m->slosh.theta * 180.0f / (float)M_PI;
    /* The sideways pile-up is the strongest cue at 18px; rotation alone is only
     * a couple of pixels of crest movement. */
    m->out.slosh_x_px = -sinf(m->slosh.theta) * LP_MOTION_SLOSH_SHIFT;
    m->out.slosh_y_px = m->slosh_y.theta * LP_MOTION_SLOSH_LIFT;
    m->out.vx_lag = m->vx_lag.value;
    m->out.speed = lp_liquidity(seen_vx, seen_vy, p->velocity_ref, 1.0f);
    m->out.grain_x = clampf(m->grain_x - m->drag_dx, -p->grain_lag, p->grain_lag);
    m->out.grain_y = clampf(m->grain_y - m->drag_dy, -p->grain_lag, p->grain_lag);
    write_corners(m, p);
    m->last_vx = n;

    int flight_settled = lp_spring_settled(&m->fly_x, 0.05f, 0.5f) && lp_spring_settled(&m->fly_y, 0.05f, 0.5f) &&
                         lp_spring_settled(&m->fly_sx, 0.0005f, 0.005f) && lp_spring_settled(&m->fly_sy, 0.0005f, 0.005f);
    if (m->flying && flight_settled) {
        m->flying = 0;
        lp_spring_snap(&m->fly_x);
        lp_spring_snap(&m->fly_y);
        lp_spring_snap(&m->fly_sx);
        lp_spring_snap(&m->fly_sy);
    }

    int deform_settled = lp_spring_settled(&m->skew, 0.005f, 0.05f) && lp_spring_settled(&m->sx, 0.0005f, 0.005f) &&
                         lp_spring_settled(&m->sy, 0.0005f, 0.005f);
    int settled = !m->dragging && !m->flying && !m->resizing && deform_settled &&
                  lp_spring_settled(&m->tilt, 0.005f, 0.05f) && lp_spring_settled(&m->sheen, 0.0005f, 0.005f) &&
                  fabsf(m->grain_x - m->drag_dx) < 0.05f && fabsf(m->grain_y - m->drag_dy) < 0.05f &&
                  lp_spring_settled(&m->vx_lag, 0.002f, 0.02f) && lp_spring_settled(&m->vy_lag, 0.002f, 0.02f) &&
                  corners_settled(m) &&
                  lp_slosh_settled(m->slosh, LP_SLOSH_TOLERANCE, LP_SLOSH_VELOCITY_TOLERANCE) &&
                  lp_slosh_settled(m->slosh_y, LP_SLOSH_TOLERANCE, LP_SLOSH_VELOCITY_TOLERANCE);

    if (settled) {
        if (!m->wrote_identity) {
            write_identity(m);
            m->wrote_identity = 1;
        }
        return 0;
    }
    m->wrote_identity = 0;
    write_transform(m);
    return 1;
}

float lp_window_motion_velocity_x(const lp_window_motion *m) { return m->last_vx; }
