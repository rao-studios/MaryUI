/* A one-dimensional damped spring (web/src/lib/spring.ts), parameterized the
 * way designers think about it: natural frequency in Hz and a damping ratio,
 * not raw stiffness. Integrated with semi-implicit Euler, which is stable for
 * every frequency the design system uses at frame rates down to 30 fps.
 * Allocation-free: lp_spring_step mutates in place so the motion engine can
 * run dozens of springs per frame. */
#ifndef MARYUI_LP_SPRING_H
#define MARYUI_LP_SPRING_H

#include "maryui/lp_types.h"

typedef struct lp_spring {
    float value, velocity, target;
} lp_spring;

/* isSettled's default tolerances. */
#define LP_SPRING_TOLERANCE 0.001f
#define LP_SPRING_VELOCITY_TOLERANCE 0.01f

/* createSpring(value, target). */
lp_spring lp_spring_make(float value, float target);
/* Advances the spring by dt seconds toward its target. */
void lp_spring_step(lp_spring *s, float dt, lp_spring_params p);
/* Jumps straight to the target with no residual motion. */
void lp_spring_snap(lp_spring *s);
/* Within tolerance of the target and (nearly) at rest. */
int lp_spring_settled(const lp_spring *s, float tolerance, float velocity_tolerance);

#endif
