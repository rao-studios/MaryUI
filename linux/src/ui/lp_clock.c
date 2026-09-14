#include "maryui/lp_clock.h"

#include <math.h>

#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static lp_text_style clock_style(void) {
    lp_text_style s = lp_text_style_default();
    s.weight = LP_TEXT_WEIGHT_MEDIUM;
    s.tabular_nums = 1;
    s.emboss = 1;
    s.color = LP_INK_PRIMARY;
    return s;
}

lp_rect lp_clock_capsule(cairo_t *cr, lp_rect box, const char *text) {
    lp_text_style s = clock_style();
    float w = ceilf(lp_text_measure(cr, text ? text : "", &s).w) + 2 * LP_CLOCK_PAD;
    if (w > box.w) w = box.w;
    return LP_RECT(floorf(box.x + box.w - w), floorf(box.y + (box.h - LP_CLOCK_H) / 2), w, LP_CLOCK_H);
}

void lp_clock_paint(cairo_t *cr, lp_rect box, const char *text, float world_x, float world_y, const lp_settings *settings) {
    (void)settings;
    lp_rect capsule = lp_clock_capsule(cr, box, text);
    /* a little lift off the wallpaper, then the metal (the menu bar's grade, with its sheen) */
    static const lp_shadow_layer lift[] = { { 0, 0, 1, 3, 0, { 0, 0, 0, 0.28f } } };
    lp_draw_outer_shadows(cr, capsule, LP_RADIUS_PILL, lift, 1);
    lp_surface_paint(cr, capsule, (lp_surface_opts){ .variant = LP_VARIANT_BAR, .radius = LP_RADIUS_PILL, .sheen = 1, .sheen_alpha = -1 },
                     (lp_surface_motion){ .sheen_x = 0.5f, .world_x = world_x, .world_y = world_y });
    lp_path_rrect(cr, LP_RECT(capsule.x + 0.5f, capsule.y + 0.5f, capsule.w - 1, capsule.h - 1), LP_RADIUS_PILL);
    lp_set_color(cr, LP_EDGE_DARK);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    lp_text_style s = clock_style();
    lp_text_draw(cr, text ? text : "", capsule, &s, LP_ALIGN_CENTER);
}
