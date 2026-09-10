#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_shadow.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define EMPTY_H 40
#define LIST_PAD LP_SPACE_1

int lp_spotlight_query_is_blank(const char *query) {
    for (; query && *query; query++) if (!isspace((unsigned char)*query)) return 0;
    return 1;
}

static float width_of(const lp_spotlight_view *v) { return v->width > 0 ? v->width : LP_SIZE_SPOTLIGHT_WIDTH; }
static float dock_height(void) { return 1 + LP_SPOTLIGHT_PAD + LP_SPOTLIGHT_CELL_H + LP_SPOTLIGHT_PAD; }
static float results_height(int count) { return count > 0 ? 1 + 2 * LIST_PAD + count * LP_LIST_ROW_H : 1 + EMPTY_H; }

lp_size lp_spotlight_measure(const lp_spotlight_view *v) {
    int dock = lp_spotlight_query_is_blank(v->query ? v->query->text : "");
    float h = 2 * LP_SPOTLIGHT_PAD + LP_SIZE_SPOTLIGHT_BAR_HEIGHT + (dock ? dock_height() : results_height(v->count));
    return (lp_size){ width_of(v), h };
}

lp_size lp_spotlight_max_size(float width) {
    float tallest = dock_height() > results_height(LP_SPOTLIGHT_MAX_RESULTS) ? dock_height() : results_height(LP_SPOTLIGHT_MAX_RESULTS);
    return (lp_size){ width > 0 ? width : LP_SIZE_SPOTLIGHT_WIDTH, 2 * LP_SPOTLIGHT_PAD + LP_SIZE_SPOTLIGHT_BAR_HEIGHT + tallest };
}

static void tile(lp_ctx *ctx, lp_id id, lp_rect cell, const lp_spotlight_item *it, int selected, lp_spotlight_result *res, int index) {
    if (lp_clicked(ctx, id, cell)) res->activated = index;
    if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, cell)) res->hovered = index;
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    if (selected) {
        lp_fill_solid(cr, cell, accent.soft, LP_RADIUS_SM);
        static lp_shadow_layer ring[1] = { { 1, 0, 0, 0, 1, { 0, 0, 0, 1 } } };
        ring[0].color = accent.light;
        lp_draw_inset_shadows(cr, cell, LP_RADIUS_SM, ring, 1);
    } else if (lp_is_hot(ctx, id)) {
        lp_fill_solid(cr, cell, LP_RGBA(0, 0, 0, 0.04f), LP_RADIUS_SM);
    }
    float plate = LP_SIZE_SPOTLIGHT_TILE;
    lp_rect ic = LP_RECT(cell.x + (cell.w - plate) / 2, cell.y + 6, plate, plate);
    /* No raised plate: hover and selection belong to the cell around the icon,
     * which is what the plate was being mistaken for. */
    lp_icon_draw(cr, it->icon, ic.x + (plate - 36) / 2, ic.y + (plate - 36) / 2, 36, 1.6f, LP_INK_SECONDARY);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS;
    st.ellipsize = 1;
    lp_rect label = LP_RECT(cell.x + 2, ic.y + plate + LP_SPACE_1, cell.w - 4, 14);
    lp_text_draw(cr, it->title, label, &st, LP_ALIGN_CENTER);
    if (it->running) lp_fill_solid(cr, LP_RECT(cell.x + cell.w / 2 - 2, label.y + label.h + 2, 4, 4), accent.base, 2);
}

void lp_spotlight_panel(lp_ctx *ctx, float x, float y, const lp_spotlight_view *v, lp_spotlight_result *out) {
    lp_spotlight_result res = { .hovered = -1, .activated = -1 };
    lp_size size = lp_spotlight_measure(v);
    lp_rect panel = LP_RECT(x, y, size.w, size.h);
    res.panel = panel;
    int draw = ctx->pass == LP_PASS_DRAW && ctx->cr;
    cairo_t *cr = ctx->cr;
    lp_id base = LP_ID("spotlight");

    if (draw) {
        float shell_radius = lp_radius_flex(ctx, LP_RADIUS_LG);
        lp_draw_shadow_9slice(cr, panel, shell_radius, LP_SHADOW_MENU, LP_SHADOW_MENU_COUNT);
        lp_surface_paint(cr, panel, (lp_surface_opts){ .variant = LP_VARIANT_FLAT, .radius = shell_radius, .sheen = 1, .sheen_alpha = -1 },
            (lp_surface_motion){ .sheen_x = 0.5f });
    }

    /* The bar */
    lp_rect bar = LP_RECT(panel.x + LP_SPOTLIGHT_PAD, panel.y + LP_SPOTLIGHT_PAD, panel.w - 2 * LP_SPOTLIGHT_PAD, LP_SIZE_SPOTLIGHT_BAR_HEIGHT);
    static lp_text_buffer scratch;
    lp_text_buffer *q = v->query ? v->query : &scratch;
    res.query_changed = lp_text_field(ctx, LP_SPOTLIGHT_QUERY_ID, bar, q,
        (lp_text_field_opts){ .placeholder = v->placeholder ? v->placeholder : LP_SPOTLIGHT_PLACEHOLDER, .icon = LP_ICON_SEARCH, .round = 1, .large = 1 });
    /* A press on a tile or row clears the field's focus; the bar keeps it while the panel is up. */
    if (ctx->pass == LP_PASS_EVENT && v->focus_bar) ctx->focus = LP_SPOTLIGHT_QUERY_ID;

    float cy = bar.y + bar.h + LP_SPOTLIGHT_PAD;
    if (draw) lp_fill_solid(cr, LP_RECT(panel.x + LP_SPOTLIGHT_PAD, cy, panel.w - 2 * LP_SPOTLIGHT_PAD, 1), LP_EDGE_DIVIDER, 0);
    cy += 1;

    if (lp_spotlight_query_is_blank(q->text)) {
        /* The dock: one centred row of tiles */
        int n = v->count;
        float row_w = n * LP_SPOTLIGHT_CELL_W;
        float x0 = panel.x + (panel.w - row_w) / 2;
        for (int i = 0; i < n; i++) {
            lp_rect cell = LP_RECT(x0 + i * LP_SPOTLIGHT_CELL_W, cy + LP_SPOTLIGHT_PAD, LP_SPOTLIGHT_CELL_W, LP_SPOTLIGHT_CELL_H);
            tile(ctx, lp_id_index(base, 100 + i), cell, &v->items[i], v->selection == i, &res, i);
        }
    } else if (v->count > 0) {
        lp_rect list = LP_RECT(panel.x + LP_SPOTLIGHT_PAD, cy + LIST_PAD, panel.w - 2 * LP_SPOTLIGHT_PAD, v->count * LP_LIST_ROW_H);
        if (draw) { cairo_save(cr); lp_path_rrect(cr, list, LP_RADIUS_SM); cairo_clip(cr); lp_fill_solid(cr, list, LP_SURFACE_WELL, 0); }
        for (int i = 0; i < v->count; i++) {
            const lp_spotlight_item *it = &v->items[i];
            const char *cols[1] = { it->subtitle };
            lp_rect row = LP_RECT(list.x, list.y + i * LP_LIST_ROW_H, list.w, LP_LIST_ROW_H);
            if (lp_list_row(ctx, lp_id_index(base, 200 + i), row, it->icon, it->title, cols, 1, v->selection == i, (i % 2) == 1)) res.activated = i;
            if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, row)) res.hovered = i;
        }
        if (draw) { cairo_restore(cr); lp_draw_inset_shadows(cr, list, LP_RADIUS_SM, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT); }
    } else if (draw) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_SM;
        st.color = LP_INK_TERTIARY;
        st.ellipsize = 1;
        char msg[320];
        snprintf(msg, sizeof msg, "No results for “%s”.", q->text);
        lp_text_draw(cr, msg, LP_RECT(panel.x + LP_SPOTLIGHT_PAD, cy, panel.w - 2 * LP_SPOTLIGHT_PAD, EMPTY_H), &st, LP_ALIGN_CENTER);
    }
    if (out) *out = res;
}
