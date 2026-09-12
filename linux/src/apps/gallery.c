/* GalleryApp — the design system showing itself: every control in every
 * state, the surfaces, the bubbles, the tokens, and a Motion tab whose
 * sliders retune the physics live. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_goo_group.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_motion.h"
#include "maryui/lp_text.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"

struct gallery {
    int tab;
    lp_scroll_state scroll[5];
    /* controls tab state */
    int segment, on, off, checked, row;
    float slider;
    lp_text_buffer text;
    int copied;
    double copied_at;
};

static void *gallery_create(lp_desktop *d, const char *window_id) {
    struct gallery *g = calloc(1, sizeof *g);
    g->segment = 1; g->on = 1; g->checked = 1; g->slider = 42; g->row = 0;
    return g;
}
static void gallery_destroy(void *state) { free(state); }

static lp_text_style heading_style(void) { lp_text_style s = lp_text_style_default(); s.size_px = LP_TEXT_LG; s.weight = LP_TEXT_WEIGHT_SEMIBOLD; return s; }
static lp_text_style note_style(void) { lp_text_style s = lp_text_style_default(); s.size_px = LP_TEXT_SM; s.color = LP_INK_TERTIARY; return s; }
static lp_text_style mono_style(void) { lp_text_style s = lp_text_style_default(); s.font = LP_FONT_MONO; s.size_px = LP_TEXT_XS; s.color = LP_INK_TERTIARY; return s; }

static float heading(lp_ctx *ctx, lp_rect *c, const char *text) {
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) { lp_text_style s = heading_style(); lp_text_draw(ctx->cr, text, LP_RECT(c->x, c->y, c->w, 22), &s, LP_ALIGN_START); }
    c->y += 22 + LP_SPACE_3;
    return 22 + LP_SPACE_3;
}
static void note(lp_ctx *ctx, lp_rect *c, const char *text) {
    c->y -= LP_SPACE_2;
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) { lp_text_style s = note_style(); lp_text_draw(ctx->cr, text, LP_RECT(c->x, c->y, c->w, 18), &s, LP_ALIGN_START); }
    c->y += 18 + LP_SPACE_3;
}
static void section_end(lp_rect *c) { c->y += LP_SPACE_6; }

static void percent(float v, char *out, size_t n) { snprintf(out, n, "%d%%", (int)roundf(v)); }

/* MARK: - Controls */
static void controls_tab(struct gallery *g, lp_ctx *ctx, lp_rect *c) {
    lp_id base = LP_ID("gallery.controls");
    heading(ctx, c, "Buttons");
    float x = c->x, y = c->y;
    struct { const char *label; lp_button_opts o; } buttons[] = {
        { "Default", { LP_BUTTON_DEFAULT, LP_CONTROL_MD, LP_ICON_COUNT, 0, 0 } }, { "Primary", { LP_BUTTON_PRIMARY, LP_CONTROL_MD, LP_ICON_COUNT, 0, 0 } },
        { "Quiet", { LP_BUTTON_QUIET, LP_CONTROL_MD, LP_ICON_COUNT, 0, 0 } }, { "With icon", { LP_BUTTON_DEFAULT, LP_CONTROL_MD, LP_ICON_STAR, 0, 0 } },
        { "Settings", { LP_BUTTON_DEFAULT, LP_CONTROL_MD, LP_ICON_GEAR, 1, 0 } }, { "Disabled", { LP_BUTTON_DEFAULT, LP_CONTROL_MD, LP_ICON_COUNT, 0, 1 } },
    };
    for (int i = 0; i < 6; i++) {
        lp_size s = lp_button_measure(ctx, buttons[i].label, buttons[i].o);
        lp_button(ctx, lp_id_index(base, i), LP_RECT(x, y, s.w, s.h), buttons[i].label, buttons[i].o);
        x += s.w + LP_SPACE_3;
    }
    y += LP_SIZE_CONTROL_HEIGHT + LP_SPACE_3;
    x = c->x;
    struct { const char *label; lp_button_opts o; } small[] = {
        { "Small", { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 } }, { "Small primary", { LP_BUTTON_PRIMARY, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 } },
        { "Add", { LP_BUTTON_QUIET, LP_CONTROL_SM, LP_ICON_PLUS, 0, 0 } },
    };
    for (int i = 0; i < 3; i++) {
        lp_size s = lp_button_measure(ctx, small[i].label, small[i].o);
        lp_button(ctx, lp_id_index(base, 10 + i), LP_RECT(x, y + (LP_SIZE_CONTROL_HEIGHT - s.h) / 2, s.w, s.h), small[i].label, small[i].o);
        x += s.w + LP_SPACE_3;
    }
    c->y = y + LP_SIZE_CONTROL_HEIGHT + LP_SPACE_3;
    section_end(c);

    heading(ctx, c, "Segmented control");
    note(ctx, c, "Hover the segment beside the selection and watch the metal reach for it.");
    static const lp_segment ranges[3] = { { "Day", LP_ICON_COUNT }, { "Week", LP_ICON_COUNT }, { "Month", LP_ICON_COUNT } };
    static const lp_segment icons[3] = { { NULL, LP_ICON_GRID }, { NULL, LP_ICON_LIST }, { NULL, LP_ICON_IMAGE } };
    lp_size s1 = lp_segmented_measure(ctx, ranges, 3, LP_CONTROL_MD);
    lp_segmented(ctx, lp_id_index(base, 20), c->x, c->y, ranges, 3, &g->segment, LP_CONTROL_MD);
    lp_segmented(ctx, lp_id_index(base, 21), c->x + s1.w + LP_SPACE_3, c->y + 2, icons, 3, &g->segment, LP_CONTROL_SM);
    c->y += s1.h + LP_SPACE_3;
    section_end(c);

    heading(ctx, c, "Toggles and checkboxes");
    x = c->x;
    lp_toggle(ctx, lp_id_index(base, 30), x, c->y, &g->on, 0); x += LP_TOGGLE_W + LP_SPACE_3;
    lp_toggle(ctx, lp_id_index(base, 31), x, c->y, &g->off, 0); x += LP_TOGGLE_W + LP_SPACE_3;
    int always = 1;
    lp_toggle(ctx, lp_id_index(base, 32), x, c->y, &always, 1); x += LP_TOGGLE_W + LP_SPACE_3;
    lp_checkbox(ctx, lp_id_index(base, 33), x, c->y + 2, &g->checked, "Remember through Thread", 0);
    x += lp_checkbox_measure(ctx, "Remember through Thread").w + LP_SPACE_3;
    int never = 0;
    lp_checkbox(ctx, lp_id_index(base, 34), x, c->y + 2, &never, "Disabled", 1);
    c->y += LP_TOGGLE_H + LP_SPACE_3;
    section_end(c);

    heading(ctx, c, "Fields and sliders");
    float sw = fminf(460, c->w);
    lp_text_field(ctx, lp_id_index(base, 40), LP_RECT(c->x, c->y, sw, LP_SIZE_CONTROL_HEIGHT), &g->text, (lp_text_field_opts){ .placeholder = "Search the design system", .icon = LP_ICON_SEARCH });
    c->y += LP_SIZE_CONTROL_HEIGHT + LP_SPACE_3;
    static lp_text_buffer capsule, disabled_field;
    lp_text_field(ctx, lp_id_index(base, 41), LP_RECT(c->x, c->y, sw, LP_SIZE_CONTROL_HEIGHT), &capsule, (lp_text_field_opts){ .placeholder = "Capsule field", .icon = LP_ICON_COUNT, .round = 1 });
    c->y += LP_SIZE_CONTROL_HEIGHT + LP_SPACE_3;
    lp_text_field(ctx, lp_id_index(base, 42), LP_RECT(c->x, c->y, sw, LP_SIZE_CONTROL_HEIGHT), &disabled_field, (lp_text_field_opts){ .placeholder = "Disabled", .icon = LP_ICON_COUNT, .disabled = 1 });
    c->y += LP_SIZE_CONTROL_HEIGHT + LP_SPACE_3;
    lp_slider(ctx, lp_id_index(base, 43), LP_RECT(c->x, c->y, sw, LP_SLIDER_H), &g->slider, (lp_slider_opts){ .max = 100, .step = 1, .label = "Brush opacity", .show_value = 1, .format = percent });
    c->y += LP_SLIDER_H + LP_SPACE_3;
    float thirty = 30;
    lp_slider(ctx, lp_id_index(base, 44), LP_RECT(c->x, c->y, sw, LP_SLIDER_H), &thirty, (lp_slider_opts){ .max = 100, .step = 1, .label = "Disabled", .disabled = 1 });
    c->y += LP_SLIDER_H + LP_SPACE_3;
    section_end(c);

    heading(ctx, c, "Progress");
    lp_progress(ctx, LP_RECT(c->x, c->y, sw, LP_PROGRESS_H), g->slider / 100);
    c->y += LP_PROGRESS_H + LP_SPACE_3;
    lp_progress(ctx, LP_RECT(c->x, c->y, sw, LP_PROGRESS_H), -1);
    c->y += LP_PROGRESS_H + LP_SPACE_3;
    section_end(c);

    heading(ctx, c, "List rows");
    static const char *const HEADER[] = { "Name", "Modified", "Size" };
    static const struct { const char *name, *modified, *size; lp_icon icon; } rows[] = {
        { "Liquid Platinum.sketch", "Today, 4:12 PM", "18.4 MB", LP_ICON_DOCUMENT }, { "monogram.svg", "Today, 3:48 PM", "6 KB", LP_ICON_IMAGE },
        { "tokens.json", "Today, 5:02 PM", "9 KB", LP_ICON_CODE }, { "Notes", "Sep 5, 2026", "--", LP_ICON_FOLDER },
    };
    lp_rect list = LP_RECT(c->x, c->y, c->w, LP_LIST_HEADER_H + 4 * LP_LIST_ROW_H);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) { cairo_save(ctx->cr); lp_path_rrect(ctx->cr, list, LP_RADIUS_SM); cairo_clip(ctx->cr); lp_fill_solid(ctx->cr, list, LP_SURFACE_WELL, 0); }
    lp_list_header(ctx, lp_id_index(base, 49), LP_RECT(list.x, list.y, list.w, LP_LIST_HEADER_H), HEADER, 3, -1, 0);
    for (int i = 0; i < 4; i++) {
        const char *cols[2] = { rows[i].modified, rows[i].size };
        if (lp_list_row(ctx, lp_id_index(base, 50 + i), LP_RECT(list.x, list.y + LP_LIST_HEADER_H + i * LP_LIST_ROW_H, list.w, LP_LIST_ROW_H), rows[i].icon, rows[i].name, cols, 2, g->row == i, (i % 2) == 1)) { g->row = i; ctx->dirty = 1; }
    }
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) { cairo_restore(ctx->cr); lp_draw_inset_shadows(ctx->cr, list, LP_RADIUS_SM, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT); }
    c->y += list.h;
    section_end(c);

    heading(ctx, c, "Text area");
    note(ctx, c, "Click to place the caret, drag to select, ↑/↓ by visual line, ⌘A/C/X/V.");
    static lp_text_area_state area;
    if (!area.doc.text) lp_text_doc_set(&area.doc, "Brushed platinum that moves like liquid.\nEvery surface is a token, every window a physics target.");
    lp_text_area(ctx, lp_id_index(base, 60), LP_RECT(c->x, c->y, sw, 96), &area, (lp_text_area_opts){ .placeholder = "Type something…" });
    c->y += 96;
    section_end(c);

    heading(ctx, c, "Spotlight");
    note(ctx, c, "Ctrl+Space opens it on the desktop: the search bar is the dock, and — there is no menu bar any more — "
                 "a blank query also shows the frontmost app's commands as pills; click one to open it. Typing filters apps and windows.");
    static const lp_spotlight_item dock[5] = {
        { .kind = LP_SPOT_APP, .id = "finder", .title = "Finder", .subtitle = "Application", .icon = LP_ICON_FOLDER, .object = "appFinder", .index = 0, .running = 1 },
        { .kind = LP_SPOT_APP, .id = "gallery", .title = "Gallery", .subtitle = "Application", .icon = LP_ICON_DROP, .index = 1, .running = 1 },
        { .kind = LP_SPOT_APP, .id = "about", .title = "About", .subtitle = "Application", .icon = LP_ICON_INFO, .index = 2 },
        { .kind = LP_SPOT_APP, .id = "textedit", .title = "TextEdit", .subtitle = "Application", .icon = LP_ICON_PENCIL, .index = 3 },
        { .kind = LP_SPOT_COMMAND, .id = "terminal", .title = "Terminal", .subtitle = "Command", .icon = LP_ICON_TERMINAL },
    };
    /* A static stand-in for lp_desktop_build_menus, so the preview does not
     * reach into the real desktop's state (ControlsTab.tsx does the same). */
    static const lp_menu_model menus[3] = {
        { "rao", "Rao", { { 0, "About Liquid Platinum", NULL, 0, 0, 0, 0 } }, 1 },
        { "file", "File", { { 0, "New Finder Window", "⌘N", 0, 0, 0, 0 }, { 0, "Open…", "⌘O", 0, 1, 0, 0 },
                            { 1, "", NULL, 0, 0, 0, 0 }, { 0, "Close Window", "⌘W", 0, 0, 0, 0 } }, 4 },
        { "view", "View", { { 0, "Liquid Merge", NULL, 1, 0, 0, 0 } }, 1 },
    };
    static int preview_menu = -1;
    static lp_text_buffer query;
    lp_spotlight_view view = { .query = &query, .items = dock, .count = 5, .selection = 3, .width = fminf(LP_SIZE_SPOTLIGHT_WIDTH, c->w),
        .menus = menus, .menu_count = 3, .open_menu = preview_menu, .menu_active = -1,
        .context_name = "Gallery", .context_icon = LP_ICON_DROP };
    lp_size ps = lp_spotlight_measure(&view);
    lp_spotlight_result pres;
    lp_spotlight_panel(ctx, c->x, c->y, &view, &pres);
    if (ctx->pass == LP_PASS_EVENT && pres.menu_pressed >= 0) {
        preview_menu = preview_menu == pres.menu_pressed ? -1 : pres.menu_pressed;
        ctx->dirty = 1;
    }
    c->y += ps.h;
    section_end(c);
}

/* MARK: - Surfaces */
static void tile_label(lp_ctx *ctx, lp_rect r, const char *text) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style s = lp_text_style_default();
    s.size_px = LP_TEXT_SM; s.weight = LP_TEXT_WEIGHT_MEDIUM; s.color = LP_INK_SECONDARY; s.emboss = 1;
    lp_text_draw(ctx->cr, text, r, &s, LP_ALIGN_CENTER);
}

static void surfaces_tab(struct gallery *g, lp_ctx *ctx, lp_rect *c) {
    heading(ctx, c, "Surface variants");
    note(ctx, c, "Base gradient, brushed grain, sheen. Only the two gradient stops change between variants.");
    static const struct { enum lp_surface_variant v; const char *name; int sheen; } variants[] = {
        { LP_VARIANT_RAISED, "raised", 1 }, { LP_VARIANT_FLAT, "flat", 1 }, { LP_VARIANT_TITLEBAR, "titlebar", 1 },
        { LP_VARIANT_BAR, "bar", 1 }, { LP_VARIANT_WELL, "well", 0 }, { LP_VARIANT_BODY, "body", 0 },
    };
    int cols = (int)fmaxf(1, floorf((c->w + LP_SPACE_3) / (180 + LP_SPACE_3)));
    float tw = (c->w - LP_SPACE_3 * (cols - 1)) / cols;
    for (int i = 0; i < 6; i++) {
        lp_rect t = LP_RECT(c->x + (i % cols) * (tw + LP_SPACE_3), c->y + (i / cols) * (96 + LP_SPACE_3), tw, 96);
        lp_surface(ctx, t, (lp_surface_opts){ .variant = variants[i].v, .radius = LP_RADIUS_MD, .sheen = variants[i].sheen, .sheen_alpha = -1 });
        tile_label(ctx, t, variants[i].name);
    }
    c->y += ((6 + cols - 1) / cols) * (96 + LP_SPACE_3);
    section_end(c);

    heading(ctx, c, "Metal beds");
    note(ctx, c, "Adjacent controls share one metal bed; hover a blob and its neighbours lean toward it.");
    lp_goo_spec circles = { .size = LP_GOO_SM, .count = 4, .blob_size = 30, .shape = LP_GOO_CIRCLE, .gap = 6, .hot = -1 };
    lp_rect row = LP_RECT(c->x, c->y, 4 * 30 + 3 * 6, 30);
    static const lp_icon names[4] = { LP_ICON_HOME, LP_ICON_FOLDER, LP_ICON_STAR, LP_ICON_GEAR };
    for (int i = 0; i < 4; i++) if (lp_hot(ctx, lp_id_index(LP_ID("gallery.goo"), i), lp_goo_blob_rect(row, &circles, i))) circles.hot = i;
    lp_goo_group(ctx, row, &circles);
    for (int i = 0; i < 4; i++) { lp_rect b = lp_goo_blob_rect(row, &circles, i); lp_icon_widget(ctx, names[i], b.x + 7, b.y + 7, 16, LP_INK_SECONDARY); }
    lp_goo_spec pills = { .size = LP_GOO_SM, .count = 3, .blob_size = 22, .shape = LP_GOO_FILL, .gap = 4, .hot = -1 };
    lp_rect prow = LP_RECT(row.x + row.w + LP_SPACE_3, c->y + 4, 260, 22);
    static const char *const labels[3] = { "Cut", "Copy", "Paste" };
    for (int i = 0; i < 3; i++) if (lp_hot(ctx, lp_id_index(LP_ID("gallery.goo2"), i), lp_goo_blob_rect(prow, &pills, i))) pills.hot = i;
    lp_goo_group(ctx, prow, &pills);
    for (int i = 0; i < 3; i++) {
        lp_rect b = lp_goo_blob_rect(prow, &pills, i);
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) { lp_text_style s = lp_text_style_default(); s.size_px = LP_TEXT_SM; s.emboss = 1; lp_text_draw(ctx->cr, labels[i], b, &s, LP_ALIGN_CENTER); }
    }
    c->y += 30 + LP_SPACE_3;
    section_end(c);

    heading(ctx, c, "Emboss");
    lp_rect t1 = LP_RECT(c->x, c->y, 180, 96), t2 = LP_RECT(c->x + 192, c->y, 180, 96), t3 = LP_RECT(c->x + 384, c->y, 180, 96);
    lp_surface(ctx, t1, (lp_surface_opts){ .variant = LP_VARIANT_RAISED, .radius = LP_RADIUS_MD }); tile_label(ctx, t1, "raised");
    lp_surface(ctx, t2, (lp_surface_opts){ .variant = LP_VARIANT_WELL, .radius = LP_RADIUS_MD }); tile_label(ctx, t2, "well");
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_fill_vgradient(ctx->cr, t3, LP_SURFACE_RAISED_TOP, LP_SURFACE_RAISED_BOTTOM, LP_RADIUS_MD);
        lp_draw_brush(ctx->cr, t3, LP_RADIUS_MD, LP_BRUSH_OPACITY, ctx->world_x, ctx->world_y);
        lp_draw_inset_shadows(ctx->cr, t3, LP_RADIUS_MD, LP_SHADOW_EMBOSS_PRESSED, LP_SHADOW_EMBOSS_PRESSED_COUNT);
    }
    tile_label(ctx, t3, "pressed");
    lp_button(ctx, LP_ID("gallery.accent"), LP_RECT(c->x + 576, c->y + 37, 70, LP_SIZE_CONTROL_HEIGHT), "accent", (lp_button_opts){ .variant = LP_BUTTON_PRIMARY, .icon = LP_ICON_COUNT });
    c->y += 96;
    section_end(c);
}

/* MARK: - Bubbles */
static void mono_label(lp_ctx *ctx, lp_rect r, const char *text) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style s = mono_style();
    lp_text_draw(ctx->cr, text, r, &s, LP_ALIGN_CENTER);
}

static void bubbles_tab(struct gallery *g, lp_ctx *ctx, lp_rect *c) {
    heading(ctx, c, "Liquid bubbles");
    note(ctx, c, "Two rounded layers roll at different periods; the window's motion banks them. Grab this window's title bar and flick it.");
    static const float sizes[5] = { 12, 20, 32, 56, 88 };
    lp_rect row = LP_RECT(c->x, c->y, c->w, 88 + 2 * LP_SPACE_4);
    lp_surface(ctx, row, (lp_surface_opts){ .variant = LP_VARIANT_FLAT, .radius = LP_RADIUS_MD });
    float x = row.x + LP_SPACE_4;
    for (int i = 0; i < 5; i++) {
        lp_liquid_bubble(ctx, x + sizes[i] / 2, row.y + row.h - LP_SPACE_4 - sizes[i] / 2, sizes[i], LP_TINT_ACCENT, i * 1.3f, -1, NULL, 0);
        x += sizes[i] + LP_SPACE_5;
    }
    lp_want_frame_rect(ctx, row);
    c->y += row.h;
    section_end(c);

    heading(ctx, c, "Tints");
    static const struct { enum lp_bubble_tint t; const char *name; } tints[] = {
        { LP_TINT_CLOSE, "close" }, { LP_TINT_MINIMIZE, "minimize" }, { LP_TINT_ZOOM, "zoom" }, { LP_TINT_ACCENT, "accent" }, { LP_TINT_PLATINUM, "platinum" }, { LP_TINT_INACTIVE, "inactive" },
    };
    x = c->x;
    for (int i = 0; i < 6; i++) {
        lp_liquid_bubble(ctx, x + 30, c->y + 20, 40, tints[i].t, i * 0.9f, -1, NULL, 0);
        mono_label(ctx, LP_RECT(x, c->y + 46, 60, 14), tints[i].name);
        x += 60 + LP_SPACE_3;
    }
    c->y += 60 + LP_SPACE_3;
    section_end(c);

    heading(ctx, c, "Fill levels");
    static const float fills[5] = { 0.2f, 0.4f, 0.6f, 0.8f, 1 };
    x = c->x;
    for (int i = 0; i < 5; i++) {
        char t[24];
        snprintf(t, sizeof t, "%g", fills[i]);
        lp_liquid_bubble(ctx, x + 30, c->y + 20, 40, LP_TINT_ZOOM, i * 0.7f, fills[i], NULL, 0);
        mono_label(ctx, LP_RECT(x, c->y + 46, 60, 14), t);
        x += 60 + LP_SPACE_3;
    }
    c->y += 60 + LP_SPACE_3;
    section_end(c);
}

/* MARK: - Tokens */
static void swatches(lp_ctx *ctx, lp_rect *c, const char *title, const char *prefix) {
    heading(ctx, c, title);
    int cols = (int)fmaxf(1, floorf((c->w + LP_SPACE_2) / (88 + LP_SPACE_2)));
    float sw = (c->w - LP_SPACE_2 * (cols - 1)) / cols;
    int n = 0;
    size_t plen = strlen(prefix);
    for (int i = 0; i < LP_TOKEN_COUNT; i++) {
        const lp_token_info *t = &LP_TOKENS[i];
        if (strncmp(t->name, prefix, plen) != 0 || strcmp(t->type, "color") != 0) continue;
        lp_rect chip = LP_RECT(c->x + (n % cols) * (sw + LP_SPACE_2), c->y + (n / cols) * (44 + 18 + LP_SPACE_2), sw, 44);
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            unsigned r = 0, g = 0, b = 0; float a = 1;
            if (t->value[0] == '#' && strlen(t->value) == 7) { sscanf(t->value + 1, "%02x%02x%02x", &r, &g, &b); }
            else { float fr, fg, fb; if (sscanf(t->value, "rgba(%f, %f, %f, %f)", &fr, &fg, &fb, &a) == 4) { r = (unsigned)fr; g = (unsigned)fg; b = (unsigned)fb; } }
            lp_fill_solid(ctx->cr, chip, LP_RGBA(r / 255.0f, g / 255.0f, b / 255.0f, a), LP_RADIUS_SM);
            static const lp_shadow_layer ring[] = { { 1, 0, 0, 0, 1, { 0, 0, 0, 0.12f } } };
            lp_draw_inset_shadows(ctx->cr, chip, LP_RADIUS_SM, ring, 1);
            lp_draw_inset_shadows(ctx->cr, chip, LP_RADIUS_SM, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
            lp_text_style s = mono_style();
            s.color = LP_INK_SECONDARY; s.ellipsize = 1;
            char name[64];
            snprintf(name, sizeof name, "%s", t->name);
            for (char *p = name; *p; p++) if (*p == '.') *p = '-';
            lp_text_draw(ctx->cr, name, LP_RECT(chip.x, chip.y + 44 + 2, chip.w, 14), &s, LP_ALIGN_START);
        }
        n++;
    }
    c->y += ((n + cols - 1) / cols) * (44 + 18 + LP_SPACE_2);
    section_end(c);
}

static void tokens_tab(struct gallery *g, lp_ctx *ctx, lp_rect *c) {
    swatches(ctx, c, "Platinum ramp", "platinum.");
    swatches(ctx, c, "Surfaces", "surface.");
    swatches(ctx, c, "Ink", "ink.");
    swatches(ctx, c, "Accent · Blue", "accent.blue.");
    swatches(ctx, c, "Accent · Graphite", "accent.graphite.");
    swatches(ctx, c, "Traffic", "traffic.");
    heading(ctx, c, "Shape and rhythm");
    static const struct { const char *name; float r; } radii[] = { { "xs", LP_RADIUS_XS }, { "sm", LP_RADIUS_SM }, { "md", LP_RADIUS_MD }, { "lg", LP_RADIUS_LG }, { "window", LP_RADIUS_WINDOW }, { "pill", LP_RADIUS_PILL } };
    float x = c->x;
    for (int i = 0; i < 6; i++) {
        lp_rect box = LP_RECT(x + 16, c->y, 48, 48);
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            static const lp_shadow_layer hair[] = { { 0, 0, 0, 0, 1, { 0, 0, 0, 0.32f } } };
            lp_draw_outer_shadows(ctx->cr, box, radii[i].r, hair, 1);
            lp_fill_vgradient(ctx->cr, box, LP_PLATINUM_0, LP_PLATINUM_3, radii[i].r);
            lp_draw_inset_shadows(ctx->cr, box, radii[i].r, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
        }
        char label[40];
        snprintf(label, sizeof label, "radius-%s %gpx", radii[i].name, radii[i].r);
        mono_label(ctx, LP_RECT(x - 10, c->y + 54, 100, 14), label);
        x += 80 + LP_SPACE_3;
    }
    c->y += 72 + LP_SPACE_3;
    static const float spaces[8] = { LP_SPACE_1, LP_SPACE_2, LP_SPACE_3, LP_SPACE_4, LP_SPACE_5, LP_SPACE_6, LP_SPACE_7, LP_SPACE_8 };
    x = c->x;
    lp_accent accent = lp_settings_accent(ctx->settings);
    for (int i = 0; i < 8; i++) {
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_fill_solid(ctx->cr, LP_RECT(x + (60 - spaces[i]) / 2, c->y, spaces[i], 24), accent.base, 2);
        char label[24];
        snprintf(label, sizeof label, "%d · %gpx", i + 1, spaces[i]);
        mono_label(ctx, LP_RECT(x, c->y + 30, 60, 14), label);
        x += 60 + LP_SPACE_3;
    }
    c->y += 48;
    section_end(c);
}

/* MARK: - Motion */
struct knob { const char *label; float min, max, step; float *value; float scale; const char *path; };

static void fmt2(float v, char *out, size_t n) { snprintf(out, n, "%.2f", v); }
static void fmt3(float v, char *out, size_t n) { snprintf(out, n, "%.3f", v); }
static void fmt0(float v, char *out, size_t n) { snprintf(out, n, "%d", (int)roundf(v)); }

static void motion_tab(struct gallery *g, lp_ctx *ctx, lp_rect *c) {
    lp_motion_params *p = &lp_motion_live;
    lp_brush_params *b = &lp_brush_live;
    static float slosh_gain_scaled;
    slosh_gain_scaled = p->slosh.gain * 1e5f;
    static float brush_opacity, brush_fx, brush_fy, brush_oct, brush_contrast, brush_rise;
    brush_opacity = (float)b->opacity; brush_fx = (float)b->freq_x; brush_fy = (float)b->freq_y; brush_oct = (float)b->octaves; brush_contrast = (float)b->contrast; brush_rise = (float)b->rise;
    struct knob knobs[] = {
        { "Sheen frequency (Hz)", 0.3f, 4, 0.1f, &p->sheen.frequency, 1, "motion.spring-sheen.frequency" },
        { "Sheen damping", 0.05f, 1.2f, 0.05f, &p->sheen.damping, 1, "motion.spring-sheen.damping" },
        { "Light x (viewport)", 0, 1, 0.05f, &p->light_x, 1, "sheen.light-x" },
        { "Jelly frequency (Hz)", 0.5f, 5, 0.1f, &p->jelly.frequency, 1, "motion.spring-jelly.frequency" },
        { "Jelly damping", 0.05f, 1.2f, 0.05f, &p->jelly.damping, 1, "motion.spring-jelly.damping" },
        { "Jelly max scale", 0, 0.12f, 0.005f, &p->jelly_max_scale, 1, "motion.jelly-max-scale" },
        { "Jelly max skew (°)", 0, 8, 0.25f, &p->jelly_max_skew, 1, "motion.jelly-max-skew" },
        { "Tilt max (°)", 0, 10, 0.5f, &p->tilt_max, 1, "motion.tilt-max" },
        { "Velocity reference (px/s)", 500, 6000, 100, &p->velocity_ref, 1, "motion.velocity-ref" },
        { "Slosh frequency (Hz)", 0.4f, 4, 0.1f, &p->slosh.frequency_hz, 1, "motion.slosh-frequency" },
        { "Slosh damping", 0.02f, 1, 0.02f, &p->slosh.damping_ratio, 1, "motion.slosh-damping" },
        { "Slosh gain (×1e-5)", 0, 20, 0.5f, &slosh_gain_scaled, 1e-5f, "motion.slosh-gain" },
        { "Slosh max angle (rad)", 0.1f, 1.2f, 0.05f, &p->slosh.max_angle, 1, "motion.slosh-max" },
    };
    struct knob brush_knobs[] = {
        { "Brush opacity", 0, 0.8f, 0.02f, &brush_opacity, 1, "brush.opacity" },
        { "Brush frequency x", 0.002f, 0.08f, 0.002f, &brush_fx, 1, "brush.freq-x" },
        { "Brush frequency y", 0.05f, 1.2f, 0.05f, &brush_fy, 1, "brush.freq-y" },
        { "Brush octaves", 1, 5, 1, &brush_oct, 1, "brush.octaves" },
        { "Brush contrast", 0.1f, 1, 0.02f, &brush_contrast, 1, "brush.contrast" },
        { "Brush angle rise (run 2)", 0, 3, 1, &brush_rise, 1, "brush.angle.rise" },
    };
    heading(ctx, c, "Motion");
    note(ctx, c, "Drag any window while you move these. The engine reads the values each frame.");
    int cols = c->w >= 640 ? 2 : 1;
    float kw = (c->w - (cols - 1) * LP_SPACE_6) / cols;
    lp_id base = LP_ID("gallery.motion");
    int changed = 0;
    for (int i = 0; i < 13; i++) {
        struct knob *k = &knobs[i];
        lp_rect r = LP_RECT(c->x + (i % cols) * (kw + LP_SPACE_6), c->y + (i / cols) * (LP_SLIDER_H + LP_SPACE_3), kw, LP_SLIDER_H);
        if (lp_slider(ctx, lp_id_index(base, i), r, k->value, (lp_slider_opts){ .min = k->min, .max = k->max, .step = k->step, .label = k->label, .show_value = 1, .format = k->step < 0.01f ? fmt3 : (k->step >= 1 ? fmt0 : fmt2) })) changed = 1;
    }
    p->slosh.gain = slosh_gain_scaled * 1e-5f;
    c->y += ((13 + cols - 1) / cols) * (LP_SLIDER_H + LP_SPACE_3);
    section_end(c);

    heading(ctx, c, "Brush");
    int brush_changed = 0;
    for (int i = 0; i < 6; i++) {
        struct knob *k = &brush_knobs[i];
        lp_rect r = LP_RECT(c->x + (i % cols) * (kw + LP_SPACE_6), c->y + (i / cols) * (LP_SLIDER_H + LP_SPACE_3), kw, LP_SLIDER_H);
        if (lp_slider(ctx, lp_id_index(base, 20 + i), r, k->value, (lp_slider_opts){ .min = k->min, .max = k->max, .step = k->step, .label = k->label, .show_value = 1, .format = k->step < 0.01f ? fmt3 : (k->step >= 1 ? fmt0 : fmt2) })) brush_changed = 1;
    }
    if (brush_changed) {
        b->opacity = brush_opacity; b->freq_x = brush_fx; b->freq_y = brush_fy; b->octaves = (int)brush_oct; b->contrast = brush_contrast; b->rise = (int)brush_rise;
        lp_brush_tile_reset();
        ctx->dirty = 1;
    }
    c->y += ((6 + cols - 1) / cols) * (LP_SLIDER_H + LP_SPACE_3);
    section_end(c);

    /* the patch: tokens.json paths → values */
    char patch[1600];
    size_t off = 0;
    off += (size_t)snprintf(patch + off, sizeof patch - off, "{\n");
    for (int i = 0; i < 13; i++) off += (size_t)snprintf(patch + off, sizeof patch - off, "  \"%s\": %g,\n", knobs[i].path, *knobs[i].value * knobs[i].scale);
    for (int i = 0; i < 6; i++) off += (size_t)snprintf(patch + off, sizeof patch - off, "  \"%s\": %g%s\n", brush_knobs[i].path, *brush_knobs[i].value, i < 5 ? "," : "");
    snprintf(patch + off, sizeof patch - off, "}");
    lp_size bs = lp_button_measure(ctx, "Copy JSON patch", (lp_button_opts){ .variant = LP_BUTTON_PRIMARY, .icon = LP_ICON_COUNT });
    if (lp_button(ctx, lp_id_index(base, 40), LP_RECT(c->x, c->y, bs.w, bs.h), g->copied ? "Copied" : "Copy JSON patch", (lp_button_opts){ .variant = LP_BUTTON_PRIMARY, .icon = LP_ICON_COUNT })) {
        const char *home = getenv("HOME");
        char path[512];
        snprintf(path, sizeof path, "%s/.cache/maryui/tokens-patch.json", home ? home : "/tmp");
        FILE *f = fopen(path, "w");
        if (f) { fputs(patch, f); fputc('\n', f); fclose(f); }
        g->copied = 1; g->copied_at = ctx->now_ms; ctx->dirty = 1;
    }
    if (g->copied && ctx->now_ms - g->copied_at > 1200) { g->copied = 0; ctx->dirty = 1; }
    lp_size rs = lp_button_measure(ctx, "Reset to tokens", (lp_button_opts){ .icon = LP_ICON_COUNT });
    if (lp_button(ctx, lp_id_index(base, 41), LP_RECT(c->x + bs.w + LP_SPACE_3, c->y, rs.w, rs.h), "Reset to tokens", (lp_button_opts){ .icon = LP_ICON_COUNT })) {
        lp_motion_reset_params();
        *b = lp_brush_params_from_tokens();
        lp_brush_tile_reset();
        ctx->dirty = 1;
    }
    c->y += bs.h + LP_SPACE_3;
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        int lines = 1;
        for (const char *q = patch; *q; q++) if (*q == '\n') lines++;
        lp_rect code = LP_RECT(c->x, c->y, c->w, lines * 16.0f + 2 * LP_SPACE_3);
        lp_fill_solid(ctx->cr, code, LP_PLATINUM_9, LP_RADIUS_SM);
        lp_text_style s = mono_style();
        s.color = LP_PLATINUM_1;
        char line[200];
        const char *q = patch;
        int li = 0;
        while (*q) {
            const char *nl = strchr(q, '\n');
            size_t len = nl ? (size_t)(nl - q) : strlen(q);
            if (len >= sizeof line) len = sizeof line - 1;
            memcpy(line, q, len); line[len] = 0;
            lp_text_draw(ctx->cr, line, LP_RECT(code.x + LP_SPACE_3, code.y + LP_SPACE_3 + li * 16.0f, code.w - 2 * LP_SPACE_3, 16), &s, LP_ALIGN_START);
            li++;
            if (!nl) break;
            q = nl + 1;
        }
        c->y += code.h;
    }
    if (changed) ctx->dirty = 1;
    section_end(c);
}

static void gallery_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    struct gallery *g = state;
    lp_rect area = body;
    /* tabs strip: padding space.2, platinum.1 → platinum.2, hairline below */
    static const lp_segment tabs[5] = { { "Controls", LP_ICON_COUNT }, { "Surfaces", LP_ICON_COUNT }, { "Bubbles", LP_ICON_COUNT }, { "Tokens", LP_ICON_COUNT }, { "Motion", LP_ICON_COUNT } };
    lp_size ts = lp_segmented_measure(ctx, tabs, 5, LP_CONTROL_MD);
    lp_rect strip = lp_rect_cut_top(&area, ts.h + 2 * LP_SPACE_2);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_fill_vgradient(ctx->cr, strip, LP_PLATINUM_1, LP_PLATINUM_2, 0);
        lp_draw_hairline(ctx->cr, strip, LP_EDGE_BOTTOM, LP_EDGE_HAIRLINE);
    }
    lp_segmented(ctx, LP_ID("gallery.tabs"), strip.x + (strip.w - ts.w) / 2, strip.y + LP_SPACE_2, tabs, 5, &g->tab, LP_CONTROL_MD);

    if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_fill_solid(ctx->cr, area, LP_SURFACE_BODY, 0);
    /* body: padding space.4 space.5 space.6; content height measured by a dry layout run */
    float pad_x = LP_SPACE_5, pad_top = LP_SPACE_4, pad_bottom = LP_SPACE_6;
    lp_size content = { area.w, 4000 };
    lp_rect c0 = lp_scroll_begin(ctx, lp_id_index(LP_ID("gallery.scroll"), g->tab), area, content, &g->scroll[g->tab]);
    lp_rect c = LP_RECT(c0.x + pad_x, c0.y + pad_top, area.w - 2 * pad_x - 12, 0);
    switch (g->tab) {
    case 0: controls_tab(g, ctx, &c); break;
    case 1: surfaces_tab(g, ctx, &c); break;
    case 2: bubbles_tab(g, ctx, &c); break;
    case 3: tokens_tab(g, ctx, &c); break;
    default: motion_tab(g, ctx, &c); break;
    }
    lp_scroll_end(ctx);
    (void)pad_bottom;
}

const lp_app lp_app_gallery = {
    .id = "gallery", .title = "Liquid Platinum", .name = "Gallery", .icon = LP_ICON_DROP, .default_rect = { 520, 140, 760, 560 }, .min_size = { 520, 320 }, .singleton = 1, .resizable = 1,
    .create = gallery_create, .paint = gallery_paint, .destroy = gallery_destroy,
};
