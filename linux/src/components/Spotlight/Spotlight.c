#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_menu_item.h"
#include "maryui/components/lp_monogram.h"
#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_object_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_shadow.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define EMPTY_H 40
#define LIST_PAD LP_SPACE_1
/* The monogram's box inside the first pill, where the others carry a label. */
#define PILL_MARK 14
#define PILLS_PAD_TOP 4

int lp_spotlight_query_is_blank(const char *query) {
    for (; query && *query; query++) if (!isspace((unsigned char)*query)) return 0;
    return 1;
}

/* The panel's padding above the bar and below everything else, plus the gap
 * between the bar and the divider — the web's `.panel` padding and `.divider`
 * margin-top. The bottom padding is what makes the nested corners concentric:
 * the open menu sits two paddings in, the results well one padding and a
 * LIST_PAD, exactly what radius.spotlight-menu / -results subtract. */
#define PANEL_V_PAD (3 * LP_SPOTLIGHT_PAD)
/* The commands section's inset from the panel edge: the panel's padding plus
 * the margin .cmdDivider, .cmdPills and .cmdDropdown each carry. */
#define CMD_INSET (2 * LP_SPOTLIGHT_PAD)

static float width_of(const lp_spotlight_view *v) { return v->width > 0 ? v->width : LP_SIZE_SPOTLIGHT_WIDTH; }
static float dock_height(void) { return 1 + LP_SPOTLIGHT_PAD + LP_SPOTLIGHT_CELL_H + LP_SPOTLIGHT_PAD; }
/* A dock cell's width: 88 while the row fits, narrower when it would not, so the row always sits inside the
 * panel at the search bar's own inset (seven pinned apps at 88 would be 616 in a 560 panel). */
static float dock_cell_w(float panel_w, int n) {
    float fit = n > 0 ? (panel_w - 2 * LP_SPOTLIGHT_PAD) / n : LP_SPOTLIGHT_CELL_W;
    return fit < LP_SPOTLIGHT_CELL_W ? fit : LP_SPOTLIGHT_CELL_W;
}
static float results_height(int count) { return count > 0 ? 1 + 2 * LIST_PAD + count * LP_LIST_ROW_H : 1 + EMPTY_H; }

static int pill_count(const lp_spotlight_view *v) {
    return v->menu_count > LP_SPOTLIGHT_MAX_PILLS ? LP_SPOTLIGHT_MAX_PILLS : v->menu_count;
}
static int has_commands(const lp_spotlight_view *v) { return v->menus && v->menu_count > 0; }
static int open_menu_of(const lp_spotlight_view *v) {
    return has_commands(v) && v->open_menu >= 0 && v->open_menu < v->menu_count ? v->open_menu : -1;
}

static lp_text_style pill_style(void) {
    lp_text_style s = lp_text_style_default();
    s.size_px = LP_TEXT_SM;
    s.weight = LP_TEXT_WEIGHT_MEDIUM;
    return s;
}

/*
 * Lays the pills out like the web's `flex-wrap: wrap`, left to right inside
 * `box`, and returns the row count. `rects` (optional) receives one rect per
 * pill. Measured with lp_text_measure, which keeps a scratch context, so the
 * EVENT pass lays them out exactly where the DRAW pass will.
 */
static int pill_layout(cairo_t *cr, const lp_spotlight_view *v, lp_rect box, lp_rect *rects) {
    lp_text_style ps = pill_style();
    float x = box.x, y = box.y;
    int rows = 1;
    for (int i = 0, n = pill_count(v); i < n; i++) {
        const char *label = v->menus[i].label ? v->menus[i].label : "";
        float inner = i == 0 ? PILL_MARK : lp_text_measure(cr, label, &ps).w;
        float w = inner + 2 * LP_SPACE_3;
        if (x > box.x && x + w > box.x + box.w) {
            x = box.x;
            y += LP_SPOTLIGHT_PILL_H + LP_SPOTLIGHT_PILL_GAP;
            rows++;
        }
        if (rects) rects[i] = LP_RECT(x, y, w, LP_SPOTLIGHT_PILL_H);
        x += w + LP_SPOTLIGHT_PILL_GAP;
    }
    return rows;
}

/* The dock's commands: the divider, the context line, the pills, and the open
 * menu's entries. `open` is -1 to measure the section with nothing expanded. */
static float commands_height(const lp_spotlight_view *v, float width, int open) {
    if (!has_commands(v)) return 0;
    lp_rect box = LP_RECT(0, 0, width - 2 * CMD_INSET, 0);
    float h = 1;                                     /* .cmdDivider */
    if (v->context_name) h += LP_SPOTLIGHT_CMD_HEADER_H;
    int rows = pill_layout(NULL, v, box, NULL);
    h += PILLS_PAD_TOP + rows * LP_SPOTLIGHT_PILL_H + (rows - 1) * LP_SPOTLIGHT_PILL_GAP + LP_SPOTLIGHT_PAD;
    if (open >= 0) h += lp_menu_list_measure(NULL, &v->menus[open]).h + LP_SPOTLIGHT_PAD;
    return h;
}

lp_size lp_spotlight_measure(const lp_spotlight_view *v) {
    int dock = lp_spotlight_query_is_blank(v->query ? v->query->text : "");
    float w = width_of(v);
    float h = PANEL_V_PAD + LP_SIZE_SPOTLIGHT_BAR_HEIGHT + (dock ? dock_height() : results_height(v->count));
    if (dock) h += commands_height(v, w, open_menu_of(v));
    if (v->max_h > 0 && h > v->max_h) h = v->max_h;
    return (lp_size){ w, h };
}

lp_size lp_spotlight_max_size(const lp_spotlight_view *v) {
    float w = width_of(v);
    float dock = dock_height() + commands_height(v, w, -1);
    float results = results_height(LP_SPOTLIGHT_MAX_RESULTS);
    return (lp_size){ w, PANEL_V_PAD + LP_SIZE_SPOTLIGHT_BAR_HEIGHT + (dock > results ? dock : results) };
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
    /* Laid out from the bottom, so the running dot keeps air inside the cell's highlight (it sat on the
     * edge): the dot, the label over it, and the icon centred in what is left above. The icon scales
     * with a narrowed cell. */
    float scale = cell.w / LP_SPOTLIGHT_CELL_W;
    lp_rect dot = LP_RECT(cell.x + cell.w / 2 - 2, cell.y + cell.h - 4 - 4, 4, 4);
    lp_rect label = LP_RECT(cell.x + 1, dot.y - 2 - 14, cell.w - 2, 14);
    float above = label.y - LP_SPACE_1 - cell.y;
    float plate = roundf(LP_SIZE_SPOTLIGHT_TILE * scale);
    lp_rect ic = LP_RECT(cell.x + (cell.w - plate) / 2, roundf(cell.y + (above - plate) / 2), plate, plate);
    /* No raised plate: hover and selection belong to the cell around the icon,
     * which is what the plate was being mistaken for. At 36 the mark is above
     * the glyph tier, so an app with an object of its name is drawn as one. */
    float side = roundf(36 * scale);
    if (side <= LP_OBJECT_TIER_GLYPH_MAX) side = LP_OBJECT_TIER_GLYPH_MAX + 1;   /* object art, never the glyph */
    lp_rect mark = LP_RECT(roundf(ic.x + (plate - side) / 2), roundf(ic.y + (plate - side) / 2), side, side);
    lp_object named = it->object ? lp_object_by_name(it->object) : LP_OBJ_COUNT;
    if (named < LP_OBJ_COUNT && lp_icon_tier(mark.w) != LP_TIER_GLYPH) lp_object_icon_draw(cr, named, mark, ctx->settings);
    else lp_icon_paint_object(cr, it->icon, mark, 1.6f, LP_INK_SECONDARY, ctx->settings);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS;
    st.ellipsize = 1;
    /* a name a narrowed cell cannot hold steps down a little before it would ellipsize, re-measured at each
     * step because hinted advances do not shrink in proportion to the size */
    while (st.size_px > 9 && lp_text_measure(cr, it->title, &st).w > label.w) st.size_px -= 0.5f;
    lp_text_draw(cr, it->title, label, &st, LP_ALIGN_CENTER);
    if (it->running) lp_fill_solid(cr, dot, accent.base, 2);
}

/*
 * The commands, under the dock: the hairline, the "Searching <app>" line, the
 * pills, and the open pill's entries inline. `y` is the section's top; the
 * panel's own bottom edge bounds the open menu, which is where view.max_h
 * takes effect.
 */
static void commands(lp_ctx *ctx, const lp_spotlight_view *v, lp_rect panel, float y, lp_id base, lp_spotlight_result *res) {
    int draw = ctx->pass == LP_PASS_DRAW && ctx->cr;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    float inner_x = panel.x + CMD_INSET, inner_w = panel.w - 2 * CMD_INSET;
    int open = open_menu_of(v);

    if (draw) lp_fill_solid(cr, LP_RECT(inner_x, y, inner_w, 1), LP_EDGE_DIVIDER, 0);
    y += 1;

    if (v->context_name) {
        if (draw) {
            lp_text_style ts = lp_text_style_default();
            ts.size_px = LP_TEXT_XS;
            ts.color = LP_INK_TERTIARY;
            lp_rect line = LP_RECT(panel.x + LP_SPOTLIGHT_PAD + LP_SPACE_3, y + LP_SPACE_2, inner_w, 14);
            lp_icon_draw(cr, v->context_icon, line.x, line.y + 1, 12, 1.6f, LP_INK_TERTIARY);
            float tx = line.x + 12 + LP_SPACE_1;
            float tw = lp_text_measure(cr, "Searching ", &ts).w;
            lp_text_draw(cr, "Searching ", LP_RECT(tx, line.y, tw + 2, line.h), &ts, LP_ALIGN_START);
            lp_text_style ns = ts;
            ns.color = LP_INK_SECONDARY;
            ns.weight = LP_TEXT_WEIGHT_MEDIUM;
            ns.ellipsize = 1;
            lp_text_draw(cr, v->context_name, LP_RECT(tx + tw, line.y, panel.x + panel.w - LP_SPOTLIGHT_PAD - LP_SPACE_3 - (tx + tw), line.h), &ns, LP_ALIGN_START);
        }
        y += LP_SPOTLIGHT_CMD_HEADER_H;
    }

    y += PILLS_PAD_TOP;
    lp_rect pills[LP_SPOTLIGHT_MAX_PILLS];
    int n = pill_count(v);
    int rows = pill_layout(cr, v, LP_RECT(inner_x, y, inner_w, 0), pills);
    for (int i = 0; i < n; i++) {
        lp_rect pr = pills[i];
        if (lp_hot(ctx, lp_id_index(base, 300 + i), pr)) {
            res->menu_hovered = i;
            /* Press, not release: a pull-down menu opens under the finger. */
            if (ctx->pass == LP_PASS_EVENT && (ctx->in.pressed & LP_BUTTON_LEFT)) res->menu_pressed = i;
        }
        if (!draw) continue;
        int is_open = i == open;
        if (is_open) lp_fill_vgradient(cr, pr, accent.light, accent.base, LP_RADIUS_PILL);
        else lp_fill_solid(cr, pr, LP_RGBA(0, 0, 0, 0.045f), LP_RADIUS_PILL);
        lp_color ink = is_open ? LP_INK_ON_ACCENT : LP_INK_PRIMARY;
        if (i == 0) {
            lp_monogram_paint(cr, LP_RECT(pr.x + LP_SPACE_3, pr.y + (pr.h - PILL_MARK) / 2, PILL_MARK, PILL_MARK), LP_MONOGRAM_FLAT, ink);
        } else if (v->menus[i].label) {
            lp_text_style ps = pill_style();
            ps.color = ink;
            lp_text_draw(cr, v->menus[i].label, pr, &ps, LP_ALIGN_CENTER);
        }
    }
    y += rows * LP_SPOTLIGHT_PILL_H + (rows - 1) * LP_SPOTLIGHT_PILL_GAP + LP_SPOTLIGHT_PAD;

    if (open < 0) return;
    /* The entries, inline. Unlike the old floating menu this carries shadow.menu
     * but no emboss: the panel around it is the surface (the web's .cmdDropdown). */
    float avail = panel.y + panel.h - 2 * LP_SPOTLIGHT_PAD - y;   /* its margin, then the panel's padding */
    lp_size ds = lp_menu_list_measure(cr, &v->menus[open]);
    float dh = ds.h < avail ? ds.h : avail;
    if (dh <= 2 * LP_SPACE_1) return;
    lp_rect drop = LP_RECT(inner_x, y, inner_w, dh);
    /* Concentric with the panel's corners: radius.spotlight less CMD_INSET. */
    float radius = lp_radius_flex(ctx, LP_RADIUS_SPOTLIGHT_MENU);
    if (draw) {
        lp_draw_shadow_9slice(cr, drop, radius, LP_SHADOW_MENU, LP_SHADOW_MENU_COUNT);
        lp_fill_solid(cr, drop, LP_SURFACE_MENU, radius);
        cairo_save(cr);
        lp_path_rrect(cr, drop, radius);
        cairo_clip(cr);
    }
    lp_menu_list_result mr;
    lp_menu_list(ctx, LP_RECT(drop.x + LP_SPACE_1, drop.y + LP_SPACE_1, drop.w - 2 * LP_SPACE_1, drop.h - 2 * LP_SPACE_1),
        &v->menus[open], v->menu_active, &mr);
    res->entry_hovered = mr.hovered;
    res->entry_selected = mr.selected;
    if (draw) cairo_restore(cr);
}

void lp_spotlight_panel(lp_ctx *ctx, float x, float y, const lp_spotlight_view *v, lp_spotlight_result *out) {
    lp_spotlight_result res = { .hovered = -1, .activated = -1, .menu_pressed = -1, .menu_hovered = -1,
                                .entry_hovered = -1, .entry_selected = -1 };
    lp_size size = lp_spotlight_measure(v);
    lp_rect panel = LP_RECT(x, y, size.w, size.h);
    res.panel = panel;
    int draw = ctx->pass == LP_PASS_DRAW && ctx->cr;
    cairo_t *cr = ctx->cr;
    lp_id base = LP_ID("spotlight");

    if (draw) {
        float shell_radius = lp_radius_flex(ctx, LP_RADIUS_SPOTLIGHT);
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
        float cell_w = dock_cell_w(panel.w, n);
        float x0 = panel.x + (panel.w - n * cell_w) / 2;
        for (int i = 0; i < n; i++) {
            lp_rect cell = LP_RECT(x0 + i * cell_w, cy + LP_SPOTLIGHT_PAD, cell_w, LP_SPOTLIGHT_CELL_H);
            tile(ctx, lp_id_index(base, 100 + i), cell, &v->items[i], v->selection == i, &res, i);
        }
        /* The commands the menu bar used to carry, folded in under the dock. */
        if (has_commands(v)) commands(ctx, v, panel, cy + LP_SPOTLIGHT_PAD + LP_SPOTLIGHT_CELL_H + LP_SPOTLIGHT_PAD, base, &res);
    } else if (v->count > 0) {
        lp_rect list = LP_RECT(panel.x + LP_SPOTLIGHT_PAD, cy + LIST_PAD, panel.w - 2 * LP_SPOTLIGHT_PAD, v->count * LP_LIST_ROW_H);
        /* Concentric with the panel's bottom corners (radius.spotlight-results). */
        float list_radius = lp_radius_flex(ctx, LP_RADIUS_SPOTLIGHT_RESULTS);
        if (draw) { cairo_save(cr); lp_path_rrect(cr, list, list_radius); cairo_clip(cr); lp_fill_solid(cr, list, LP_SURFACE_WELL, 0); }
        for (int i = 0; i < v->count; i++) {
            const lp_spotlight_item *it = &v->items[i];
            const char *cols[1] = { it->subtitle };
            lp_rect row = LP_RECT(list.x, list.y + i * LP_LIST_ROW_H, list.w, LP_LIST_ROW_H);
            if (lp_list_row(ctx, lp_id_index(base, 200 + i), row, it->icon, it->title, cols, 1, v->selection == i, (i % 2) == 1)) res.activated = i;
            if (ctx->pass == LP_PASS_EVENT && lp_hit(ctx, row)) res.hovered = i;
        }
        if (draw) { cairo_restore(cr); lp_draw_inset_shadows(cr, list, list_radius, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT); }
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
