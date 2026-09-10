/* Liquid corner radii (web/src/lib/radius.ts): the corners of a moving window
 * do not stay round. The corner the window leads with flattens, the one it
 * trails with swells, and the two on the axis of travel are left at rest — the
 * shape of a drop being pulled through air.
 *
 * Pure maths, no state: the springs that chase these targets live in
 * lp_motion.h. The corners keep their own velocity reference, far below the
 * engine's jelly reference, so an ordinary drag reaches the full range instead
 * of needing a fling. */
#ifndef MARYUI_LP_RADIUS_H
#define MARYUI_LP_RADIUS_H

typedef struct lp_corners { float tl, tr, br, bl; } lp_corners;

typedef struct lp_radius_params {
    float rest, min, max;
    float spread;        /* px a fully leading corner gives up */
    float velocity_ref;  /* speed at which the spread is fully spent */
    float gamma;         /* response curve; below 1 lifts slow drags */
} lp_radius_params;

/* Corner order everywhere: tl, tr, br, bl. */
#define LP_CORNER_COUNT 4
enum lp_corner { LP_CORNER_TL, LP_CORNER_TR, LP_CORNER_BR, LP_CORNER_BL };

/* Normalized, clamped speed, curved by gamma. */
float lp_liquidity(float vx, float vy, float velocity_ref, float gamma);
lp_corners lp_rest_corners(float rest);
/* The radii a window travelling at (vx, vy) is pulling toward. */
lp_corners lp_corner_targets(float vx, float vy, lp_radius_params p);
/* Corner i's spring frequency, detuned off `base` so one kick sets all four
 * wobbling out of phase rather than in lockstep. */
float lp_detuned_frequency(float base, int index, float detune);
/* Corner i of c, in the order above. */
float lp_corner_at(lp_corners c, int index);

#endif
