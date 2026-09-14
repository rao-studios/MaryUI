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
#include "maryui/lp_skill.h"
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

/* Code points before a byte offset, and the byte offset of a code point. */
static int points_before(const char *s, int byte) {
    int n = 0;
    for (int i = 0; i < byte && s[i]; i++) if ((s[i] & 0xC0) != 0x80) n++;
    return n;
}
static int byte_of_point(const char *s, int point) {
    int i = 0, n = 0;
    while (s[i] && n < point) { i++; while ((s[i] & 0xC0) == 0x80) i++; n++; }
    return i;
}

/* What Mary sees of the editor (PARITY D28): the document, a window of its text around the selection or the
 * caret, and the selection itself. */
static int textedit_surface(void *state, lp_desktop *d, lp_app_surface *out) {
    struct textedit *t = state;
    if (!t) return 0;
    const char *name = t->name.len ? t->name.text : "Untitled";
    snprintf(out->window_title, sizeof out->window_title, "%s", name);
    snprintf(out->document_name, sizeof out->document_name, "%s", name);
    if (t->path[0]) snprintf(out->document_path, sizeof out->document_path, "%s", t->path);
    const lp_text_doc *doc = &t->area.doc;
    const char *text = doc->text ? doc->text : "";
    int start = 0, end = 0;
    lp_text_doc_selection(doc, &start, &end);
    out->document_total = lp_text_doc_char_count(doc);
    int centre = points_before(text, start), lo = centre - LP_SURFACE_TEXT_WINDOW / 2;
    if (lo < 0) lo = 0;
    int hi = lo + LP_SURFACE_TEXT_WINDOW;
    if (hi > out->document_total) hi = out->document_total;
    int lo_byte = byte_of_point(text, lo), hi_byte = byte_of_point(text, hi);
    out->document_text = strndup(text + lo_byte, (size_t)(hi_byte - lo_byte));
    out->document_lower = lo;
    out->document_upper = hi;
    if (end > start) {
        out->selection_text = strndup(text + start, (size_t)(end - start));
        out->selection_lower = points_before(text, start);
        out->selection_upper = points_before(text, end);
        out->selection_editable = 1;
    }
    lp_app_surface_add(out, "textfield", "text field", name, 0, 1);
    lp_app_surface_add(out, "textarea", "text area", "Document", 1, 1);
    lp_app_surface_add(out, "button", "button", "Save", 0, t->dir[0] != 0);
    return 1;
}

void lp_textedit_select(void *state, int start, int end) {
    struct textedit *t = state;
    if (!t) return;
    if (start < 0) start = 0;
    if (end > t->area.doc.len) end = t->area.doc.len;
    t->area.doc.anchor = start;
    t->area.doc.cursor = end;
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

/* MARK: - Mary's skills (PARITY D20, D30) */

static const char *const TE_READ_TOKENS[] = { "read", "document", "text" };
static const char *const TE_READ_PHRASES[] = { "read the document", "what does the document say", "read me the text" };
static const char *const TE_INSERT_TOKENS[] = { "insert", "type", "write", "add" };
static const char *const TE_INSERT_PHRASES[] = { "insert text", "write this down", "add to the document", "type this" };
static const char *const TE_REPLACE_TOKENS[] = { "replace", "rewrite" };
static const char *const TE_REPLACE_PHRASES[] = { "replace the selection", "replace this with", "rewrite the selection" };
static const char *const TE_SAVE_TOKENS[] = { "save" };
static const char *const TE_NOTE_TOKENS[] = { "write", "note", "new", "document" };   /* "write" first: the verb the no-model dispatch peels */
static const char *const TE_NOTE_PHRASES[] = { "write a new note", "new note", "make a new note", "start a new document", "write a note" };
static const char *const TE_SAVE_PHRASES[] = { "save the document", "save this", "save the file" };
static const char *const TE_CLASSES[] = { "document", "text", "selection" };

static const lp_skill textedit_skills[] = {
    { .id = "read", .title = "Read the document", .summary = "Reads the open document: its name, where it lives, its text and what is selected.",
      .effect = LP_SKILL_READ, .kind = "cognitive", .access = "seamless", .triggers = TE_READ_TOKENS, .trigger_count = 3,
      .phrases = TE_READ_PHRASES, .phrase_count = 3, .target_classes = TE_CLASSES, .target_class_count = 3 },
    { .id = "insert_text", .title = "Insert text", .summary = "Types text into the document at the caret, at its end or at its start.",
      .params = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"where\":{\"type\":\"string\",\"enum\":[\"caret\",\"end\",\"start\"]}},\"required\":[\"text\"]}",
      .effect = LP_SKILL_ACT, .kind = "effectful", .access = "reversible", .triggers = TE_INSERT_TOKENS, .trigger_count = 4,
      .phrases = TE_INSERT_PHRASES, .phrase_count = 4, .target_classes = TE_CLASSES, .target_class_count = 3,
      .spoken = "{\"where\":{\"end\":[\"at the end\",\"to the end\",\"at the bottom\"],\"start\":[\"at the top\",\"at the start\",\"at the beginning\"],\"caret\":[\"here\",\"at the cursor\"]}}" },
    { .id = "replace_selection", .title = "Replace the selection", .summary = "Replaces the selected text with new text.",
      .params = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
      .effect = LP_SKILL_ACT, .kind = "effectful", .access = "reversible", .triggers = TE_REPLACE_TOKENS, .trigger_count = 2,
      .phrases = TE_REPLACE_PHRASES, .phrase_count = 3, .target_classes = TE_CLASSES, .target_class_count = 3 },
    { .id = "new_document", .title = "Write a new note", .summary = "Opens a fresh untitled note and types the text into it, leaving any open document alone.",
      .params = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"}}}",
      .effect = LP_SKILL_ACT, .kind = "effectful", .access = "reversible", .triggers = TE_NOTE_TOKENS, .trigger_count = 4,
      .phrases = TE_NOTE_PHRASES, .phrase_count = 5, .target_classes = TE_CLASSES, .target_class_count = 3 },
    { .id = "save", .title = "Save the document", .summary = "Writes the document back where it came from.",
      .effect = LP_SKILL_ACT, .kind = "effectful", .access = "reversible", .triggers = TE_SAVE_TOKENS, .trigger_count = 1,
      .phrases = TE_SAVE_PHRASES, .phrase_count = 3, .target_classes = TE_CLASSES, .target_class_count = 3 },
};

#define TE_READ_MAX 6000

/* "Note", then "Note 2", "Note 3" …: the first name with no file of its own in the folder. */
static void new_note_name(const struct textedit *t, char *out, size_t n) {
    for (int i = 1; i < 1000; i++) {
        if (i == 1) snprintf(out, n, "Note");
        else snprintf(out, n, "Note %d", i);
        char path[LP_FILES_PATH_MAX];
        lp_files_join(t->dir, out, path, sizeof path);
        size_t len = strlen(path);
        snprintf(path + len, sizeof path - len, ".txt");
        if (!lp_files_exists(path, NULL)) return;
    }
}

static int textedit_perform(void *state, lp_desktop *d, const char *skill, const char *args, char *result, size_t n) {
    struct textedit *t = state;
    if (strcmp(skill, "new_document") == 0) {
        /* always a fresh window with an empty note of its own name — never the document in front, never the reopened Untitled */
        char id[12];
        if (!d || !lp_desktop_open_app_with(d, "textedit", NULL, NULL, id)) { snprintf(result, n, "TextEdit would not open."); return -EIO; }
        lp_app_instance *inst = lp_desktop_instance(d, id);
        struct textedit *fresh = inst ? inst->state : NULL;
        if (!fresh) { snprintf(result, n, "TextEdit would not open."); return -EIO; }
        char name[64];
        if (!lp_skill_arg_string(args, "name", name, sizeof name) || !name[0]) new_note_name(fresh, name, sizeof name);
        lp_text_buffer_set(&fresh->name, name);
        fresh->path[0] = 0;
        set_document_text(fresh, "");
        fresh->status[0] = 0;
        fresh->dirty = 0;
        char *text = malloc(LP_SKILL_RESULT_MAX);
        int chars = 0;
        if (text && lp_skill_arg_string(args, "text", text, LP_SKILL_RESULT_MAX) && text[0]) {
            lp_text_doc_insert(&fresh->area.doc, text, (int)strlen(text));
            for (const char *c = text; *c; c++) if (((unsigned char)*c & 0xC0) != 0x80) chars++;
            fresh->dirty = 1;
        }
        free(text);
        if (d->on_app_dirty) d->on_app_dirty(d, fresh->window_id);
        char nameq[520];
        lp_skill_json_escape(name, nameq, sizeof nameq);
        snprintf(result, n, "{\"landed\":true,\"name\":\"%s\",\"chars\":%d,\"summary\":\"%s\"}", nameq, chars,
                 chars ? "Wrote it in a new note." : "Opened a new note.");
        return 0;
    }
    if (!t && d) {                      /* the skill needs a window: open the document */
        lp_desktop_open_app(d, "textedit");
        t = lp_desktop_app_state(d, "textedit");
    }
    if (!t) { snprintf(result, n, "TextEdit would not open."); return -EIO; }
    const char *name = t->name.len ? t->name.text : "Untitled";
    char nameq[520], escaped[TE_READ_MAX * 2 + 8], path[LP_FILES_PATH_MAX * 2];
    lp_skill_json_escape(name, nameq, sizeof nameq);
    lp_skill_json_escape(t->path, path, sizeof path);
    if (strcmp(skill, "read") == 0) {
        int start = 0, end = 0;
        lp_text_doc_selection(&t->area.doc, &start, &end);
        size_t len = (size_t)t->area.doc.len, shown = len > TE_READ_MAX ? TE_READ_MAX : len;
        while (shown && shown < len && ((unsigned char)t->area.doc.text[shown] & 0xC0) == 0x80) shown--;
        char *piece = malloc(shown + 1);
        if (!piece) return -ENOMEM;
        memcpy(piece, t->area.doc.text, shown);
        piece[shown] = 0;
        lp_skill_json_escape(piece, escaped, sizeof escaped);
        free(piece);
        char selected[1200] = "";
        if (end > start) {
            size_t sl = (size_t)(end - start) > 500 ? 500 : (size_t)(end - start);
            char raw[504];
            memcpy(raw, t->area.doc.text + start, sl);
            raw[sl] = 0;
            lp_skill_json_escape(raw, selected, sizeof selected);
        }
        int wrote = snprintf(result, n, "{\"name\":\"%s\",\"path\":\"%s\",\"chars\":%d,\"truncated\":%s,\"text\":\"%s\",\"selection\":{\"start\":%d,\"end\":%d,\"text\":\"%s\"}}",
                             nameq, path, lp_text_doc_char_count(&t->area.doc), shown < len ? "true" : "false", escaped, start, end, selected);
        return wrote < (int)n ? 0 : -ENOBUFS;
    }
    if (strcmp(skill, "insert_text") == 0 || strcmp(skill, "replace_selection") == 0) {
        char *text = malloc(LP_SKILL_RESULT_MAX);
        if (!text) return -ENOMEM;
        if (!lp_skill_arg_string(args, "text", text, LP_SKILL_RESULT_MAX) || !text[0]) {
            free(text);
            snprintf(result, n, strcmp(skill, "insert_text") == 0 ? "What should I insert?" : "What should the selection become?");
            return -EINVAL;
        }
        if (strcmp(skill, "replace_selection") == 0) {
            if (!lp_text_doc_has_selection(&t->area.doc)) {
                free(text);
                snprintf(result, n, "Nothing is selected in the document.");
                return -EINVAL;
            }
        } else {
            char where[16] = "";
            lp_skill_arg_string(args, "where", where, sizeof where);
            if (strcmp(where, "end") == 0) t->area.doc.cursor = t->area.doc.anchor = t->area.doc.len;
            else if (strcmp(where, "start") == 0) t->area.doc.cursor = t->area.doc.anchor = 0;
            else t->area.doc.anchor = t->area.doc.cursor;      /* at the caret, never over a selection */
        }
        lp_text_doc_insert(&t->area.doc, text, (int)strlen(text));
        int chars = 0;
        for (const char *c = text; *c; c++) if (((unsigned char)*c & 0xC0) != 0x80) chars++;
        free(text);
        t->dirty = 1;
        if (d && d->on_app_dirty) d->on_app_dirty(d, t->window_id);
        snprintf(result, n, "{\"landed\":true,\"chars\":%d,\"summary\":\"%s %d character%s in %s.\"}", chars,
                 strcmp(skill, "insert_text") == 0 ? "Inserted" : "Replaced the selection with", chars, chars == 1 ? "" : "s", nameq);
        return 0;
    }
    if (strcmp(skill, "save") == 0) {
        if (!t->dir[0]) { snprintf(result, n, "This document has nowhere to go."); return -ENOENT; }
        save(t, d);
        if (strncmp(t->status, "Could not", 9) == 0) { snprintf(result, n, "%s", t->status); return -EIO; }
        lp_skill_json_escape(t->path, path, sizeof path);
        char shown[LP_FILES_PATH_MAX], shownq[LP_FILES_PATH_MAX * 2];
        lp_files_abbreviate(t->path, shown, sizeof shown);
        lp_skill_json_escape(shown, shownq, sizeof shownq);
        snprintf(result, n, "{\"landed\":true,\"path\":\"%s\",\"summary\":\"Saved %s.\"}", path, shownq);
        return 0;
    }
    return -ENOENT;
}

const lp_app lp_app_textedit = {
    .id = "textedit", .title = "Untitled", .name = "TextEdit", .icon = LP_ICON_PENCIL, .hidden = 1, .dock = 1,
    .default_rect = { 200, 120, 560, 420 }, .min_size = { 320, 220 }, .singleton = 0, .resizable = 1,
    .create = textedit_create, .paint = textedit_paint, .destroy = textedit_destroy,
    .open = textedit_open, .command = textedit_command, .menu_entries = textedit_menu_entries,
    .skills = textedit_skills, .skill_count = 5, .perform = textedit_perform,
    .surface = textedit_surface, .surface_poll_s = 15,
    .summary = "Writes and reads plain text documents.", .discipline = "writing",
};

/* MARK: - Introspection (tests) */

const char *lp_textedit_path(const void *state) { return ((const struct textedit *)state)->path; }
const char *lp_textedit_name(const void *state) { return ((const struct textedit *)state)->name.text; }
const char *lp_textedit_text(const void *state) { return ((const struct textedit *)state)->area.doc.text; }
