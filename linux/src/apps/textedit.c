/* TextEditApp — a plain-text editor with a name field, a Save button, a
 * TextArea and a status bar. Spotlight opens it on ~/Documents/Untitled.txt;
 * the Finder opens it on any text file (lp_desktop_open_path). ⌘/Ctrl+S
 * writes the document back where it came from; ⌘/Ctrl+O shows its folder. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_files.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

struct textedit {
    char window_id[12];
    lp_text_buffer name;
    lp_text_area_state area;
    int dirty;
    int focused_once;
    char status[LP_FILES_PATH_MAX + 64];  /* "Saved ~/Documents/x.txt at 4:12 PM", or an error */
    char dir[LP_FILES_PATH_MAX];   /* where the document lives: ~/Documents for a new one */
    char path[LP_FILES_PATH_MAX];  /* the file, once opened or saved */
};

/* dir/<name>, with ".txt" added only when the name has no extension; the name stays one path component. */
static void document_path(const struct textedit *t, char *out, size_t n) {
    char name[256];
    snprintf(name, sizeof name, "%s", t->name.len ? t->name.text : "Untitled");
    for (char *p = name; *p; p++) if (*p == '/') *p = '-';
    const char *dot = strrchr(name, '.');
    const char *ext = dot && dot != name && dot[1] ? "" : ".txt";
    lp_files_join(t->dir, name, out, n);
    size_t len = strlen(out);
    snprintf(out + len, n - len, "%s", ext);
}

static void set_document_text(struct textedit *t, const char *text) {
    lp_text_doc_set(&t->area.doc, text ? text : "");
    t->area.doc.cursor = t->area.doc.anchor = 0;
    t->area.scroll.y = 0;
}

/* Loads `path` when it exists; the name becomes its basename. */
static void load_path(struct textedit *t, const char *path) {
    char *text = NULL;
    int rc = lp_files_read(path, &text, NULL);
    snprintf(t->path, sizeof t->path, "%s", path);
    lp_files_parent(path, t->dir, sizeof t->dir);
    lp_text_buffer_set(&t->name, lp_files_basename(path));
    if (rc == -ENOENT) { set_document_text(t, ""); t->status[0] = 0; return; }
    if (rc) { snprintf(t->status, sizeof t->status, "Could not open: %s", strerror(-rc)); return; }
    set_document_text(t, text);
    free(text);
    char shown[LP_FILES_PATH_MAX];
    lp_files_abbreviate(path, shown, sizeof shown);
    snprintf(t->status, sizeof t->status, "Opened %s", shown);
    t->dirty = 0;
}

static void save(struct textedit *t, lp_desktop *d) {
    char path[LP_FILES_PATH_MAX];
    if (!t->dir[0]) return; /* the preview has nowhere to go */
    lp_files_mkdir_p(t->dir);
    document_path(t, path, sizeof path);
    int rc = lp_files_write(path, t->area.doc.text, (size_t)t->area.doc.len);
    if (rc) { snprintf(t->status, sizeof t->status, "Could not save: %s", strerror(-rc)); return; }
    /* a rename through the name field leaves the old file where it was (the web's localStorage did too) */
    snprintf(t->path, sizeof t->path, "%s", path);
    t->dirty = 0;
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    int hour = tm.tm_hour % 12;
    if (hour == 0) hour = 12;
    char shown[LP_FILES_PATH_MAX];
    lp_files_abbreviate(path, shown, sizeof shown);
    snprintf(t->status, sizeof t->status, "Saved %s at %d:%02d %s", shown, hour, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
    if (d) lp_desktop_files_changed(d, t->dir);
}

static void *textedit_create(lp_desktop *d, const char *window_id) {
    struct textedit *t = calloc(1, sizeof *t);
    snprintf(t->window_id, sizeof t->window_id, "%s", window_id);
    lp_text_buffer_set(&t->name, "");
    lp_text_doc_init(&t->area.doc);
    lp_files_user_dir(LP_USER_DOCUMENTS, t->dir, sizeof t->dir);
    /* Spotlight's TextEdit reopens ~/Documents/Untitled.txt; a path from the Finder replaces it (open). */
    char path[LP_FILES_PATH_MAX];
    document_path(t, path, sizeof path);
    if (lp_files_exists(path, NULL)) { load_path(t, path); lp_text_buffer_set(&t->name, ""); }
    return t;
}

static void textedit_open(void *state, lp_desktop *d, const char *path) {
    struct textedit *t = state;
    if (!t || !path || !*path) return;
    load_path(t, path);
}

static void textedit_command(void *state, lp_desktop *d, int cmd) {
    struct textedit *t = state;
    switch ((enum lp_textedit_command)cmd) {
    case LP_TEXTEDIT_SAVE: save(t, d); break;
    case LP_TEXTEDIT_OPEN: if (d) { lp_files_mkdir_p(t->dir); lp_desktop_open_path(d, t->dir); } break;
    }
}

static void textedit_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    if (menu != LP_MENU_FILE || m->count + 2 > LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "Open…");
    e->shortcut = "⌘O"; e->command = LP_CMD_APP; e->arg = LP_TEXTEDIT_OPEN;
    e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "Save");
    e->shortcut = "⌘S"; e->command = LP_CMD_APP; e->arg = LP_TEXTEDIT_SAVE;
}

static void textedit_destroy(void *state) {
    struct textedit *t = state;
    if (!t) return;
    lp_text_doc_free(&t->area.doc);
    free(t);
}

void lp_textedit_set_text(void *state, const char *name, const char *text) {
    struct textedit *t = state;
    if (!t) return;
    lp_text_buffer_set(&t->name, name ? name : "");
    lp_text_doc_set(&t->area.doc, text ? text : "");
    t->dirty = 1;
    t->status[0] = 0;
}

static void sync_title(struct textedit *t, lp_desktop *d) {
    char title[160];
    snprintf(title, sizeof title, "%s%s", t->dirty ? "• " : "", t->name.len ? t->name.text : "Untitled");
    int i = lp_wm_find(&d->wm, t->window_id);
    if (i < 0 || strcmp(d->wm.windows[i].title, title) == 0) return;
    lp_wm_action a = { .type = LP_WM_SET_TITLE, .id = t->window_id, .title = title };
    lp_desktop_dispatch(d, &a);
}

static void textedit_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct textedit preview;
    struct textedit *t = state ? state : &preview;
    if (!t->area.doc.text) lp_text_doc_init(&t->area.doc);
    lp_id base = LP_ID("textedit");
    lp_id area_id = lp_id_index(base, 1), name_id = lp_id_index(base, 2), save_id = lp_id_index(base, 3);
    if (!t->focused_once) { ctx->focus = area_id; t->focused_once = 1; }
    lp_rect area = body;
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;

    /* ⌘/Ctrl+S and ⌘/Ctrl+O before the widgets see the key */
    if (ctx->pass == LP_PASS_EVENT && ctx->in.key_pressed && (ctx->in.mods & (LP_MOD_CTRL | LP_MOD_LOGO))) {
        if (ctx->in.keysym == XKB_KEY_s || ctx->in.keysym == XKB_KEY_S) { save(t, state ? d : NULL); ctx->dirty = 1; }
        if ((ctx->in.keysym == XKB_KEY_o || ctx->in.keysym == XKB_KEY_O) && state) { textedit_command(t, d, LP_TEXTEDIT_OPEN); ctx->dirty = 1; }
    }

    /* Toolbar: name field · Save · status */
    lp_rect bar = lp_toolbar(ctx, &area);
    float cy = bar.y + bar.h / 2;
    lp_rect name = LP_RECT(bar.x, cy - LP_SIZE_CONTROL_HEIGHT / 2, 200, LP_SIZE_CONTROL_HEIGHT);
    if (lp_text_field(ctx, name_id, name, &t->name, (lp_text_field_opts){ .placeholder = "Untitled", .icon = LP_ICON_DOCUMENT })) t->dirty = 1;
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_DOWNLOAD, 0, 0 };
    lp_size bs = lp_button_measure(ctx, "Save", bo);
    if (lp_button(ctx, save_id, LP_RECT(name.x + name.w + LP_SPACE_2, cy - bs.h / 2, bs.w, bs.h), "Save", bo)) { save(t, state ? d : NULL); ctx->dirty = 1; }
    if (draw && t->status[0]) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS; st.color = LP_INK_TERTIARY; st.ellipsize = 1;
        float x = name.x + name.w + LP_SPACE_2 + bs.w + LP_SPACE_3;
        lp_text_draw(cr, t->status, LP_RECT(x, bar.y, bar.x + bar.w - x, bar.h), &st, LP_ALIGN_END);
    }

    /* Status bar: 22px, like the Finder's */
    lp_rect status = lp_rect_cut_bottom(&area, 22);

    /* The document */
    if (draw) lp_fill_solid(cr, area, LP_SURFACE_BODY, 0);
    lp_rect well = lp_rect_inset(area, LP_SPACE_2, LP_SPACE_2);
    if (lp_text_area(ctx, area_id, well, &t->area, (lp_text_area_opts){ .placeholder = "Type something…" })) t->dirty = 1;

    if (ctx->pass == LP_PASS_EVENT && state) sync_title(t, d);

    if (draw) {
        lp_fill_vgradient(cr, status, LP_PLATINUM_2, LP_PLATINUM_3, 0);
        static const lp_shadow_layer top[] = { { 1, 0, 1, 0, 0, { 1, 1, 1, 0.78f } } };
        lp_draw_inset_shadows(cr, status, 0, top, 1);
        lp_draw_hairline(cr, status, LP_EDGE_TOP, LP_EDGE_DIVIDER);
        int line, col;
        lp_text_doc_line_col(&t->area.doc, t->area.doc.cursor, &line, &col);
        int words = lp_text_doc_word_count(&t->area.doc), chars = lp_text_doc_char_count(&t->area.doc);
        char text[240];
        snprintf(text, sizeof text, "%d word%s · %d character%s · Ln %d, Col %d%s", words, words == 1 ? "" : "s", chars, chars == 1 ? "" : "s",
            line, col, t->dirty ? " · Edited" : "");
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS; st.color = LP_INK_SECONDARY; st.tabular_nums = 1;
        lp_text_draw(cr, text, status, &st, LP_ALIGN_CENTER);
    }
}

const lp_app lp_app_textedit = {
    .id = "textedit", .title = "Untitled", .name = "TextEdit", .icon = LP_ICON_PENCIL, .hidden = 1, .dock = 1,
    .default_rect = { 200, 120, 560, 420 }, .min_size = { 320, 220 }, .singleton = 0, .resizable = 1,
    .create = textedit_create, .paint = textedit_paint, .destroy = textedit_destroy,
    .open = textedit_open, .command = textedit_command, .menu_entries = textedit_menu_entries,
};

/* MARK: - Introspection (tests) */

const char *lp_textedit_path(const void *state) { return ((const struct textedit *)state)->path; }
const char *lp_textedit_name(const void *state) { return ((const struct textedit *)state)->name.text; }
const char *lp_textedit_text(const void *state) { return ((const struct textedit *)state)->area.doc.text; }
