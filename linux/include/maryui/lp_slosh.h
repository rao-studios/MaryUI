/* Liquid slosh (web/src/lib/slosh.ts): a damped pendulum driven by the
 * acceleration of its container. Drag a window and the liquid in its
 * indicators tilts against the motion, then rings down, like water in a glass
 * you just moved. One state per window is enough (all bubbles share the same
 * physical acceleration); per-bubble variety comes from phase, not physics. */
#ifndef MARYUI_LP_SLOSH_H
#define MARYUI_LP_SLOSH_H

typedef struct lp_slosh_params {
    float frequency_hz, damping_ratio;
    float gain;       /* radians of tilt per px/s² of container acceleration */
    float max_angle;  /* hard limit on tilt so the liquid never flips */
    float max_accel;  /* accelerations beyond this are clipped before driving the pendulum */
} lp_slosh_params;

typedef struct lp_slosh_state {
    float theta;  /* surface tilt in radians; positive tilts the liquid toward +x */
    float omega;  /* angular velocity in rad/s */
} lp_slosh_state;

/* isSloshSettled's default tolerances. */
#define LP_SLOSH_TOLERANCE 0.002f
#define LP_SLOSH_VELOCITY_TOLERANCE 0.02f

lp_slosh_state lp_slosh_make(void);
/* Advances the pendulum by dt seconds under a container acceleration ax (px/s²). */
lp_slosh_state lp_slosh_step(lp_slosh_state s, float ax, float dt, lp_slosh_params p);
int lp_slosh_settled(lp_slosh_state s, float tolerance, float velocity_tolerance);

#endif
