/* Preview — a picture or a PDF in a window: fitted to the window or at a
 * zoom step, turned in quarter turns, paged through, and stepped to the next
 * picture in its folder. The documents are lp_image (src/draw/lp_image.c);
 * the Finder opens it on any picture or PDF (lp_desktop_open_path). Linux only
 * (PARITY D15). */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_image.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define PAD LP_SPACE_5
#define STATUS_H 22

struct preview {
    char window_id[12];
    char path[LP_FILES_PATH_MAX];
    lp_image_doc doc;
    int err;                  /* lp_image_open's result, when it failed */
    int page;
    int turns;                /* quarter turns clockwise */
    int fit;                  /* the scale follows the window */
    float scale;              /* the scale in use: the fit's while fit */
    float view_w, view_h;     /* the viewport at the last paint, to zoom about its centre */
    lp_scroll_state scroll;
};

static void load(struct preview *p, const char *path) {
    lp_image_close(&p->doc);
    snprintf(p->path, sizeof p->path, "%s", path);
    p->err = lp_image_open(&p->doc, path);
    p->page = 0;
    p->turns = 0;
    p->fit = 1;
    p->scroll.x = p->scroll.y = 0;
}

static void set_title(struct preview *p, lp_desktop *d) {
    if (!d || !p->path[0] || lp_wm_find(&d->wm, p->window_id) < 0) return;
    char name[LP_FILES_NAME_MAX];
    lp_files_display_name(p->path, name, sizeof name);
    lp_wm_action a = { .type = LP_WM_SET_TITLE, .id = p->window_id, .title = name };
    lp_desktop_dispatch(d, &a);
}

/* A new scale, keeping the point at the viewport's centre where it is. */
static void zoom_to(struct preview *p, float scale) {
    float k = scale / (p->scale > 0 ? p->scale : 1);
    p->scroll.x = fmaxf(0, (p->scroll.x + p->view_w / 2) * k - p->view_w / 2);
    p->scroll.y = fmaxf(0, (p->scroll.y + p->view_h / 2) * k - p->view_h / 2);
    p->scale = scale;
    p->fit = 0;
}

static void preview_command(void *state, lp_desktop *d, int cmd) {
    struct preview *p = state;
    if (!p) return;
    int open = p->path[0] && !p->err;
    switch ((enum lp_preview_command)cmd) {
    case LP_PREVIEW_PREVIOUS:
    case LP_PREVIEW_NEXT: {
        char next[LP_FILES_PATH_MAX];
        if (p->path[0] && lp_image_sibling(p->path, cmd == LP_PREVIEW_NEXT ? 1 : -1, next, sizeof next)) {
            load(p, next);
            set_title(p, d);
        }
        break;
    }
    case LP_PREVIEW_PREVIOUS_PAGE: if (p->page > 0) { p->page--; p->scroll.y = 0; } break;
    case LP_PREVIEW_NEXT_PAGE: if (p->page + 1 < p->doc.page_count) { p->page++; p->scroll.y = 0; } break;
    case LP_PREVIEW_ZOOM_IN: if (open) zoom_to(p, lp_image_zoom_step(p->scale, 1)); break;
    case LP_PREVIEW_ZOOM_OUT: if (open) zoom_to(p, lp_image_zoom_step(p->scale, -1)); break;
    case LP_PREVIEW_ACTUAL_SIZE: if (open) zoom_to(p, 1); break;
    case LP_PREVIEW_ZOOM_TO_FIT: p->fit = 1; p->scroll.x = p->scroll.y = 0; break;
    case LP_PREVIEW_ROTATE_LEFT: p->turns = (p->turns + 3) % 4; break;
    case LP_PREVIEW_ROTATE_RIGHT: p->turns = (p->turns + 1) % 4; break;
    case LP_PREVIEW_SHOW_IN_FINDER: {
        char dir[LP_FILES_PATH_MAX];
        if (p->path[0]) lp_files_parent(p->path, dir, sizeof dir);
        else lp_files_user_dir(LP_USER_HOME, dir, sizeof dir);
        if (d) lp_desktop_open_path(d, dir);
        break;
    }
    }
}

static int key_command(const lp_input *in) {
    if (in->mods & (LP_MOD_CTRL | LP_MOD_LOGO)) {
        switch (in->keysym) {
        case XKB_KEY_plus: case XKB_KEY_equal: case XKB_KEY_KP_Add: return LP_PREVIEW_ZOOM_IN;
        case XKB_KEY_minus: case XKB_KEY_KP_Subtract: return LP_PREVIEW_ZOOM_OUT;
        case XKB_KEY_0: return LP_PREVIEW_ACTUAL_SIZE;
        case XKB_KEY_9: return LP_PREVIEW_ZOOM_TO_FIT;
        case XKB_KEY_l: case XKB_KEY_L: return LP_PREVIEW_ROTATE_LEFT;
        case XKB_KEY_r: case XKB_KEY_R: return LP_PREVIEW_ROTATE_RIGHT;
        default: return -1;
        }
    }
    switch (in->keysym) {
    case XKB_KEY_Left: return LP_PREVIEW_PREVIOUS;
    case XKB_KEY_Right: return LP_PREVIEW_NEXT;
    case XKB_KEY_Page_Up: return LP_PREVIEW_PREVIOUS_PAGE;
    case XKB_KEY_Page_Down: case XKB_KEY_space: return LP_PREVIEW_NEXT_PAGE;
    default: return -1;
    }
}

static int tool(lp_ctx *ctx, lp_id id, lp_rect r, lp_icon icon, int disabled) {
    lp_button_opts o = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, icon, 1, disabled };
    return lp_button(ctx, id, r, "", o) && !disabled;
}

static int toolbar(lp_ctx *ctx, struct preview *p, lp_rect bar, lp_id base) {
    lp_size s = lp_button_measure(ctx, "", (lp_button_opts){ LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_ZOOM_IN, 1, 0 });
    float y = bar.y + (bar.h - s.h) / 2, gap = LP_SPACE_1, x = bar.x;
    int open = p->path[0] && !p->err, cmd = -1;
    if (tool(ctx, lp_id_index(base, 1), LP_RECT(x, y, s.w, s.h), LP_ICON_CHEVRON_LEFT, !p->path[0])) cmd = LP_PREVIEW_PREVIOUS;
    x += s.w + gap;
    if (tool(ctx, lp_id_index(base, 2), LP_RECT(x, y, s.w, s.h), LP_ICON_CHEVRON_RIGHT, !p->path[0])) cmd = LP_PREVIEW_NEXT;
    x += s.w + LP_SPACE_4;
    if (open && p->doc.page_count > 1) {
        if (tool(ctx, lp_id_index(base, 3), LP_RECT(x, y, s.w, s.h), LP_ICON_CHEVRON_UP, p->page == 0)) cmd = LP_PREVIEW_PREVIOUS_PAGE;
        x += s.w + gap;
        if (tool(ctx, lp_id_index(base, 4), LP_RECT(x, y, s.w, s.h), LP_ICON_CHEVRON_DOWN, p->page + 1 >= p->doc.page_count)) cmd = LP_PREVIEW_NEXT_PAGE;
        x += s.w + LP_SPACE_2;
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            char label[48];
            snprintf(label, sizeof label, "Page %d of %d", p->page + 1, p->doc.page_count);
            lp_text_style st = lp_text_style_default();
            st.size_px = LP_TEXT_SM;
            st.color = LP_INK_SECONDARY;
            st.tabular_nums = 1;
            lp_text_draw(ctx->cr, label, LP_RECT(x, bar.y, 120, bar.h), &st, LP_ALIGN_START);
        }
    }
    static const struct { lp_icon icon; int cmd; } RIGHT[4] = {
        { LP_ICON_ZOOM_OUT, LP_PREVIEW_ZOOM_OUT }, { LP_ICON_ZOOM_IN, LP_PREVIEW_ZOOM_IN },
        { LP_ICON_EXPAND, LP_PREVIEW_ZOOM_TO_FIT }, { LP_ICON_RELOAD, LP_PREVIEW_ROTATE_RIGHT },
    };
    float rx = bar.x + bar.w - 4 * s.w - 3 * gap;
    for (int i = 0; i < 4; i++) {
        if (i == 2) rx += LP_SPACE_2;   /* zoom apart from fit and turn */
        lp_rect r = LP_RECT(rx - (i >= 2 ? LP_SPACE_2 : 0) + i * (s.w + gap), y, s.w, s.h);
        if (tool(ctx, lp_id_index(base, 5 + i), r, RIGHT[i].icon, !open)) cmd = RIGHT[i].cmd;
    }
    return cmd;
}

static void paint_message(lp_ctx *ctx, struct preview *p, lp_desktop *d, lp_rect area, lp_id base) {
    const char *title, *detail;
    if (!p->path[0]) {
        title = "No picture open";
        detail = "Open a picture or a PDF from the Finder.";
    } else {
        title = "Preview can’t open this file";
        detail = p->err == -ENOTSUP ? "Nothing in this build of the desktop decodes it."
               : p->err == -EINVAL ? "It is damaged, or not the kind of file its name says."
               : strerror(-p->err);
    }
    float h = 48 + LP_SPACE_3 + 22 + 18 + LP_SPACE_4 + LP_SIZE_CONTROL_HEIGHT;
    float y = area.y + (area.h - h) / 2;
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_MD, LP_ICON_FOLDER, 0, 0 };
    lp_size bs = lp_button_measure(ctx, "Show in Finder", bo);
    lp_rect button = LP_RECT(area.x + (area.w - bs.w) / 2, y + h - bs.h, bs.w, bs.h);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        cairo_t *cr = ctx->cr;
        lp_icon_draw(cr, LP_ICON_IMAGE, area.x + (area.w - 48) / 2, y, 48, 1.4f, LP_INK_TERTIARY);
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_LG;
        st.weight = LP_TEXT_WEIGHT_SEMIBOLD;
        st.color = LP_INK_SECONDARY;
        st.emboss = 1;
        lp_text_draw(cr, title, LP_RECT(area.x, y + 48 + LP_SPACE_3, area.w, 22), &st, LP_ALIGN_CENTER);
        st = lp_text_style_default();
        st.size_px = LP_TEXT_SM;
        st.color = LP_INK_TERTIARY;
        lp_text_draw(cr, detail, LP_RECT(area.x, y + 48 + LP_SPACE_3 + 22, area.w, 18), &st, LP_ALIGN_CENTER);
    }
    if (lp_button(ctx, lp_id_index(base, 30), button, "Show in Finder", bo) && d) {
        preview_command(p, d, LP_PREVIEW_SHOW_IN_FINDER);
        ctx->dirty = 1;
    }
}

static void paint_document(lp_ctx *ctx, struct preview *p, lp_rect area, lp_id base) {
    if (ctx->pass == LP_PASS_EVENT && (ctx->in.mods & (LP_MOD_CTRL | LP_MOD_LOGO)) && ctx->in.scroll_y != 0 && lp_hit(ctx, area)) {
        preview_command(p, NULL, ctx->in.scroll_y < 0 ? LP_PREVIEW_ZOOM_IN : LP_PREVIEW_ZOOM_OUT);
        ctx->in.scroll_y = 0;   /* a zoom, not a scroll as well */
        ctx->dirty = 1;
    }
    p->view_w = area.w;
    p->view_h = area.h;
    lp_size page = lp_image_page_size(&p->doc, p->page);
    if (p->fit) p->scale = lp_image_fit_scale(page, p->turns, (lp_size){ area.w, area.h }, PAD);
    lp_size turned = lp_image_rotated(page, p->turns);
    float pw = roundf(turned.w * p->scale), ph = roundf(turned.h * p->scale);
    lp_size extent = { fmaxf(pw + 2 * PAD, area.w), fmaxf(ph + 2 * PAD, area.h) };
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 20), area, extent, &p->scroll);
    lp_rect r = LP_RECT(roundf(c.x + (extent.w - pw) / 2), roundf(c.y + (extent.h - ph) / 2), pw, ph);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        cairo_t *cr = ctx->cr;
        lp_fill_solid(cr, LP_RECT(r.x, r.y + 2, r.w, r.h), LP_RGBA(0, 0, 0, 0.12f), 0);   /* the page lifts off the desk */
        lp_image_draw(cr, &p->doc, p->page, r, p->turns);
        lp_draw_focus_ring(cr, r, 0, LP_EDGE_DIVIDER, 1);
    }
    lp_scroll_end(ctx);
}

static void paint_status(lp_ctx *ctx, const struct preview *p, lp_rect status) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    lp_fill_vgradient(cr, status, LP_PLATINUM_2, LP_PLATINUM_3, 0);
    static const lp_shadow_layer top[] = { { 1, 0, 1, 0, 0, { 1, 1, 1, 0.78f } } };
    lp_draw_inset_shadows(cr, status, 0, top, 1);
    lp_draw_hairline(cr, status, LP_EDGE_TOP, LP_EDGE_DIVIDER);
    if (!p->path[0]) return;
    char name[LP_FILES_NAME_MAX], text[LP_FILES_NAME_MAX + 96];
    lp_files_display_name(p->path, name, sizeof name);
    lp_size s = lp_image_page_size(&p->doc, p->page);
    if (p->err) snprintf(text, sizeof text, "%s", name);
    else if (p->doc.type == LP_IMAGE_PDF)
        snprintf(text, sizeof text, "%s · Page %d of %d · %.0f × %.0f pt · %.0f%%", name, p->page + 1, p->doc.page_count, s.w, s.h, p->scale * 100);
    else snprintf(text, sizeof text, "%s · %.0f × %.0f px · %.0f%%", name, s.w, s.h, p->scale * 100);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS;
    st.color = LP_INK_SECONDARY;
    st.tabular_nums = 1;
    st.ellipsize = 1;
    lp_text_draw(cr, text, lp_rect_inset(status, LP_SPACE_3, 0), &st, LP_ALIGN_CENTER);
}

static void preview_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct preview empty = { .fit = 1, .scale = 1, .doc = { .cache_page = -1 } };
    struct preview *p = state ? state : &empty;
    lp_id base = LP_ID("preview");
    if (ctx->pass == LP_PASS_EVENT && ctx->in.key_pressed && ctx->in.keysym && state) {
        int cmd = key_command(&ctx->in);
        if (cmd >= 0) { preview_command(p, d, cmd); ctx->dirty = 1; }
    }
    lp_rect area = body;
    lp_rect bar = lp_toolbar(ctx, &area);
    lp_rect status = lp_rect_cut_bottom(&area, STATUS_H);
    int cmd = toolbar(ctx, p, bar, base);
    if (cmd >= 0 && ctx->pass == LP_PASS_EVENT && state) { preview_command(p, d, cmd); ctx->dirty = 1; }
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_fill_solid(ctx->cr, area, LP_PLATINUM_2, 0);
    if (p->path[0] && !p->err) paint_document(ctx, p, area, base);
    else paint_message(ctx, p, state ? d : NULL, area, base);
    paint_status(ctx, p, status);
}

static void entry(lp_menu_model *m, const char *label, const char *shortcut, int arg, int checked, int disabled) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "%s", label);
    e->shortcut = shortcut;
    e->command = LP_CMD_APP;
    e->arg = arg;
    e->checked = checked;
    e->disabled = disabled;
}

static void separator(lp_menu_model *m) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    e->separator = 1;
}

static void preview_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    const struct preview *p = state;
    if (!p) return;
    int open = p->path[0] && !p->err, pages = open && p->doc.page_count > 1;
    switch (menu) {
    case LP_MENU_FILE:
        entry(m, "Show in Finder", NULL, LP_PREVIEW_SHOW_IN_FINDER, 0, 0);
        break;
    case LP_MENU_VIEW:
        separator(m);
        entry(m, "Actual Size", "⌘0", LP_PREVIEW_ACTUAL_SIZE, 0, !open);
        entry(m, "Zoom to Fit", "⌘9", LP_PREVIEW_ZOOM_TO_FIT, p->fit, !open);
        entry(m, "Zoom In", "⌘+", LP_PREVIEW_ZOOM_IN, 0, !open);
        entry(m, "Zoom Out", "⌘−", LP_PREVIEW_ZOOM_OUT, 0, !open);
        entry(m, "Rotate Left", "⌘L", LP_PREVIEW_ROTATE_LEFT, 0, !open);
        entry(m, "Rotate Right", "⌘R", LP_PREVIEW_ROTATE_RIGHT, 0, !open);
        break;
    case LP_MENU_GO:
        entry(m, "Previous Document", "←", LP_PREVIEW_PREVIOUS, 0, !p->path[0]);
        entry(m, "Next Document", "→", LP_PREVIEW_NEXT, 0, !p->path[0]);
        entry(m, "Previous Page", "⇞", LP_PREVIEW_PREVIOUS_PAGE, 0, !pages || p->page == 0);
        entry(m, "Next Page", "⇟", LP_PREVIEW_NEXT_PAGE, 0, !pages || p->page + 1 >= p->doc.page_count);
        break;
    default:
        break;
    }
}

static void *preview_create(lp_desktop *d, const char *window_id) {
    struct preview *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    snprintf(p->window_id, sizeof p->window_id, "%s", window_id);
    p->doc.cache_page = -1;
    p->fit = 1;
    p->scale = 1;
    return p;
}

static void preview_open(void *state, lp_desktop *d, const char *path) {
    if (state && path && *path) load(state, path);   /* the window's title comes with the open */
}

static void preview_destroy(void *state) {
    struct preview *p = state;
    if (!p) return;
    lp_image_close(&p->doc);
    free(p);
}

const lp_app lp_app_preview = {
    .id = "preview", .title = "Preview", .name = "Preview", .icon = LP_ICON_IMAGE, .object = "docImage", .hidden = 1, .dock = 1,
    .default_rect = { NAN, NAN, 720, 540 }, .min_size = { 360, 280 }, .singleton = 0, .resizable = 1,
    .create = preview_create, .paint = preview_paint, .destroy = preview_destroy,
    .open = preview_open, .command = preview_command, .menu_entries = preview_menu_entries,
};

/* MARK: - Introspection (tests) */

const char *lp_preview_path(const void *state) { return ((const struct preview *)state)->path; }
int lp_preview_error(const void *state) { return ((const struct preview *)state)->err; }
int lp_preview_page(const void *state) { return ((const struct preview *)state)->page; }
int lp_preview_turns(const void *state) { return ((const struct preview *)state)->turns; }
int lp_preview_fits(const void *state) { return ((const struct preview *)state)->fit; }
float lp_preview_scale(const void *state) { return ((const struct preview *)state)->scale; }
