#include <math.h>

#include "maryui/lp_bubble.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

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

/* One liquid layer: a 200% blob with four unequal corner radii, rotated by
 * the swirl about its own centre, filled with the tint's radial gradient. */
static void liquid(cairo_t *cr, float s, const lp_bubble_colors *c, float top, const float radii[4], double angle, float opacity, float brighten) {
    float bw = 2 * s, bh = 2 * s, bx = -0.5f * s, by = top;
    cairo_save(cr);
    cairo_translate(cr, bx + bw / 2, by + bh / 2);
    cairo_rotate(cr, angle);
    cairo_translate(cr, -bw / 2, -bh / 2);
    lp_path_rrect4(cr, LP_RECT(0, 0, bw, bh), radii[0] * bw, radii[1] * bw, radii[2] * bw, radii[3] * bw);
    cairo_pattern_t *p = cairo_pattern_create_radial(0.4 * bw, 0.3 * bh, 0, 0.4 * bw, 0.3 * bh, bw * 0.7);
    lp_color light = lp_color_mix(c->light, LP_RGBA(1, 1, 1, 1), brighten);
    lp_color base = lp_color_mix(c->base, LP_RGBA(1, 1, 1, 1), brighten);
    lp_color deep = lp_color_mix(c->deep, LP_RGBA(1, 1, 1, 1), brighten);
    cairo_pattern_add_color_stop_rgba(p, 0, light.r, light.g, light.b, 1);
    cairo_pattern_add_color_stop_rgba(p, 0.42, base.r, base.g, base.b, 1);
    cairo_pattern_add_color_stop_rgba(p, 1, deep.r, deep.g, deep.b, 1);
    cairo_set_source(cr, p);
    cairo_clip(cr);
    cairo_paint_with_alpha(cr, opacity);
    cairo_pattern_destroy(p);
    cairo_restore(cr);
}

void lp_bubble_paint(cairo_t *cr, float cx, float cy, const lp_bubble_spec *spec) {
    float s = spec->size;
    float fill = spec->fill >= 0 ? spec->fill : LP_LIQUID_FILL;
    const lp_bubble_colors *c = &spec->colors;
    cairo_save(cr);
    cairo_translate(cr, cx - s / 2, cy - s / 2);

    /* shell: radial well from a lightened deep to deep at 80% */
    cairo_arc(cr, s / 2, s / 2, s / 2, 0, 2 * M_PI);
    cairo_clip(cr);
    lp_color lit = lp_color_mix(c->deep, LP_RGBA(1, 1, 1, 1), 0.3f);
    cairo_pattern_t *well = cairo_pattern_create_radial(s / 2, s * 0.35, 0, s / 2, s * 0.35, s * 0.8);
    cairo_pattern_add_color_stop_rgba(well, 0, lit.r, lit.g, lit.b, 1);
    cairo_pattern_add_color_stop_rgba(well, 1, c->deep.r, c->deep.g, c->deep.b, 1);
    cairo_set_source(cr, well);
    cairo_paint(cr);
    cairo_pattern_destroy(well);

    /* slosh frame: rotate and drop the liquid with the window's acceleration */
    cairo_save(cr);
    cairo_translate(cr, s / 2, s / 2);
    cairo_rotate(cr, spec->slosh_deg * M_PI / 180.0);
    cairo_translate(cr, -s / 2, -s / 2 + spec->slosh_y);
    double period = LP_MOTION_SWIRL_PERIOD_MS;
    double t = spec->now_ms / period + spec->phase_s * 1000.0 / period;
    double angle_front = 2 * M_PI * fmod(t, 1.0);
    double angle_back = -2 * M_PI * fmod(t / 1.6, 1.0);
    static const float front_radii[4] = { 0.42f, 0.45f, 0.40f, 0.44f };
    static const float back_radii[4] = { 0.45f, 0.40f, 0.44f, 0.41f };
    liquid(cr, s, c, (1 - fill) * s - 0.05f * s, back_radii, angle_back, LP_LIQUID_OPACITY_BACK, 0.15f);
    liquid(cr, s, c, (1 - fill) * s, front_radii, angle_front, LP_LIQUID_OPACITY_FRONT, 0);
    cairo_restore(cr);

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
    static const lp_shadow_layer rim[] = { { 1, 0, 1, 2, 0, { 0, 0, 0, 0.32f } }, { 1, 0, 0, 0, 0.5f, { 0, 0, 0, 0.25f } } };
    lp_draw_inset_shadows(cr, LP_RECT(0, 0, s, s), s / 2, rim, 2);

    if (spec->glyph && spec->glyph_alpha > 0) {
        lp_text_style st = lp_text_style_default();
        st.size_px = s * 0.8f;
        st.weight = LP_TEXT_WEIGHT_BOLD;
        st.color = LP_RGBA(0, 0, 0, 0.55f * spec->glyph_alpha);
        lp_text_draw(cr, spec->glyph, LP_RECT(0, 0, s, s), &st, LP_ALIGN_CENTER);
    }
    cairo_restore(cr);
}
