#include <math.h>

#include "maryui/lp_slosh.h"

static const float TAU = 6.283185307179586f;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

lp_slosh_state lp_slosh_make(void) { return (lp_slosh_state){ 0.0f, 0.0f }; }

lp_slosh_state lp_slosh_step(lp_slosh_state s, lp_slosh_drive drive, float dt, lp_slosh_params p) {
    float k = (TAU * p.frequency_hz) * (TAU * p.frequency_hz);
    float c = 2.0f * p.damping_ratio * sqrtf(k);
    /* Clamp before the power, or an out-of-range shear would blow up under gamma. */
    float shear = clampf(drive.shear, -1.0f, 1.0f);
    float force = clampf(drive.accel, -p.max_accel, p.max_accel) * p.gain +
                  (shear < 0 ? -1.0f : shear > 0 ? 1.0f : 0.0f) * powf(fabsf(shear), p.shear_gamma) * p.shear_gain;
    float alpha = -k * s.theta - c * s.omega - force;
    float omega = s.omega + alpha * dt;
    float theta = clampf(s.theta + omega * dt, -p.max_angle, p.max_angle);
    /* Hitting the rim bleeds energy, like liquid splashing against glass. */
    return (lp_slosh_state){ theta, fabsf(theta) >= p.max_angle ? omega * 0.4f : omega };
}

int lp_slosh_settled(lp_slosh_state s, float tolerance, float velocity_tolerance) {
    return fabsf(s.theta) < tolerance && fabsf(s.omega) < velocity_tolerance;
}
