#include "maryui/lp_launchpad.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "maryui/lp_blur.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_object_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define BACKDROP_SCALE 4
#define BACKDROP_SIGMA 4.0f
#define SCRIM ((lp_color){ 0.90f, 0.91f, 0.93f, 0.42f })

cairo_surface_t *lp_launchpad_backdrop(cairo_surface_t *wallpaper, int w, int h) {
    if (!wallpaper || w <= 0 || h <= 0) return NULL;
    int sw = w / BACKDROP_SCALE > 0 ? w / BACKDROP_SCALE : 1, sh = h / BACKDROP_SCALE > 0 ? h / BACKDROP_SCALE : 1;
    cairo_surface_t *small = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sw, sh);
    cairo_t *cr = cairo_create(small);
    cairo_scale(cr, (double)sw / w, (double)sh / h);
    cairo_set_source_surface(cr, wallpaper, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_destroy(cr);
    lp_blur_surface(small, BACKDROP_SIGMA);
    cairo_surface_t *out = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cr = cairo_create(out);
    cairo_scale(cr, (double)w / sw, (double)h / sh);
    cairo_set_source_surface(cr, small, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(small);
    return out;
}

/* One app on the grid: its mark at the lit tier, its name under it, a dot when it has a window. */
static void tile(lp_ctx *ctx, lp_id id, lp_rect cell, const lp_spotlight_item *it, int selected, lp_launchpad_result *res, int index) {
    if (lp_clicked(ctx, id, cell)) res->activated = index;
    if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, cell)) res->hovered = index;
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr || !lp_clip_intersects(ctx->cr, cell)) return;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    if (selected) {
        lp_fill_solid(cr, cell, accent.soft, LP_RADIUS_MD);
        static lp_shadow_layer ring[1] = { { 1, 0, 0, 0, 1, { 0, 0, 0, 1 } } };
        ring[0].color = accent.light;
        lp_draw_inset_shadows(cr, cell, LP_RADIUS_MD, ring, 1);
    } else if (lp_is_hot(ctx, id)) {
        lp_fill_solid(cr, cell, LP_RGBA(1, 1, 1, 0.22f), LP_RADIUS_MD);
    }
    lp_rect dot = LP_RECT(cell.x + cell.w / 2 - 2, cell.y + cell.h - 10, 4, 4);
    lp_rect label = LP_RECT(cell.x + 4, dot.y - 4 - 16, cell.w - 8, 16);
    float above = label.y - LP_SPACE_1 - cell.y;
    lp_rect mark = LP_RECT(roundf(cell.x + (cell.w - LP_LAUNCHPAD_MARK) / 2), roundf(cell.y + (above - LP_LAUNCHPAD_MARK) / 2), LP_LAUNCHPAD_MARK, LP_LAUNCHPAD_MARK);
    lp_object named = it->object ? lp_object_by_name(it->object) : LP_OBJ_COUNT;
    if (named < LP_OBJ_COUNT) lp_object_icon_draw(cr, named, mark, ctx->settings);
    else lp_icon_paint_object(cr, it->icon, mark, 1.6f, LP_INK_SECONDARY, ctx->settings);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.weight = LP_TEXT_WEIGHT_MEDIUM;
    st.emboss = 1;
    st.ellipsize = 1;
    lp_text_draw(cr, it->title, label, &st, LP_ALIGN_CENTER);
    if (it->running) lp_fill_solid(cr, dot, accent.base, 2);
}

void lp_launchpad_panel(lp_ctx *ctx, const lp_launchpad_view *v, lp_launchpad_result *out) {
    lp_launchpad_result res = { .hovered = -1, .activated = -1, .page_pressed = -1 };
    lp_id base = LP_ID("launchpad");
    int draw = ctx->pass == LP_PASS_DRAW && ctx->cr;
    cairo_t *cr = ctx->cr;
    lp_rect screen = LP_RECT(0, 0, v->width, v->height);
    res.columns = lp_launchpad_columns(v->width);
    res.rows = lp_launchpad_rows(v->height);
    int per_page = res.columns * res.rows;
    res.pages = v->count > 0 ? (v->count + per_page - 1) / per_page : 1;
    int page = v->page < 0 ? 0 : v->page >= res.pages ? res.pages - 1 : v->page;

    if (draw) {
        if (v->backdrop) { cairo_set_source_surface(cr, v->backdrop, 0, 0); cairo_paint(cr); }
        else lp_fill_solid(cr, screen, LP_PLATINUM_3, 0);
        lp_fill_solid(cr, screen, SCRIM, 0);
    }
    /* the field */
    lp_rect field = LP_RECT(roundf((v->width - LP_LAUNCHPAD_SEARCH_W) / 2), LP_LAUNCHPAD_SEARCH_Y, LP_LAUNCHPAD_SEARCH_W, LP_LAUNCHPAD_SEARCH_H);
    static lp_text_buffer scratch;
    lp_text_buffer *q = v->query ? v->query : &scratch;
    res.query_changed = lp_text_field(ctx, LP_LAUNCHPAD_QUERY_ID, field, q, (lp_text_field_opts){ .placeholder = "Search", .icon = LP_ICON_SEARCH, .round = 1, .large = 1 });
    if (ctx->pass == LP_PASS_EVENT && v->focus_field) ctx->focus = LP_LAUNCHPAD_QUERY_ID;
    /* the grid, centred in what is left */
    int first = page * per_page, on_page = v->count - first;
    if (on_page > per_page) on_page = per_page;
    if (on_page < 0) on_page = 0;
    int rows_used = (on_page + res.columns - 1) / res.columns;
    float grid_w = res.columns * LP_LAUNCHPAD_CELL_W, grid_h = rows_used * LP_LAUNCHPAD_CELL_H;
    float avail = v->height - LP_LAUNCHPAD_GRID_TOP - LP_LAUNCHPAD_DOTS_H;
    float x0 = roundf((v->width - grid_w) / 2), y0 = roundf(LP_LAUNCHPAD_GRID_TOP + (avail - grid_h) / 2);
    int over_tile = 0;
    for (int i = 0; i < on_page; i++) {
        int index = first + i;
        lp_rect cell = LP_RECT(x0 + (i % res.columns) * LP_LAUNCHPAD_CELL_W, y0 + (i / res.columns) * LP_LAUNCHPAD_CELL_H, LP_LAUNCHPAD_CELL_W, LP_LAUNCHPAD_CELL_H);
        tile(ctx, lp_id_index(base, 100 + index), cell, &v->items[index], v->selection == index, &res, index);
        if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, cell)) over_tile = 1;
    }
    if (!v->count && draw) {
        lp_text_style st = lp_text_style_default();
        st.color = LP_INK_SECONDARY;
        st.emboss = 1;
        char msg[320];
        snprintf(msg, sizeof msg, "No application named \xE2\x80\x9C%s\xE2\x80\x9D.", q->text);
        lp_text_draw(cr, msg, LP_RECT(0, LP_LAUNCHPAD_GRID_TOP, v->width, 40), &st, LP_ALIGN_CENTER);
    }
    /* the page dots */
    int over_dots = 0;
    if (res.pages > 1) {
        float dw = 8, gap = 10, total = res.pages * dw + (res.pages - 1) * gap;
        float dx = roundf((v->width - total) / 2), dy = v->height - LP_LAUNCHPAD_DOTS_H / 2 - dw / 2;
        for (int p = 0; p < res.pages; p++) {
            lp_rect r = LP_RECT(dx + p * (dw + gap) - 4, dy - 4, dw + 8, dw + 8);
            if (lp_clicked(ctx, lp_id_index(base, 50 + p), r)) res.page_pressed = p;
            if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, r)) over_dots = 1;
            if (draw) {
                lp_accent accent = lp_settings_accent(ctx->settings);
                cairo_arc(cr, r.x + r.w / 2, r.y + r.h / 2, dw / 2, 0, 2 * M_PI);
                lp_set_color(cr, p == page ? accent.base : LP_RGBA(0, 0, 0, 0.22f));
                cairo_fill(cr);
            }
        }
    }
    /* a click on nothing dismisses */
    if (ctx->pass == LP_PASS_EVENT && (ctx->in.pressed & LP_BUTTON_LEFT) && !over_tile && !over_dots && !lp_hit(ctx, field) && lp_hit(ctx, screen)) res.dismissed = 1;
    if (out) *out = res;
}
