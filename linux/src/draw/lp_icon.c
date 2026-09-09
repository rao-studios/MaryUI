#include <string.h>

#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_svgpath.h"
#include "maryui/lp_tokens.h"

void lp_icon_draw(cairo_t *cr, lp_icon icon, float x, float y, float size, float stroke_width, lp_color color) {
    if (icon >= LP_ICON_COUNT) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / LP_ICON_VIEWBOX, size / LP_ICON_VIEWBOX);
    cairo_new_path(cr);
    for (int i = 0; LP_ICON_PATHS[icon][i]; i++) lp_svgpath_apply(cr, LP_ICON_PATHS[icon][i]);
    cairo_set_line_width(cr, stroke_width > 0 ? stroke_width : LP_ICON_STROKE);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    lp_set_color(cr, color);
    cairo_stroke(cr);
    cairo_restore(cr);
}

lp_icon lp_icon_by_name(const char *name) {
    for (int i = 0; i < LP_ICON_COUNT; i++) if (strcmp(LP_ICON_NAMES[i], name) == 0) return (lp_icon)i;
    return LP_ICON_COUNT;
}

/* finder.c's tile, shared with the drag ghost and the Info window. */
void lp_file_icon_paint(cairo_t *cr, lp_rect r, lp_icon icon, int folder, lp_accent accent) {
    static const lp_shadow_layer icon_shadow[] = { { 0, 0, 0, 0, 1, { 0, 0, 0, 0.32f } }, { 0, 0, 2, 4, 0, { 0, 0, 0, 0.15f } } };
    lp_draw_outer_shadows(cr, r, LP_RADIUS_MD, icon_shadow, 2);
    if (folder) lp_fill_vgradient(cr, r, accent.soft, LP_PLATINUM_2, LP_RADIUS_MD);
    else lp_fill_vgradient(cr, r, LP_PLATINUM_0, LP_PLATINUM_3, LP_RADIUS_MD);
    lp_draw_inset_shadows(cr, r, LP_RADIUS_MD, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
    float inset = r.w * 9.0f / 52.0f, size = r.w - 2 * inset;
    lp_icon_draw(cr, icon, r.x + inset, r.y + inset, size, 1.2f, folder ? accent.base : LP_INK_SECONDARY);
}
