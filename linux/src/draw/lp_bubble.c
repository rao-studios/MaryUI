#include <math.h>

#include "maryui/lp_bubble.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_motion.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

/* The CSS `ease-in-out` keyword; the wave is the only thing that uses it. */
static const lp_cubic_bezier EASE_IN_OUT = { 0.42f, 0.0f, 0.58f, 1.0f };
/* The slosh numbers were tuned against a 12px bead. */
#define BUBBLE_REF 12.0f

lp_bubble_colors lp_bubble_tint_colors(enum lp_bubble_tint tint, const lp_color *ab, const lp_color *ad, const lp_color *al) {
    switch (tint) {
    case LP_TINT_CLOSE: return (lp_bubble_colors){ LP_TRAFFIC_CLOSE_BASE, LP_TRAFFIC_CLOSE_DEEP, LP_TRAFFIC_CLOSE_LIGHT };
    case LP_TINT_MINIMIZE: return (lp_bubble_colors){ LP_TRAFFIC_MINIMIZE_BASE, LP_TRAFFIC_MINIMIZE_DEEP, LP_TRAFFIC_MINIMIZE_LIGHT };
    case LP_TINT_ZOOM: return (lp_bubble_colors){ LP_TRAFFIC_ZOOM_BASE, LP_TRAFFIC_ZOOM_DEEP, LP_TRAFFIC_ZOOM_LIGHT };
    case LP_TINT_INACTIVE: return (lp_bubble_colors){ LP_TRAFFIC_INACTIVE, LP_TRAFFIC_INACTIVE_DEEP, LP_PLATINUM_1 };
    case LP_TINT_ACCENT:
        if (ab && ad && al) return (lp_bubble_colors){ *ab, *ad, *al };
        return (lp_bubble_colors){ LP_ACCENT_BLUE_BASE, LP_ACCENT_BLUE_DEEP, LP_ACCENT_BLUE_LIGHT };
    default: return (lp_bubble_colors){ LP_PLATINUM_4, LP_PLATINUM_7, LP_PLATINUM_0 };
    }
}

/* One iteration of `animation: … ease-in-out infinite alternate`, as an eased
 * 0..1. The phase leads rather than delays — for an endless ping-pong the two
 * are the same shift, and leading keeps the clock positive. */
static float wave_progress(double now_ms, float phase_s, double period_ms, int reverse) {
    double u = (now_ms + (double)phase_s * 1000.0) / period_ms;
    double iter = floor(u);
    double frac = u - iter;
    int backward = ((long long)iter) & 1;
    if (reverse) backward = !backward;
    if (backward) frac = 1.0 - frac;
    return lp_cubic_bezier_eval(EASE_IN_OUT, (float)frac);
}

/* One liquid layer: a 200% blob with four unequal corner radii, rolling
 * sideways and rocking as it goes, filled with the tint's radial gradient. */
static void liquid(cairo_t *cr, float s, const lp_bubble_colors *c, float top, const float radii[4], float shift,
                   float rock_deg, float opacity, float brighten, int surface_light) {
    float bw = 2 * s, bh = 2 * s, bx = -0.5f * s, by = top;
    cairo_save(cr);
    cairo_translate(cr, bx + bw / 2 + shift, by + bh / 2);
    cairo_rotate(cr, rock_deg * M_PI / 180.0);
    cairo_translate(cr, -bw / 2, -bh / 2);
    lp_path_rrect4(cr, LP_RECT(0, 0, bw, bh), radii[0] * bw, radii[1] * bw, radii[2] * bw, radii[3] * bw);
    cairo_clip(cr);

    cairo_pattern_t *p = cairo_pattern_create_radial(0.4 * bw, 0.3 * bh, 0, 0.4 * bw, 0.3 * bh, bw * 0.7);
    lp_color light = lp_color_mix(c->light, LP_RGBA(1, 1, 1, 1), brighten);
    lp_color base = lp_color_mix(c->base, LP_RGBA(1, 1, 1, 1), brighten);
    lp_color deep = lp_color_mix(c->deep, LP_RGBA(1, 1, 1, 1), brighten);
    cairo_pattern_add_color_stop_rgba(p, 0, light.r, light.g, light.b, 1);
    cairo_pattern_add_color_stop_rgba(p, 0.42, base.r, base.g, base.b, 1);
    cairo_pattern_add_color_stop_rgba(p, 1, deep.r, deep.g, deep.b, 1);
    cairo_set_source(cr, p);
    cairo_paint_with_alpha(cr, opacity);
    cairo_pattern_destroy(p);

    /* The light that catches the top of the body, so the waterline reads as a
     * surface rather than an edge. The back layer does without it. */
    if (surface_light) {
        lp_color sl = LP_LIQUID_SURFACE_LIGHT;
        cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, bh * LP_LIQUID_SURFACE_DEPTH);
        cairo_pattern_add_color_stop_rgba(g, 0, sl.r, sl.g, sl.b, sl.a);
        cairo_pattern_add_color_stop_rgba(g, 1, sl.r, sl.g, sl.b, 0);
        cairo_set_source(cr, g);
        cairo_paint_with_alpha(cr, opacity);
        cairo_pattern_destroy(g);
    }
    cairo_restore(cr);
}

void lp_bubble_paint(cairo_t *cr, float cx, float cy, const lp_bubble_spec *spec) {
    float s = spec->size;
    float fill = spec->fill >= 0 ? spec->fill : LP_LIQUID_FILL;
    float scale = s / BUBBLE_REF;
    const lp_bubble_colors *c = &spec->colors;
    cairo_save(cr);
    cairo_translate(cr, cx - s / 2, cy - s / 2);

    cairo_arc(cr, s / 2, s / 2, s / 2, 0, 2 * M_PI);
    cairo_clip(cr);

    /* shell: the empty well is pale glass, so the colour below is the level */
    if (!spec->liquid_only) {
        lp_color inner = lp_color_mix(c->light, LP_RGBA(1, 1, 1, 1), 0.6f);
        lp_color outer = lp_color_mix(c->base, LP_PLATINUM_2, 0.82f);
        /* CSS radial-gradient's default extent is farthest-corner: from 50% 32%
         * of a square that is the bottom two corners. */
        float r = s * hypotf(0.5f, 0.68f);
        cairo_pattern_t *well = cairo_pattern_create_radial(s / 2, s * 0.32f, 0, s / 2, s * 0.32f, r);
        cairo_pattern_add_color_stop_rgba(well, 0, inner.r, inner.g, inner.b, 1);
        cairo_pattern_add_color_stop_rgba(well, 0.88, outer.r, outer.g, outer.b, 1);
        cairo_set_source(cr, well);
        cairo_paint(cr);
        cairo_pattern_destroy(well);
    }

    /* slosh frame: bank and drop the liquid with the window's motion */
    cairo_save(cr);
    cairo_translate(cr, s / 2, s / 2);
    cairo_rotate(cr, spec->slosh_deg * LP_LIQUID_SLOSH_SCALE * M_PI / 180.0);
    cairo_translate(cr, -s / 2 + spec->slosh_x * scale, -s / 2 + spec->slosh_y * scale);

    /* The body rolls from side to side and rocks as it goes; the back layer
     * runs slower and against it, so the two never read as one shape. */
    double period = LP_LIQUID_WAVE_PERIOD_MS;
    float amp = LP_LIQUID_WAVE_AMPLITUDE * s;
    float pf = wave_progress(spec->now_ms, spec->phase_s, period, 0);
    float pb = wave_progress(spec->now_ms, spec->phase_s, period * 1.55, 1);
    static const float front_radii[4] = { 0.44f, 0.46f, 0.40f, 0.42f };
    static const float back_radii[4] = { 0.46f, 0.41f, 0.44f, 0.40f };
    liquid(cr, s, c, (1 - fill) * s - 0.05f * s, back_radii, -amp + 2 * amp * pb, -4 + 8 * pb, LP_LIQUID_OPACITY_BACK, 0.15f, 0);
    liquid(cr, s, c, (1 - fill) * s, front_radii, -amp + 2 * amp * pf, -4 + 8 * pf, LP_LIQUID_OPACITY_FRONT, 0, 1);
    cairo_restore(cr);

    if (!spec->liquid_only) {
        /* gloss: an ellipse at 18% 8%, 46% × 32%, white fading down */
        cairo_save(cr);
        cairo_translate(cr, 0.18f * s + 0.23f * s, 0.08f * s + 0.16f * s);
        cairo_scale(cr, 0.23f * s, 0.16f * s);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_pattern_t *gloss = cairo_pattern_create_linear(0, 0.08f * s, 0, 0.40f * s);
        lp_color g = LP_TRAFFIC_GLOSS;
        cairo_pattern_add_color_stop_rgba(gloss, 0, g.r, g.g, g.b, g.a);
        cairo_pattern_add_color_stop_rgba(gloss, 1, g.r, g.g, g.b, 0);
        cairo_set_source(cr, gloss);
        cairo_fill(cr);
        cairo_pattern_destroy(gloss);

        /* rim: inset 0 1px 2px traffic.rim + inset 0 0 0 .5px rgba(0,0,0,.25) */
        lp_color rim_color = LP_TRAFFIC_RIM;
        const lp_shadow_layer rim[] = { { 1, 0, 1, 2, 0, rim_color }, { 1, 0, 0, 0, 0.5f, { 0, 0, 0, 0.25f } } };
        lp_draw_inset_shadows(cr, LP_RECT(0, 0, s, s), s / 2, rim, 2);

        if (spec->glyph && spec->glyph_alpha > 0) {
            lp_text_style st = lp_text_style_default();
            st.size_px = s * (spec->glyph_scale > 0 ? spec->glyph_scale : 0.72f);
            st.weight = LP_TEXT_WEIGHT_BOLD;
            float ink = spec->glyph_ink > 0 ? spec->glyph_ink : 0.55f;
            st.color = LP_RGBA(0, 0, 0, ink * spec->glyph_alpha);
            st.emboss = 1;
            st.emboss_color = lp_color_with_alpha(LP_INK_EMBOSS, LP_INK_EMBOSS.a * spec->glyph_alpha);
            lp_text_draw(cr, spec->glyph, LP_RECT(0, 0, s, s), &st, LP_ALIGN_CENTER);
        }
    }
    cairo_restore(cr);
}
