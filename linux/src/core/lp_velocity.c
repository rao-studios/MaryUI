#include <string.h>

#include "maryui/lp_velocity.h"

void lp_pointer_tracker_reset(lp_pointer_tracker *t) { memset(t, 0, sizeof *t); }

void lp_pointer_tracker_push(lp_pointer_tracker *t, double t_ms, float x, float y) {
    if (t->count == LP_POINTER_TRACKER_CAPACITY) {
        memmove(&t->samples[0], &t->samples[1], sizeof t->samples[0] * (LP_POINTER_TRACKER_CAPACITY - 1));
        t->count--;
    }
    t->samples[t->count].t = t_ms;
    t->samples[t->count].x = x;
    t->samples[t->count].y = y;
    t->count++;
}

int lp_pointer_tracker_has_samples(const lp_pointer_tracker *t) { return t->count > 0; }

lp_motion_sample lp_pointer_tracker_sample(lp_pointer_tracker *t, double now_ms) {
    float raw_vx = 0.0f, raw_vy = 0.0f;
    if (t->count > 0) {
        int newest = t->count - 1;
        if (now_ms - t->samples[newest].t <= LP_POINTER_TRACKER_STALE_MS) {
            int oldest = newest;
            for (int i = newest - 1; i >= 0; i--) {
                if (t->samples[newest].t - t->samples[i].t > LP_POINTER_TRACKER_WINDOW_MS) break;
                oldest = i;
            }
            double dt = (t->samples[newest].t - t->samples[oldest].t) / 1000.0;
            if (dt > 0) {
                raw_vx = (float)((t->samples[newest].x - t->samples[oldest].x) / dt);
                raw_vy = (float)((t->samples[newest].y - t->samples[oldest].y) / dt);
            }
        }
    }
    float prev_vx = t->svx, prev_vy = t->svy;
    t->svx += (raw_vx - t->svx) * LP_POINTER_TRACKER_SMOOTHING;
    t->svy += (raw_vy - t->svy) * LP_POINTER_TRACKER_SMOOTHING;

    double frame_dt = t->primed ? (now_ms - t->last_sample_at) / 1000.0 : 0.0;
    t->last_sample_at = now_ms;
    t->primed = 1;
    float ax = frame_dt > 0 ? (float)((t->svx - prev_vx) / frame_dt) : 0.0f;
    float ay = frame_dt > 0 ? (float)((t->svy - prev_vy) / frame_dt) : 0.0f;
    return (lp_motion_sample){ t->svx, t->svy, ax, ay };
}
