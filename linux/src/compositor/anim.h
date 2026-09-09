/* Time-based tweens for the CSS keyframe animations the web plays (lp-menu-in,
 * lp-window-close, the shade height transition): a start time, a duration and
 * a cubic-bezier easing from the motion tokens. Springs live in lp_motion. */
#ifndef MUI_ANIM_H
#define MUI_ANIM_H

#include "maryui/lp_motion.h"

struct mui_tween {
    int active;
    double start_ms;
    float duration_ms;
    lp_cubic_bezier ease;
};

static inline void mui_tween_start(struct mui_tween *t, double now_ms, float duration_ms, lp_cubic_bezier ease) {
    t->active = 1;
    t->start_ms = now_ms;
    t->duration_ms = duration_ms;
    t->ease = ease;
}

/* Eased progress in [0, 1]; clears `active` once the duration has passed. */
static inline float mui_tween_progress(struct mui_tween *t, double now_ms) {
    if (!t->active) return 1.0f;
    float p = t->duration_ms > 0 ? (float)((now_ms - t->start_ms) / t->duration_ms) : 1.0f;
    if (p >= 1.0f) {
        t->active = 0;
        return 1.0f;
    }
    if (p < 0) p = 0;
    return lp_cubic_bezier_eval(t->ease, p);
}

/* CSS `ease-in`, the timing function of lp-window-close. */
#define MUI_EASE_IN ((lp_cubic_bezier){ 0.42f, 0.0f, 1.0f, 1.0f })

#endif
