#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <pango/pangocairo.h>

#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

const char *lp_font_families(enum lp_font font) {
    switch (font) {
    case LP_FONT_DISPLAY: return LP_FONT_DISPLAY_PANGO;
    case LP_FONT_MONO: return LP_FONT_MONO_PANGO;
    default: return LP_FONT_UI_PANGO;
    }
}

lp_text_style lp_text_style_default(void) {
    return (lp_text_style){
        .font = LP_FONT_UI, .size_px = LP_TEXT_MD, .weight = LP_TEXT_WEIGHT_REGULAR, .color = LP_INK_PRIMARY,
        .emboss = 0, .emboss_color = LP_INK_EMBOSS, .tabular_nums = 0, .letter_spacing = 0, .uppercase = 0, .ellipsize = 0,
    };
}

/* A cairo context for measuring when the caller has none (the EVENT pass): an
 * image surface like every chrome buffer, so the metrics are the same. */
static cairo_t *scratch_cr(void) {
    static cairo_t *cr;
    if (!cr) {
        cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        cr = cairo_create(s);
        cairo_surface_destroy(s);
    }
    return cr;
}

static PangoLayout *make_layout_ex(cairo_t *cr, const char *text, int len, const lp_text_style *style, float width,
                                   int single_paragraph, int wrap) {
    PangoLayout *layout = pango_cairo_create_layout(cr ? cr : scratch_cr());
    PangoFontDescription *desc = pango_font_description_new();
    pango_font_description_set_family(desc, lp_font_families(style->font));
    pango_font_description_set_absolute_size(desc, style->size_px * PANGO_SCALE);
    pango_font_description_set_weight(desc, (PangoWeight)style->weight);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    char *upper = NULL;
    if (style->uppercase) {
        upper = len < 0 ? strdup(text) : strndup(text, (size_t)len);
        for (char *p = upper; *p; p++) *p = (char)toupper((unsigned char)*p);
        text = upper;
        len = -1;
    }
    pango_layout_set_text(layout, text, len);
    free(upper);

    PangoAttrList *attrs = pango_attr_list_new();
    if (style->tabular_nums) pango_attr_list_insert(attrs, pango_attr_font_features_new("tnum=1"));
    if (style->letter_spacing != 0) pango_attr_list_insert(attrs, pango_attr_letter_spacing_new((int)(style->letter_spacing * PANGO_SCALE)));
    pango_layout_set_attributes(layout, attrs);
    pango_attr_list_unref(attrs);

    pango_layout_set_single_paragraph_mode(layout, single_paragraph ? TRUE : FALSE);
    if (wrap && width > 0) {
        pango_layout_set_width(layout, (int)(width * PANGO_SCALE));
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    } else if (style->ellipsize && width > 0) {
        pango_layout_set_width(layout, (int)(width * PANGO_SCALE));
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    }
    return layout;
}

static PangoLayout *make_layout(cairo_t *cr, const char *text, const lp_text_style *style, float width) {
    return make_layout_ex(cr, text, -1, style, width, 1, 0);
}

lp_size lp_text_measure(cairo_t *cr, const char *text, const lp_text_style *style) {
    PangoLayout *layout = make_layout(cr, text, style, 0);
    PangoRectangle ink, logical;
    pango_layout_get_pixel_extents(layout, &ink, &logical);
    g_object_unref(layout);
    return (lp_size){ (float)logical.width, (float)logical.height };
}

void lp_text_draw(cairo_t *cr, const char *text, lp_rect r, const lp_text_style *style, enum lp_align align) {
    PangoLayout *layout = make_layout(cr, text, style, r.w);
    PangoRectangle ink, logical;
    pango_layout_get_pixel_extents(layout, &ink, &logical);
    double x = r.x;
    if (align == LP_ALIGN_CENTER) x = r.x + (r.w - logical.width) / 2.0;
    else if (align == LP_ALIGN_END) x = r.x + r.w - logical.width;
    double y = r.y + (r.h - logical.height) / 2.0;
    x = floor(x + 0.5);
    y = floor(y + 0.5);
    if (style->emboss) {
        cairo_move_to(cr, x, y + 1);
        cairo_set_source_rgba(cr, style->emboss_color.r, style->emboss_color.g, style->emboss_color.b, style->emboss_color.a);
        pango_cairo_show_layout(cr, layout);
    }
    cairo_move_to(cr, x, y);
    cairo_set_source_rgba(cr, style->color.r, style->color.g, style->color.b, style->color.a);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

void lp_text_draw_at(cairo_t *cr, const char *text, float x, float y, const lp_text_style *style) {
    PangoLayout *layout = make_layout(cr, text, style, 0);
    int baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;
    if (style->emboss) {
        cairo_move_to(cr, x, y - baseline + 1);
        cairo_set_source_rgba(cr, style->emboss_color.r, style->emboss_color.g, style->emboss_color.b, style->emboss_color.a);
        pango_cairo_show_layout(cr, layout);
    }
    cairo_move_to(cr, x, y - baseline);
    cairo_set_source_rgba(cr, style->color.r, style->color.g, style->color.b, style->color.a);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

/* MARK: - Multi-line layouts */

struct lp_text_layout {
    PangoLayout *layout;
    const char *text;   /* pango's copy */
};

lp_text_layout *lp_text_layout_new(cairo_t *cr, const char *text, int len, const lp_text_style *style, float wrap_width) {
    lp_text_layout *l = calloc(1, sizeof *l);
    l->layout = make_layout_ex(cr, text ? text : "", len, style, wrap_width, 0, wrap_width > 0);
    l->text = pango_layout_get_text(l->layout);
    return l;
}

void lp_text_layout_free(lp_text_layout *l) {
    if (!l) return;
    g_object_unref(l->layout);
    free(l);
}

lp_size lp_text_layout_size(const lp_text_layout *l) {
    PangoRectangle ink, logical;
    pango_layout_get_pixel_extents(l->layout, &ink, &logical);
    return (lp_size){ (float)logical.width, (float)logical.height };
}

int lp_text_layout_line_count(const lp_text_layout *l) { return pango_layout_get_line_count(l->layout); }

static int clamp_index(const lp_text_layout *l, int index) {
    int len = (int)strlen(l->text);
    return index < 0 ? 0 : index > len ? len : index;
}

lp_rect lp_text_layout_index_to_pos(const lp_text_layout *l, int index) {
    PangoRectangle pos;
    pango_layout_index_to_pos(l->layout, clamp_index(l, index), &pos);
    float x = (float)pos.x / PANGO_SCALE;
    if (pos.width < 0) x += (float)pos.width / PANGO_SCALE; /* right-to-left runs */
    return LP_RECT(x, (float)pos.y / PANGO_SCALE, 1, (float)pos.height / PANGO_SCALE);
}

static int with_trailing(const lp_text_layout *l, int index, int trailing) {
    const char *p = l->text + index;
    while (trailing-- > 0 && *p) p = g_utf8_next_char(p);
    return (int)(p - l->text);
}

int lp_text_layout_xy_to_index(const lp_text_layout *l, float x, float y) {
    int index = 0, trailing = 0;
    pango_layout_xy_to_index(l->layout, (int)(x * PANGO_SCALE), (int)(y * PANGO_SCALE), &index, &trailing);
    return clamp_index(l, with_trailing(l, index, trailing));
}

void lp_text_layout_line_bounds(const lp_text_layout *l, int index, int *start, int *end) {
    int line_no = 0, x_pos = 0;
    pango_layout_index_to_line_x(l->layout, clamp_index(l, index), FALSE, &line_no, &x_pos);
    PangoLayoutLine *line = pango_layout_get_line_readonly(l->layout, line_no);
    int s = line->start_index, e = line->start_index + line->length;
    while (e > s && (l->text[e - 1] == '\n' || l->text[e - 1] == '\r')) e--;
    if (start) *start = s;
    if (end) *end = e;
}

int lp_text_layout_move_line(const lp_text_layout *l, int index, int delta, float x) {
    int line_no = 0, x_pos = 0;
    pango_layout_index_to_line_x(l->layout, clamp_index(l, index), FALSE, &line_no, &x_pos);
    int target = line_no + delta;
    if (target < 0 || target >= pango_layout_get_line_count(l->layout)) return -1;
    PangoLayoutLine *line = pango_layout_get_line_readonly(l->layout, target);
    int idx = 0, trailing = 0;
    pango_layout_line_x_to_index(line, (int)(x * PANGO_SCALE), &idx, &trailing);
    return clamp_index(l, with_trailing(l, idx, trailing));
}

int lp_text_layout_range_rects(const lp_text_layout *l, int start, int end, lp_rect *out, int max) {
    start = clamp_index(l, start);
    end = clamp_index(l, end);
    if (end <= start || max <= 0) return 0;
    int n = 0;
    PangoLayoutIter *iter = pango_layout_get_iter(l->layout);
    do {
        PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
        int ls = line->start_index, le = line->start_index + line->length;
        if (le < start || ls > end) continue;
        if (ls == le && !(start <= ls && ls < end)) continue;
        PangoRectangle logical;
        pango_layout_iter_get_line_extents(iter, NULL, &logical);
        int *ranges = NULL, count = 0;
        int s = start > ls ? start : ls, e = end < le ? end : le;
        if (e < s) continue;
        pango_layout_line_get_x_ranges(line, s, e, &ranges, &count);
        for (int i = 0; i < count && n < max; i++) {
            float x0 = (float)ranges[2 * i] / PANGO_SCALE, x1 = (float)ranges[2 * i + 1] / PANGO_SCALE;
            out[n++] = LP_RECT(x0, (float)logical.y / PANGO_SCALE, x1 - x0, (float)logical.height / PANGO_SCALE);
        }
        if (count == 0 && ls < end && le <= end && n < max) {
            /* a selected paragraph break: a sliver at the line's end */
            float x0 = (float)(logical.x + logical.width) / PANGO_SCALE;
            out[n++] = LP_RECT(x0, (float)logical.y / PANGO_SCALE, 4, (float)logical.height / PANGO_SCALE);
        }
        g_free(ranges);
    } while (pango_layout_iter_next_line(iter));
    pango_layout_iter_free(iter);
    return n;
}

void lp_text_layout_draw(cairo_t *cr, const lp_text_layout *l, float x, float y, lp_color color) {
    cairo_move_to(cr, x, y);
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
    pango_cairo_show_layout(cr, l->layout);
}
