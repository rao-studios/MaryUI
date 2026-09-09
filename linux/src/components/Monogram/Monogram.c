#include <math.h>
#include <pango/pangocairo.h>

#include "maryui/components/lp_monogram.h"
#include "maryui/lp_blur.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"

/* The symbol: viewBox 0 0 200 200, two "R" glyphs at 150px bold, one mirrored
 * about x = 66 (translate(132) scale(-1 1)), both with the baseline at y = 156. */
static void mark_path(cairo_t *cr, double scale) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc = pango_font_description_new();
    pango_font_description_set_family(desc, lp_font_families(LP_FONT_DISPLAY));
    pango_font_description_set_absolute_size(desc, 150 * scale * PANGO_SCALE);
    pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_text(layout, "R", -1);
    PangoRectangle ink, logical;
    pango_layout_get_pixel_extents(layout, &ink, &logical);
    int baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;
    double half = logical.width / 2.0; /* text-anchor="middle" */

    cairo_new_path(cr);
    /* right R: x = 134 */
    cairo_save(cr);
    cairo_translate(cr, 134 * scale - half, 156 * scale - baseline);
    pango_cairo_layout_path(cr, layout);
    cairo_restore(cr);
    /* mirrored R: x = 66 under translate(132 0) scale(-1 1) → mirrored about x = 66 */
    cairo_save(cr);
    cairo_translate(cr, 132 * scale, 0);
    cairo_scale(cr, -1, 1);
    cairo_translate(cr, 66 * scale - half, 156 * scale - baseline);
    pango_cairo_layout_path(cr, layout);
    cairo_restore(cr);
    g_object_unref(layout);
}

void lp_monogram_paint(cairo_t *cr, lp_rect box, enum lp_monogram_variant variant, lp_color color) {
    double scale = fmin(box.w, box.h) / 200.0;
    cairo_save(cr);
    cairo_translate(cr, box.x + (box.w - 200 * scale) / 2, box.y + (box.h - 200 * scale) / 2);
    if (variant == LP_MONOGRAM_FLAT) {
        mark_path(cr, scale);
        lp_set_color(cr, color);
        cairo_fill(cr);
        cairo_restore(cr);
        return;
    }
    /* platinum: drop shadow, gradient fill, brushed overlay, diagonal specular, 1px rim. */
    lp_rect b = LP_RECT(0, 0, 200 * scale, 200 * scale);
    cairo_surface_t *shadow = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)(b.w + 40 * scale), (int)(b.h + 40 * scale));
    cairo_t *sc = cairo_create(shadow);
    cairo_translate(sc, 20 * scale, 20 * scale + 4 * scale);
    mark_path(sc, scale);
    cairo_set_source_rgba(sc, 0, 0, 0, 0.35);
    cairo_fill(sc);
    cairo_destroy(sc);
    lp_blur_surface(shadow, 4 * (float)scale);
    cairo_set_source_surface(cr, shadow, -20 * scale, -20 * scale);
    cairo_paint(cr);
    cairo_surface_destroy(shadow);

    mark_path(cr, scale);
    cairo_pattern_t *grad = cairo_pattern_create_linear(0, 0, 0, b.h);
    lp_color p0 = LP_PLATINUM_0, p4 = LP_PLATINUM_4, p6 = LP_PLATINUM_6, p3 = LP_PLATINUM_3;
    cairo_pattern_add_color_stop_rgba(grad, 0, p0.r, p0.g, p0.b, 1);
    cairo_pattern_add_color_stop_rgba(grad, 0.45, p4.r, p4.g, p4.b, 1);
    cairo_pattern_add_color_stop_rgba(grad, 0.5, p6.r, p6.g, p6.b, 1);
    cairo_pattern_add_color_stop_rgba(grad, 1, p3.r, p3.g, p3.b, 1);
    cairo_set_source(cr, grad);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(grad);

    cairo_save(cr);
    cairo_clip_preserve(cr);
    cairo_pattern_t *tile = cairo_pattern_create_for_surface(lp_brush_tile());
    cairo_pattern_set_extend(tile, CAIRO_EXTEND_REPEAT);
    cairo_matrix_t m;
    cairo_matrix_init_scale(&m, 512.0 / (128 * scale), 512.0 / (128 * scale));
    cairo_pattern_set_matrix(tile, &m);
    cairo_set_source(cr, tile);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVERLAY);
    cairo_paint_with_alpha(cr, 0.9);
    cairo_pattern_destroy(tile);
    cairo_pattern_t *spec = cairo_pattern_create_linear(0, 0, b.w, b.h);
    cairo_pattern_add_color_stop_rgba(spec, 0.3, 1, 1, 1, 0);
    cairo_pattern_add_color_stop_rgba(spec, 0.5, 1, 1, 1, 0.7);
    cairo_pattern_add_color_stop_rgba(spec, 0.7, 1, 1, 1, 0);
    cairo_set_source(cr, spec);
    cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);
    cairo_paint(cr);
    cairo_pattern_destroy(spec);
    cairo_restore(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void lp_monogram(lp_ctx *ctx, lp_rect box, enum lp_monogram_variant variant, lp_color color) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_monogram_paint(ctx->cr, box, variant, color);
}
