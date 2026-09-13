#include "maryui/lp_brush.h"

#include <math.h>
#include <string.h>

int64_t lp_brush_hash(const char *s) {
    int64_t h = 5381;
    for (const unsigned char *p = (const unsigned char *)s; s && *p; p++) h = (int64_t)((uint64_t)h * 33u + *p);
    return h;
}

int64_t lp_brush_seed(const char *span_id, int line) { return lp_brush_hash(span_id) ^ (int64_t)line; }

lp_color lp_brush_palette(int index) {
    static const lp_color PALETTE[LP_BRUSH_PALETTE] = {
        { 0.68f, 0.56f, 0.38f, 1 },     /* mary gold */
        { 0.22f, 0.44f, 0.65f, 1 },     /* muted blue */
        { 0.38f, 0.55f, 0.38f, 1 },     /* sage green */
        { 0.65f, 0.40f, 0.55f, 1 },     /* muted mauve */
        { 0.94f, 0.56f, 0.68f, 1 },     /* warm pink */
    };
    return PALETTE[((index % LP_BRUSH_PALETTE) + LP_BRUSH_PALETTE) % LP_BRUSH_PALETTE];
}

lp_color lp_brush_color(const char *owner_id) {
    int64_t h = lp_brush_hash(owner_id);
    return lp_brush_palette((int)((h < 0 ? -h : h) % LP_BRUSH_PALETTE));
}

/* SeededRNG: splitmix64 from seed + 0x9E3779B97F4A7C1. */
struct rng { uint64_t state; };

static uint64_t next_raw(struct rng *r) {
    r->state += 0x9E3779B97F4A7C15ull;
    uint64_t z = r->state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static double next_in(struct rng *r, double lo, double hi) {
    double normalized = (double)next_raw(r) / 18446744073709551615.0;
    return lo + normalized * (hi - lo);
}

void lp_brush_path(cairo_t *cr, lp_rect rect, int64_t seed) {
    struct rng rng = { (uint64_t)(seed + 0x9E3779B97F4A7C1ll) };
    const int steps = 2;
    float step_w = rect.w / steps, wobble = rect.h * 0.25f;
    float skew_y = (float)next_in(&rng, -0.8, 0.8);
    float top_x[3], top_y[3], bot_x[3], bot_y[3];
    for (int i = 0; i <= steps; i++) {
        top_x[i] = rect.x + step_w * i;
        float base = rect.y + skew_y * ((float)i / steps - 0.5f);
        top_y[i] = base + (float)next_in(&rng, -wobble, wobble * 0.4);
    }
    for (int i = 0; i <= steps; i++) {
        bot_x[i] = rect.x + step_w * i;
        float base = rect.y + rect.h + skew_y * ((float)i / steps - 0.5f);
        bot_y[i] = base + (float)next_in(&rng, -wobble * 0.4, wobble);
    }
    float taper = rect.h * 0.22f;
    top_y[0] += taper;
    bot_y[0] -= taper;
    top_y[steps] += taper * 0.65f;
    bot_y[steps] -= taper * 0.65f;
    /* the top edge, left to right, as quadratic curves with the control at the previous height */
    cairo_move_to(cr, top_x[0], top_y[0]);
    for (int i = 1; i <= steps; i++) {
        double cx = (top_x[i - 1] + top_x[i]) / 2.0, cy = top_y[i - 1];
        double x0, y0;
        cairo_get_current_point(cr, &x0, &y0);
        cairo_curve_to(cr, x0 + 2.0 / 3.0 * (cx - x0), y0 + 2.0 / 3.0 * (cy - y0), top_x[i] + 2.0 / 3.0 * (cx - top_x[i]),
                       top_y[i] + 2.0 / 3.0 * (cy - top_y[i]), top_x[i], top_y[i]);
    }
    /* the bottom edge, back */
    cairo_line_to(cr, bot_x[steps], bot_y[steps]);
    for (int i = steps - 1; i >= 0; i--) {
        double cx = (bot_x[i + 1] + bot_x[i]) / 2.0, cy = bot_y[i + 1];
        double x0, y0;
        cairo_get_current_point(cr, &x0, &y0);
        cairo_curve_to(cr, x0 + 2.0 / 3.0 * (cx - x0), y0 + 2.0 / 3.0 * (cy - y0), bot_x[i] + 2.0 / 3.0 * (cx - bot_x[i]),
                       bot_y[i] + 2.0 / 3.0 * (cy - bot_y[i]), bot_x[i], bot_y[i]);
    }
    cairo_close_path(cr);
}

float lp_brush_opacity(double now_ms, double since_ms, int idx, int instant) {
    if (instant || since_ms <= 0) return 1;
    double t = (now_ms - since_ms - idx * LP_BRUSH_STAGGER_MS) / LP_BRUSH_FADE_MS;
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return (float)(t * t);      /* easeIn */
}
