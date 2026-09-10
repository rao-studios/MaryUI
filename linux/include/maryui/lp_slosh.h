/* Liquid slosh (web/src/lib/slosh.ts): a damped pendulum driven by what the
 * container is doing to the liquid. Drag a window and the liquid in its
 * indicators tilts against the motion, then rings down, like water in a glass
 * you just moved. One state per window is enough (all bubbles share the same
 * physical acceleration); per-bubble variety comes from phase, not physics.
 *
 * Acceleration alone only kicks at the start and the end of a gesture. Shear —
 * the speed the liquid has not caught up with — is what holds the surface over
 * through the middle of a drag. */
#ifndef MARYUI_LP_SLOSH_H
#define MARYUI_LP_SLOSH_H

typedef struct lp_slosh_params {
    float frequency_hz, damping_ratio;
    float gain;         /* radians of tilt per px/s² of container acceleration */
    float shear_gain;   /* drive per unit of shear */
    float shear_gamma;  /* response curve on the shear; below 1 lifts slow, careful drags */
    float max_angle;    /* hard limit on tilt so the liquid never flips */
    float max_accel;    /* accelerations beyond this are clipped before driving the pendulum */
} lp_slosh_params;

/* What the container is doing to the liquid this frame. */
typedef struct lp_slosh_drive {
    float accel;  /* container acceleration in px/s²: the kick at each end of a gesture */
    float shear;  /* normalized velocity the liquid has not caught up with (-1..1) */
} lp_slosh_drive;

typedef struct lp_slosh_state {
    float theta;  /* surface tilt in radians; positive tilts the liquid toward +x */
    float omega;  /* angular velocity in rad/s */
} lp_slosh_state;

/* isSloshSettled's default tolerances: a tilt nobody can see at 18px
 * (0.005 rad ≈ 0.29°), not a tilt of zero, or the engine animates a ring-down
 * long past the point it reads. */
#define LP_SLOSH_TOLERANCE 0.005f
#define LP_SLOSH_VELOCITY_TOLERANCE 0.03f

lp_slosh_state lp_slosh_make(void);
/* Advances the pendulum by dt seconds under what the container is doing. */
lp_slosh_state lp_slosh_step(lp_slosh_state s, lp_slosh_drive drive, float dt, lp_slosh_params p);
int lp_slosh_settled(lp_slosh_state s, float tolerance, float velocity_tolerance);

#endif
