/* Pointer velocity and acceleration from raw pointer samples
 * (web/src/lib/velocity.ts PointerTracker). Velocity is the displacement
 * across the last ~80 ms of samples, smoothed with an EMA; acceleration is the
 * frame-to-frame change of that smoothed velocity. When the pointer stops
 * sending events (finger held still) the velocity decays to zero instead of
 * freezing at its last value. Times are milliseconds; outputs px/s and px/s². */
#ifndef MARYUI_LP_VELOCITY_H
#define MARYUI_LP_VELOCITY_H

typedef struct lp_motion_sample {
    float vx, vy, ax, ay;
} lp_motion_sample;

#define LP_POINTER_TRACKER_CAPACITY 8
#define LP_POINTER_TRACKER_WINDOW_MS 80.0
#define LP_POINTER_TRACKER_STALE_MS 60.0
#define LP_POINTER_TRACKER_SMOOTHING 0.5f

/* Zero-initialised is a fresh tracker; lp_pointer_tracker_reset returns to that. */
typedef struct lp_pointer_tracker {
    struct { double t; float x, y; } samples[LP_POINTER_TRACKER_CAPACITY];
    int count;
    float svx, svy;
    double last_sample_at;
    int primed;  /* sample() has run since the last reset */
} lp_pointer_tracker;

void lp_pointer_tracker_reset(lp_pointer_tracker *t);
void lp_pointer_tracker_push(lp_pointer_tracker *t, double t_ms, float x, float y);
int lp_pointer_tracker_has_samples(const lp_pointer_tracker *t);
/* Call once per frame with the frame's timestamp (ms). */
lp_motion_sample lp_pointer_tracker_sample(lp_pointer_tracker *t, double now_ms);

#endif
