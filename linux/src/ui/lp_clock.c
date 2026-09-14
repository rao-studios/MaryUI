#include "maryui/lp_clock.h"

#include <math.h>
#include <stdio.h>

#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static lp_text_style clock_style(void) {
    lp_text_style s = lp_text_style_default();
    s.size_px = LP_CLOCK_SIZE;
    s.weight = LP_TEXT_WEIGHT_BOLD;
    s.tabular_nums = 1;
    return s;
}

void lp_clock_format(const struct tm *tm, int hours24, char *out, size_t n) {
    static const char *const DAYS[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char *const MONTHS[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const char *day = tm->tm_wday >= 0 && tm->tm_wday < 7 ? DAYS[tm->tm_wday] : "";
    const char *month = tm->tm_mon >= 0 && tm->tm_mon < 12 ? MONTHS[tm->tm_mon] : "";
    int hour = tm->tm_hour % 12;
    if (hour == 0) hour = 12;
    if (hours24) snprintf(out, n, "%s %s %d %02d:%02d", day, month, tm->tm_mday, tm->tm_hour, tm->tm_min);
    else snprintf(out, n, "%s %s %d %d:%02d %s", day, month, tm->tm_mday, hour, tm->tm_min, tm->tm_hour < 12 ? "AM" : "PM");
}

lp_rect lp_clock_bounds(cairo_t *cr, lp_rect box, const char *text) {
    lp_text_style s = clock_style();
    lp_size m = lp_text_measure(cr, text ? text : "", &s);
    float room = box.w - LP_CLOCK_INSET, w = ceilf(m.w) < room ? ceilf(m.w) : room, h = ceilf(m.h) < box.h ? ceilf(m.h) : box.h;
    return LP_RECT(floorf(box.x + box.w - LP_CLOCK_INSET - w), floorf(box.y + (box.h - h) / 2), w, h);
}

lp_rect lp_clock_hit_rect(cairo_t *cr, lp_rect box, const char *text) {
    lp_rect b = lp_clock_bounds(cr, box, text);
    float x0 = b.x - LP_CLOCK_HIT_PAD, y0 = b.y - LP_CLOCK_HIT_PAD, x1 = b.x + b.w + LP_CLOCK_HIT_PAD, y1 = b.y + b.h + LP_CLOCK_HIT_PAD;
    if (x0 < box.x) x0 = box.x;
    if (y0 < box.y) y0 = box.y;
    if (x1 > box.x + box.w) x1 = box.x + box.w;
    if (y1 > box.y + box.h) y1 = box.y + box.h;
    return LP_RECT(x0, y0, x1 - x0, y1 - y0);
}

void lp_clock_paint(cairo_t *cr, lp_rect box, const char *text, float world_x, float world_y, const lp_settings *settings) {
    (void)settings;
    const char *t = text ? text : "";
    lp_rect line = lp_clock_bounds(cr, box, t);
    lp_text_style s = clock_style();
    cairo_save(cr);
    /* the letters lifted off the wallpaper: their own shape a pixel and two below, dark and soft */
    static const struct { float dy, alpha; } drop[] = { { 2, 0.16f }, { 1, 0.40f } };
    for (int i = 0; i < 2; i++) {
        cairo_new_path(cr);
        cairo_save(cr);
        cairo_translate(cr, 0, drop[i].dy);
        lp_text_path(cr, t, line, &s, LP_ALIGN_END);
        cairo_restore(cr);                         /* the path keeps the offset it was built with */
        cairo_set_source_rgba(cr, 0, 0, 0, drop[i].alpha);
        cairo_fill(cr);
    }
    /* a dark keyline round the letters (its inner half is covered by the metal), so pale metal reads on pale wallpaper */
    cairo_new_path(cr);
    lp_text_path(cr, t, line, &s, LP_ALIGN_END);
    cairo_set_line_width(cr, 1.6);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_source_rgba(cr, 0.09, 0.10, 0.12, 0.55);
    cairo_stroke_preserve(cr);
    /* the metal: the glyphs are the clip, and the desktop's sheet shows through them, grain, sheen and bevel */
    double x1, y1, x2, y2;
    cairo_path_extents(cr, &x1, &y1, &x2, &y2);
    cairo_clip(cr);
    lp_rect metal = LP_RECT((float)floor(x1) - 1, (float)floor(y1) - 1, (float)(ceil(x2) - floor(x1)) + 2, (float)(ceil(y2) - floor(y1)) + 2);
    lp_surface_paint(cr, metal, (lp_surface_opts){ .variant = LP_VARIANT_BAR, .radius = 0, .sheen = 1, .sheen_alpha = -1 },
                     (lp_surface_motion){ .sheen_x = 0.35f, .world_x = world_x, .world_y = world_y });
    cairo_restore(cr);
}
