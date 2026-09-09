#include <math.h>

#include "maryui/lp_spring.h"

static const float TAU = 6.283185307179586f;

lp_spring lp_spring_make(float value, float target) { return (lp_spring){ value, 0.0f, target }; }

void lp_spring_step(lp_spring *s, float dt, lp_spring_params p) {
    float k = (TAU * p.frequency) * (TAU * p.frequency);
    float c = 2.0f * p.damping * sqrtf(k);
    float acceleration = -k * (s->value - s->target) - c * s->velocity;
    s->velocity += acceleration * dt;
    s->value += s->velocity * dt;
}

void lp_spring_snap(lp_spring *s) {
    s->value = s->target;
    s->velocity = 0.0f;
}

int lp_spring_settled(const lp_spring *s, float tolerance, float velocity_tolerance) {
    return fabsf(s->value - s->target) < tolerance && fabsf(s->velocity) < velocity_tolerance;
}
