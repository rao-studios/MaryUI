#include <math.h>

#include "maryui/lp_radius.h"

#define SQRT1_2 0.7071067811865476f

/* Outward diagonal normals, in corner order. */
static const float NORMALS[LP_CORNER_COUNT][2] = {
    { -SQRT1_2, -SQRT1_2 },  /* tl */
    { SQRT1_2, -SQRT1_2 },   /* tr */
    { SQRT1_2, SQRT1_2 },    /* br */
    { -SQRT1_2, SQRT1_2 },   /* bl */
};

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

float lp_liquidity(float vx, float vy, float velocity_ref, float gamma) {
    float k = clampf(hypotf(vx, vy) / fmaxf(velocity_ref, 1.0f), 0.0f, 1.0f);
    return gamma == 1.0f ? k : powf(k, gamma);
}

lp_corners lp_rest_corners(float rest) { return (lp_corners){ rest, rest, rest, rest }; }

lp_corners lp_corner_targets(float vx, float vy, lp_radius_params p) {
    float speed = hypotf(vx, vy);
    if (speed < 1e-3f) return lp_rest_corners(p.rest);
    float k = lp_liquidity(vx, vy, p.velocity_ref, p.gamma);
    float ux = vx / speed, uy = vy / speed;
    float out[LP_CORNER_COUNT];
    for (int i = 0; i < LP_CORNER_COUNT; i++) {
        /* How much this corner leads the travel: +1 dead ahead, -1 dead behind. */
        float lead = ux * NORMALS[i][0] + uy * NORMALS[i][1];
        out[i] = clampf(p.rest - lead * p.spread * k, p.min, p.max);
    }
    return (lp_corners){ out[0], out[1], out[2], out[3] };
}

float lp_detuned_frequency(float base, int index, float detune) {
    static const float ladder[LP_CORNER_COUNT] = { -1.0f, 0.5f, -0.5f, 1.0f };
    return base * (1.0f + ladder[index % LP_CORNER_COUNT] * detune);
}

float lp_corner_at(lp_corners c, int index) {
    switch (index) {
    case LP_CORNER_TL: return c.tl;
    case LP_CORNER_TR: return c.tr;
    case LP_CORNER_BR: return c.br;
    default: return c.bl;
    }
}
