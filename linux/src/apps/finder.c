/* FinderApp — the file manager: a toolbar (Back/Forward, icon or list view,
 * search), a source-list sidebar (the user's folders, the volumes, the Trash),
 * the listing of a real directory as icons or as a sortable list, a path bar
 * of crumbs and a status bar. Items open on double-click (folders navigate,
 * text files go to TextEdit), rename inline, move to the Trash, copy, paste,
 * duplicate, and drag between windows; a right-click opens a context menu.
 * Every filesystem read happens in the EVENT pass (or in a model callback);
 * the DRAW pass paints cached state. lp-render shows the mock listing. */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_files.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

int lp_finder_mock_fill(lp_file_list *l);

#define HISTORY_MAX 32
#define STATUS_H 22
#define PATHBAR_H 22
#define TILE_MIN_W 96
#define TILE_H 88
#define TYPEAHEAD_MS 1000.0
#define DRAG_THRESHOLD 4.0f
#define MAX_CRUMBS 24

enum drop_kind { DROP_NONE, DROP_ITEM, DROP_SIDEBAR, DROP_BODY };

struct sidebar_entry { char label[64]; char path[LP_FILES_PATH_MAX]; lp_icon icon; int trash; };

struct finder {
    char window_id[12];
    lp_desktop *desktop;         /* for the watch hook at destroy (NULL for the preview) */
    char path[LP_FILES_PATH_MAX];
    lp_file_list list;
    int *sel;                    /* per entry */
    int *vis;                    /* the entries the search shows, in order */
    int nvis, cap;
    int anchor, cursor;          /* ⇧-range anchor and the keyboard cursor: entry indices, -1 */
    char history[HISTORY_MAX][LP_FILES_PATH_MAX];
    int hist_count, hist_pos;
    int view;                    /* 0 icons, 1 list */
    lp_text_buffer query;
    lp_scroll_state scroll;
    enum lp_file_sort sort;
    int sort_desc;
    int hidden;
    int stale, preview, title_pending;
    char status[LP_FILES_NAME_MAX + 160];
    /* the pointer gesture */
    int press_item, press_was_selected;
    float press_x, press_y;
    int marquee;
    float mq_x0, mq_y0, mq_x1, mq_y1; /* content coordinates (scrolled) */
    /* rename */
    int renaming;                /* entry index, -1 */
    lp_text_buffer rename_buf;
    int rename_focus_pending;
    /* drop target */
    enum drop_kind drop_kind;
    int drop_index;
    char drop_dir[LP_FILES_PATH_MAX];
    /* type-ahead */
    char typed[64];
    double typed_ms;
    int reveal;                  /* an entry to scroll into view, -1 */
    /* sidebar */
    struct sidebar_entry side[16];
    int nside, nfavorites;
    /* the path bar, laid out in the DRAW pass for the EVENT pass */
    lp_rect crumbs[MAX_CRUMBS];
    char crumb_paths[MAX_CRUMBS][LP_FILES_PATH_MAX];
    int ncrumbs;
};

/* MARK: - The listing and the selection */

static int matches(const lp_file_entry *e, const char *q);

/* The entries the search shows, in order. */
static void filter(struct finder *f) {
    f->nvis = 0;
    for (int i = 0; i < f->list.count; i++) if (matches(&f->list.entries[i], f->query.text)) f->vis[f->nvis++] = i;
}

static void grow(struct finder *f) {
    if (f->list.count <= f->cap) return;
    int cap = f->list.count + 16;
    f->sel = realloc(f->sel, (size_t)cap * sizeof *f->sel);
    f->vis = realloc(f->vis, (size_t)cap * sizeof *f->vis);
    f->cap = cap;
}

static int selected_count(const struct finder *f) {
    int n = 0;
    for (int i = 0; i < f->list.count; i++) n += f->sel[i] != 0;
    return n;
}

static void clear_selection(struct finder *f) {
    for (int i = 0; i < f->list.count; i++) f->sel[i] = 0;
}

static void select_only(struct finder *f, int i) {
    clear_selection(f);
    if (i >= 0 && i < f->list.count) { f->sel[i] = 1; f->anchor = f->cursor = i; }
}

static int first_selected(const struct finder *f) {
    for (int i = 0; i < f->list.count; i++) if (f->sel[i]) return i;
    return -1;
}

static void item_path(const struct finder *f, int i, char *out, size_t n) { lp_files_join(f->path, f->list.entries[i].name, out, n); }

static int in_trash(const struct finder *f) {
    char trash[LP_FILES_PATH_MAX];
    lp_files_user_dir(LP_USER_TRASH, trash, sizeof trash);
    return strcmp(f->path, trash) == 0;
}

/* Re-reads the directory, keeping the selection by name. */
static void reload(struct finder *f) {
    if (f->preview) { f->stale = 0; return; }
    char (*keep)[LP_FILES_NAME_MAX] = NULL;
    int nkeep = 0;
    int n = f->list.count ? selected_count(f) : 0;
    if (n) {
        keep = calloc((size_t)n, sizeof *keep);
        for (int i = 0; i < f->list.count && keep; i++) if (f->sel[i]) snprintf(keep[nkeep++], LP_FILES_NAME_MAX, "%s", f->list.entries[i].name);
    }
    char cursor_name[LP_FILES_NAME_MAX] = "", rename_name[LP_FILES_NAME_MAX] = "";
    if (f->cursor >= 0 && f->cursor < f->list.count) snprintf(cursor_name, sizeof cursor_name, "%s", f->list.entries[f->cursor].name);
    if (f->renaming >= 0 && f->renaming < f->list.count) snprintf(rename_name, sizeof rename_name, "%s", f->list.entries[f->renaming].name);
    int rc = lp_files_list(f->path, f->hidden, &f->list);
    if (rc == 0) lp_files_sort(&f->list, f->sort, f->sort_desc);
    else snprintf(f->status, sizeof f->status, "Could not read “%s”: %s", lp_files_basename(f->path), strerror(-rc));
    grow(f);
    clear_selection(f);
    f->anchor = f->cursor = -1;
    for (int k = 0; k < nkeep; k++) { int i = lp_files_find(&f->list, keep[k]); if (i >= 0) { f->sel[i] = 1; if (f->anchor < 0) f->anchor = i; } }
    if (cursor_name[0]) f->cursor = lp_files_find(&f->list, cursor_name);
    f->renaming = rename_name[0] ? lp_files_find(&f->list, rename_name) : -1;
    free(keep);
    filter(f);
    f->stale = 0;
}

static void watch(struct finder *f, lp_desktop *d, const char *dir, int on) {
    if (d && d->watch && dir && *dir && !f->preview) d->watch(d, dir, on);
}

static void set_path(struct finder *f, lp_desktop *d, const char *path) {
    if (strcmp(f->path, path) != 0) watch(f, d, f->path, 0);
    snprintf(f->path, sizeof f->path, "%s", path);
    watch(f, d, f->path, 1);
    f->renaming = -1;
    f->marquee = 0;
    f->scroll.y = 0;
    f->status[0] = 0;
    lp_files_list_free(&f->list);
    reload(f);
    clear_selection(f);
    f->anchor = f->cursor = -1;
    f->title_pending = 1;
}

/* Goes somewhere, remembering it for Back. */
static void navigate(struct finder *f, lp_desktop *d, const char *path) {
    if (f->hist_count == 0 || strcmp(f->history[f->hist_pos], path) != 0) {
        /* drop the forward entries, push */
        f->hist_count = f->hist_count ? f->hist_pos + 1 : 0;
        if (f->hist_count == HISTORY_MAX) { memmove(f->history[0], f->history[1], (HISTORY_MAX - 1) * sizeof f->history[0]); f->hist_count--; }
        snprintf(f->history[f->hist_count], sizeof f->history[0], "%s", path);
        f->hist_pos = f->hist_count++;
    }
    set_path(f, d, path);
}

static void go_history(struct finder *f, lp_desktop *d, int delta) {
    int next = f->hist_pos + delta;
    if (next < 0 || next >= f->hist_count) return;
    f->hist_pos = next;
    set_path(f, d, f->history[next]);
}

static void sidebar_build(struct finder *f) {
    f->nside = 0;
    for (enum lp_user_dir k = LP_USER_HOME; k <= LP_USER_DOWNLOADS; k++) {
        struct sidebar_entry *e = &f->side[f->nside++];
        snprintf(e->label, sizeof e->label, "%s", lp_files_user_dir_label(k));
        lp_files_user_dir(k, e->path, sizeof e->path);
        e->icon = lp_files_user_dir_icon(k);
        e->trash = 0;
    }
    f->nfavorites = f->nside;
    lp_volume vols[8];
    int nv = f->preview ? 0 : lp_files_volumes(vols, 8);
    for (int i = 0; i < nv && f->nside < 15; i++) {
        struct sidebar_entry *e = &f->side[f->nside++];
        snprintf(e->label, sizeof e->label, "%.63s", vols[i].name);
        snprintf(e->path, sizeof e->path, "%s", vols[i].path);
        e->icon = LP_ICON_DRIVE;
        e->trash = 0;
    }
    struct sidebar_entry *t = &f->side[f->nside++];
    snprintf(t->label, sizeof t->label, "Trash");
    lp_files_user_dir(LP_USER_TRASH, t->path, sizeof t->path);
    t->icon = LP_ICON_TRASH;
    t->trash = 1;
}

/* MARK: - Lifecycle */

static void *finder_create(lp_desktop *d, const char *window_id) {
    struct finder *f = calloc(1, sizeof *f);
    snprintf(f->window_id, sizeof f->window_id, "%s", window_id);
    f->desktop = d;
    f->anchor = f->cursor = f->press_item = f->renaming = f->reveal = -1;
    f->drop_index = -1;
    sidebar_build(f);
    navigate(f, d, lp_files_home());
    return f;
}

static void finder_destroy(void *state) {
    struct finder *f = state;
    if (!f) return;
    watch(f, f->desktop, f->path, 0);
    lp_files_list_free(&f->list);
    free(f->sel);
    free(f->vis);
    free(f);
}

static void finder_open(void *state, lp_desktop *d, const char *path) {
    struct finder *f = state;
    int is_dir = 0;
    if (!lp_files_exists(path, &is_dir) || !is_dir) return;
    navigate(f, d, path);
}

void lp_finder_set_preview(void *state) {
    struct finder *f = state;
    if (!f) return;
    f->preview = 1;
    lp_finder_mock_fill(&f->list);
    grow(f);
    clear_selection(f);
    filter(f);
    snprintf(f->path, sizeof f->path, "%s/Repositories", lp_files_home());
    sidebar_build(f);
    /* files.ts: Desktop · Repositories · Documents · Downloads; MaryUI · Rao Cloud */
    f->nside = 0;
    static const struct { const char *label; lp_icon icon; } SIDE[] = {
        { "Desktop", LP_ICON_DESKTOP }, { "Repositories", LP_ICON_FOLDER }, { "Documents", LP_ICON_DOCUMENT }, { "Downloads", LP_ICON_DOWNLOAD },
        { "MaryUI", LP_ICON_DRIVE }, { "Rao Cloud", LP_ICON_CLOUD },
    };
    for (int i = 0; i < 6; i++) {
        struct sidebar_entry *e = &f->side[f->nside++];
        snprintf(e->label, sizeof e->label, "%s", SIDE[i].label);
        snprintf(e->path, sizeof e->path, "%s/%s", lp_files_home(), SIDE[i].label);
        e->icon = SIDE[i].icon;
        e->trash = 0;
    }
    f->nfavorites = 4;
    f->hist_count = 1;
    f->hist_pos = 0;
    snprintf(f->history[0], sizeof f->history[0], "%s", f->path);
}

static void finder_title_of(void *state, char *out, size_t n) {
    struct finder *f = state;
    lp_files_display_name(f->path, out, n);
    f->title_pending = 0;
}

static int finder_notify(void *state, lp_desktop *d, const char *dir) {
    struct finder *f = state;
    if (f->preview || strcmp(dir, f->path) != 0) return 0;
    /* A model callback, not a paint pass: re-read here, so a repaint alone shows the change. */
    reload(f);
    return 1;
}

/* The window's title follows the folder (SET_TITLE is a dispatch: never from open(), never in DRAW). */
static void sync_title(struct finder *f, lp_desktop *d) {
    if (!f->title_pending || !d || f->preview) return;
    char name[LP_FILES_NAME_MAX];
    lp_files_display_name(f->path, name, sizeof name);
    int w = lp_wm_find(&d->wm, f->window_id);
    if (w >= 0 && strcmp(d->wm.windows[w].title, name) != 0) {
        lp_wm_action a = { .type = LP_WM_SET_TITLE, .id = f->window_id, .title = name };
        lp_desktop_dispatch(d, &a);
    }
    f->title_pending = 0;
}

/* MARK: - Operations */

static void changed(lp_desktop *d, const char *dir) { if (d) lp_desktop_files_changed(d, dir); }

static void fail(struct finder *f, const char *what, const char *name, int rc) {
    snprintf(f->status, sizeof f->status, "Could not %s “%s”: %s", what, name, strerror(-rc));
}

static void open_item(struct finder *f, lp_desktop *d, int i) {
    if (i < 0 || i >= f->list.count) return;
    char path[LP_FILES_PATH_MAX];
    item_path(f, i, path, sizeof path);
    if (f->list.entries[i].is_dir) { navigate(f, d, path); return; }
    if (!d || !lp_desktop_open_path(d, path)) snprintf(f->status, sizeof f->status, "No application can open “%s”.", f->list.entries[i].name);
}

static void open_selection(struct finder *f, lp_desktop *d) {
    int n = selected_count(f);
    if (n == 0) return;
    if (n == 1 || f->list.entries[first_selected(f)].is_dir) { open_item(f, d, first_selected(f)); return; }
    for (int i = 0; i < f->list.count; i++) if (f->sel[i] && !f->list.entries[i].is_dir) open_item(f, d, i);
}

static void begin_rename(struct finder *f, int i) {
    if (i < 0 || i >= f->list.count || f->preview) return;
    f->renaming = i;
    lp_text_buffer_set_selected(&f->rename_buf, f->list.entries[i].name);
    f->rename_focus_pending = 1;
    f->reveal = i;
}

static void commit_rename(struct finder *f, lp_desktop *d) {
    int i = f->renaming;
    f->renaming = -1;
    if (i < 0 || i >= f->list.count) return;
    const char *from = f->list.entries[i].name;
    if (strcmp(from, f->rename_buf.text) == 0) return;
    int rc = lp_files_rename(f->path, from, f->rename_buf.text);
    if (rc) { fail(f, "rename", from, rc); return; }
    char kept[LP_FILES_NAME_MAX];
    snprintf(kept, sizeof kept, "%s", f->rename_buf.text);
    reload(f);
    select_only(f, lp_files_find(&f->list, kept));
    f->reveal = f->cursor;
    changed(d, f->path);
}

static void new_folder(struct finder *f, lp_desktop *d) {
    if (f->preview) return;
    char name[LP_FILES_NAME_MAX];
    int rc = lp_files_mkdir_unique(f->path, name, sizeof name);
    if (rc) { fail(f, "create a folder in", lp_files_basename(f->path), rc); return; }
    reload(f);
    select_only(f, lp_files_find(&f->list, name));
    begin_rename(f, f->cursor);
    changed(d, f->path);
}

static void trash_selection(struct finder *f, lp_desktop *d) {
    if (f->preview) return;
    int deleting = in_trash(f);
    char trash[LP_FILES_PATH_MAX];
    lp_files_trash_dir(trash, sizeof trash);
    int done = 0;
    for (int i = 0; i < f->list.count; i++) {
        if (!f->sel[i]) continue;
        char path[LP_FILES_PATH_MAX];
        item_path(f, i, path, sizeof path);
        int rc;
        if (deleting) {
            rc = lp_files_delete_tree(path);
            char info[LP_FILES_PATH_MAX + LP_FILES_NAME_MAX + 16];
            snprintf(info, sizeof info, "%s/info/%s.trashinfo", trash, f->list.entries[i].name);
            unlink(info);
        } else {
            rc = lp_files_trash(path);
        }
        if (rc) { fail(f, deleting ? "delete" : "move to the Trash", f->list.entries[i].name, rc); break; }
        done++;
    }
    if (done) { reload(f); changed(d, f->path); }
}

static void copy_selection(struct finder *f, int cut) {
    int n = selected_count(f);
    if (n == 0) return;
    const char **paths = calloc((size_t)n, sizeof *paths);
    char (*buf)[LP_FILES_PATH_MAX] = calloc((size_t)n, sizeof *buf);
    int k = 0;
    for (int i = 0; i < f->list.count; i++) if (f->sel[i]) { item_path(f, i, buf[k], LP_FILES_PATH_MAX); paths[k] = buf[k]; k++; }
    lp_files_clipboard_set(lp_files_clipboard_shared(), paths, n, cut);
    free(paths);
    free(buf);
    snprintf(f->status, sizeof f->status, "%d item%s %s", n, n == 1 ? "" : "s", cut ? "cut" : "copied");
}

/* Pastes the clipboard into the current folder (moving when `move`, or when the items were cut). */
static void paste(struct finder *f, lp_desktop *d, int move) {
    lp_file_clipboard *c = lp_files_clipboard_shared();
    if (c->count == 0 || f->preview) return;
    move = move || c->cut;
    int done = 0;
    char first[LP_FILES_NAME_MAX] = "";
    for (int i = 0; i < c->count; i++) {
        char name[LP_FILES_NAME_MAX];
        int rc = move ? lp_files_move(c->paths[i], f->path) : lp_files_copy(c->paths[i], f->path, name, sizeof name);
        if (move) snprintf(name, sizeof name, "%s", lp_files_basename(c->paths[i]));
        if (rc) { fail(f, move ? "move" : "copy", lp_files_basename(c->paths[i]), rc); break; }
        if (!first[0]) snprintf(first, sizeof first, "%s", name);
        if (move) { char parent[LP_FILES_PATH_MAX]; lp_files_parent(c->paths[i], parent, sizeof parent); changed(d, parent); }
        done++;
    }
    if (move && done) lp_files_clipboard_clear(c);
    if (done) {
        reload(f);
        select_only(f, lp_files_find(&f->list, first));
        f->reveal = f->cursor;
        changed(d, f->path);
    }
}

static void duplicate_selection(struct finder *f, lp_desktop *d) {
    if (f->preview) return;
    int n = selected_count(f);
    if (n == 0) return;
    char (*made)[LP_FILES_NAME_MAX] = calloc((size_t)n, sizeof *made);
    int k = 0;
    for (int i = 0; i < f->list.count; i++) {
        if (!f->sel[i]) continue;
        char path[LP_FILES_PATH_MAX];
        item_path(f, i, path, sizeof path);
        int rc = lp_files_copy(path, f->path, made[k], LP_FILES_NAME_MAX);
        if (rc) { fail(f, "duplicate", f->list.entries[i].name, rc); break; }
        k++;
    }
    if (k) {
        reload(f);
        clear_selection(f);
        for (int j = 0; j < k; j++) { int i = lp_files_find(&f->list, made[j]); if (i >= 0) { f->sel[i] = 1; if (f->cursor < 0) f->cursor = f->anchor = i; } }
        f->reveal = f->cursor;
        changed(d, f->path);
    }
    free(made);
}

static void show_info(struct finder *f, lp_desktop *d) {
    if (!d || f->preview) return;
    int any = 0;
    for (int i = 0; i < f->list.count; i++) {
        if (!f->sel[i]) continue;
        char path[LP_FILES_PATH_MAX], title[LP_FILES_NAME_MAX + 8];
        item_path(f, i, path, sizeof path);
        snprintf(title, sizeof title, "%s Info", f->list.entries[i].name);
        lp_desktop_open_app_with(d, "info", path, title, NULL);
        any = 1;
    }
    if (!any) {
        char name[LP_FILES_NAME_MAX], title[LP_FILES_NAME_MAX + 8];
        lp_files_display_name(f->path, name, sizeof name);
        snprintf(title, sizeof title, "%s Info", name);
        lp_desktop_open_app_with(d, "info", f->path, title, NULL);
    }
}

static void go_user_dir(struct finder *f, lp_desktop *d, enum lp_user_dir k) {
    char path[LP_FILES_PATH_MAX];
    lp_files_user_dir(k, path, sizeof path);
    if (k == LP_USER_TRASH) lp_files_mkdir_p(path);
    else lp_files_user_dirs_ensure();
    navigate(f, d, path);
}

static void go_up(struct finder *f, lp_desktop *d) {
    char parent[LP_FILES_PATH_MAX], child[LP_FILES_NAME_MAX];
    if (strcmp(f->path, "/") == 0) return;
    snprintf(child, sizeof child, "%s", lp_files_basename(f->path));
    lp_files_parent(f->path, parent, sizeof parent);
    navigate(f, d, parent);
    select_only(f, lp_files_find(&f->list, child));
    f->reveal = f->cursor;
}

static void set_sort(struct finder *f, enum lp_file_sort by) {
    if (f->sort == by) f->sort_desc = !f->sort_desc;
    else { f->sort = by; f->sort_desc = 0; }
    lp_files_sort(&f->list, f->sort, f->sort_desc);
    /* the selection is per entry: rebuild it by name */
    f->stale = 1;
}

/* Drops the drag session's items into `dir` (moving, or copying when asked or across devices). */
static void drop_into(struct finder *f, lp_desktop *d, const char *dir, int trash) {
    lp_drag *g = &d->drag;
    if (!g->active || g->count == 0) return;
    if (!trash && strcmp(dir, g->src_dir) == 0) return;
    int copy = g->copy || (!trash && !lp_files_same_device(g->src_dir, dir));
    int done = 0;
    for (int i = 0; i < g->count; i++) {
        char src[LP_FILES_PATH_MAX];
        lp_files_join(g->src_dir, g->names[i], src, sizeof src);
        int rc = trash ? lp_files_trash(src) : copy ? lp_files_copy(src, dir, NULL, 0) : lp_files_move(src, dir);
        if (rc) { fail(f, trash ? "move to the Trash" : copy ? "copy" : "move", g->names[i], rc); break; }
        done++;
    }
    if (done) {
        changed(d, g->src_dir);
        if (!trash) changed(d, dir);
        if (trash) { char t[LP_FILES_PATH_MAX]; lp_files_user_dir(LP_USER_TRASH, t, sizeof t); changed(d, t); }
    }
}

static void finder_command(void *state, lp_desktop *d, int cmd) {
    struct finder *f = state;
    switch ((enum lp_finder_command)cmd) {
    case LP_FINDER_OPEN: open_selection(f, d); break;
    case LP_FINDER_NEW_FOLDER: new_folder(f, d); break;
    case LP_FINDER_RENAME: if (selected_count(f) == 1) begin_rename(f, first_selected(f)); break;
    case LP_FINDER_DUPLICATE: duplicate_selection(f, d); break;
    case LP_FINDER_TRASH: trash_selection(f, d); break;
    case LP_FINDER_INFO: show_info(f, d); break;
    case LP_FINDER_COPY: copy_selection(f, 0); break;
    case LP_FINDER_CUT: copy_selection(f, 1); break;
    case LP_FINDER_PASTE: paste(f, d, 0); break;
    case LP_FINDER_MOVE_HERE: paste(f, d, 1); break;
    case LP_FINDER_SELECT_ALL: for (int i = 0; i < f->list.count; i++) f->sel[i] = 1; if (f->cursor < 0 && f->list.count) f->cursor = f->anchor = 0; break;
    case LP_FINDER_VIEW_ICONS: f->view = 0; f->scroll.y = 0; break;
    case LP_FINDER_VIEW_LIST: f->view = 1; f->scroll.y = 0; break;
    case LP_FINDER_TOGGLE_HIDDEN: f->hidden = !f->hidden; f->stale = 1; break;
    case LP_FINDER_BACK: go_history(f, d, -1); break;
    case LP_FINDER_FORWARD: go_history(f, d, 1); break;
    case LP_FINDER_UP: go_up(f, d); break;
    case LP_FINDER_GO_HOME: go_user_dir(f, d, LP_USER_HOME); break;
    case LP_FINDER_GO_DESKTOP: go_user_dir(f, d, LP_USER_DESKTOP); break;
    case LP_FINDER_GO_DOCUMENTS: go_user_dir(f, d, LP_USER_DOCUMENTS); break;
    case LP_FINDER_GO_DOWNLOADS: go_user_dir(f, d, LP_USER_DOWNLOADS); break;
    case LP_FINDER_GO_TRASH: go_user_dir(f, d, LP_USER_TRASH); break;
    case LP_FINDER_EMPTY_TRASH: {
        int rc = lp_files_empty_trash();
        if (rc) fail(f, "empty", "Trash", rc);
        char t[LP_FILES_PATH_MAX];
        lp_files_user_dir(LP_USER_TRASH, t, sizeof t);
        changed(d, t);
        if (in_trash(f)) reload(f);
        break;
    }
    case LP_FINDER_SORT_NAME: set_sort(f, LP_FILE_SORT_NAME); break;
    case LP_FINDER_SORT_DATE: set_sort(f, LP_FILE_SORT_DATE); break;
    case LP_FINDER_SORT_SIZE: set_sort(f, LP_FILE_SORT_SIZE); break;
    case LP_FINDER_SORT_KIND: set_sort(f, LP_FILE_SORT_KIND); break;
    }
    if (f->stale) reload(f);
    sync_title(f, d);
}

/* MARK: - Menus */

static lp_menu_entry *entry(lp_menu_model *m, const char *label, const char *shortcut, int cmd, int disabled) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return &m->entries[LP_MENU_MAX_ENTRIES - 1];
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "%s", label);
    e->shortcut = shortcut;
    e->command = LP_CMD_APP;
    e->arg = cmd;
    e->disabled = disabled;
    return e;
}
static void separator(lp_menu_model *m) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    memset(&m->entries[m->count], 0, sizeof m->entries[0]);
    m->entries[m->count++].separator = 1;
}

static void finder_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    struct finder *f = state;
    int n = selected_count(f), trash = in_trash(f);
    switch (menu) {
    case LP_MENU_FILE:
        entry(m, "Open", "⌘O", LP_FINDER_OPEN, n == 0);
        separator(m);
        entry(m, "New Folder", "⇧⌘N", LP_FINDER_NEW_FOLDER, trash);
        entry(m, "Get Info", "⌘I", LP_FINDER_INFO, 0);
        entry(m, "Rename", NULL, LP_FINDER_RENAME, n != 1);
        entry(m, "Duplicate", "⌘D", LP_FINDER_DUPLICATE, n == 0 || trash);
        separator(m);
        entry(m, trash ? "Delete Immediately" : "Move to Trash", "⌘⌫", LP_FINDER_TRASH, n == 0);
        entry(m, "Empty Trash…", NULL, LP_FINDER_EMPTY_TRASH, 0);
        break;
    case LP_MENU_EDIT: {
        lp_file_clipboard *c = lp_files_clipboard_shared();
        entry(m, "Undo", "⌘Z", LP_FINDER_OPEN, 1)->command = LP_CMD_NONE;
        entry(m, "Redo", "⇧⌘Z", LP_FINDER_OPEN, 1)->command = LP_CMD_NONE;
        separator(m);
        entry(m, "Cut", "⌘X", LP_FINDER_CUT, n == 0);
        entry(m, "Copy", "⌘C", LP_FINDER_COPY, n == 0);
        entry(m, "Paste", "⌘V", LP_FINDER_PASTE, c->count == 0 || trash);
        entry(m, "Move Items Here", "⌥⌘V", LP_FINDER_MOVE_HERE, c->count == 0 || trash);
        separator(m);
        entry(m, "Select All", "⌘A", LP_FINDER_SELECT_ALL, f->list.count == 0);
        break;
    }
    case LP_MENU_VIEW:
        entry(m, "as Icons", "⌘1", LP_FINDER_VIEW_ICONS, 0)->checked = f->view == 0;
        entry(m, "as List", "⌘2", LP_FINDER_VIEW_LIST, 0)->checked = f->view == 1;
        separator(m);
        entry(m, "Sort by Name", NULL, LP_FINDER_SORT_NAME, 0)->checked = f->sort == LP_FILE_SORT_NAME;
        entry(m, "Sort by Date Modified", NULL, LP_FINDER_SORT_DATE, 0)->checked = f->sort == LP_FILE_SORT_DATE;
        entry(m, "Sort by Size", NULL, LP_FINDER_SORT_SIZE, 0)->checked = f->sort == LP_FILE_SORT_SIZE;
        entry(m, "Sort by Kind", NULL, LP_FINDER_SORT_KIND, 0)->checked = f->sort == LP_FILE_SORT_KIND;
        separator(m);
        entry(m, "Show Hidden Files", "⇧⌘.", LP_FINDER_TOGGLE_HIDDEN, 0)->checked = f->hidden;
        break;
    case LP_MENU_GO:
        entry(m, "Back", "⌘[", LP_FINDER_BACK, f->hist_pos <= 0);
        entry(m, "Forward", "⌘]", LP_FINDER_FORWARD, f->hist_pos >= f->hist_count - 1);
        entry(m, "Enclosing Folder", "⌘↑", LP_FINDER_UP, strcmp(f->path, "/") == 0);
        separator(m);
        entry(m, lp_files_user_dir_label(LP_USER_HOME), "⇧⌘H", LP_FINDER_GO_HOME, 0);
        entry(m, "Desktop", "⇧⌘D", LP_FINDER_GO_DESKTOP, 0);
        entry(m, "Documents", "⇧⌘O", LP_FINDER_GO_DOCUMENTS, 0);
        entry(m, "Downloads", "⌥⌘L", LP_FINDER_GO_DOWNLOADS, 0);
        separator(m);
        entry(m, "Trash", NULL, LP_FINDER_GO_TRASH, 0);
        break;
    default: break;
    }
}

static void open_popup(struct finder *f, lp_desktop *d, float x, float y) {
    if (!d) return;
    lp_menu_model m = { .id = "popup", .label = "", .count = 0 };
    int n = selected_count(f), trash = in_trash(f);
    if (n > 0) {
        entry(&m, "Open", NULL, LP_FINDER_OPEN, 0);
        entry(&m, "Get Info", NULL, LP_FINDER_INFO, 0);
        separator(&m);
        entry(&m, "Rename", NULL, LP_FINDER_RENAME, n != 1);
        entry(&m, "Duplicate", NULL, LP_FINDER_DUPLICATE, trash);
        entry(&m, trash ? "Delete Immediately" : "Move to Trash", NULL, LP_FINDER_TRASH, 0);
        separator(&m);
        entry(&m, "Cut", NULL, LP_FINDER_CUT, 0);
        entry(&m, "Copy", NULL, LP_FINDER_COPY, 0);
    } else {
        lp_file_clipboard *c = lp_files_clipboard_shared();
        entry(&m, "New Folder", NULL, LP_FINDER_NEW_FOLDER, trash);
        entry(&m, "Paste", NULL, LP_FINDER_PASTE, c->count == 0 || trash);
        entry(&m, "Get Info", NULL, LP_FINDER_INFO, 0);
        separator(&m);
        entry(&m, "as Icons", NULL, LP_FINDER_VIEW_ICONS, 0)->checked = f->view == 0;
        entry(&m, "as List", NULL, LP_FINDER_VIEW_LIST, 0)->checked = f->view == 1;
        entry(&m, "Show Hidden Files", NULL, LP_FINDER_TOGGLE_HIDDEN, 0)->checked = f->hidden;
        if (trash) { separator(&m); entry(&m, "Empty Trash…", NULL, LP_FINDER_EMPTY_TRASH, f->list.count == 0); }
    }
    lp_desktop_open_popup(d, f->window_id, x, y, &m);
}

/* MARK: - Keys */

static int matches(const lp_file_entry *e, const char *q) {
    if (!*q) return 1;
    char a[LP_FILES_NAME_MAX], b[256];
    size_t i;
    for (i = 0; e->name[i] && i < sizeof a - 1; i++) a[i] = (char)tolower((unsigned char)e->name[i]);
    a[i] = 0;
    for (i = 0; q[i] && i < sizeof b - 1; i++) b[i] = (char)tolower((unsigned char)q[i]);
    b[i] = 0;
    return strstr(a, b) != NULL;
}

/* The visible position of an entry, -1 when the search hides it. */
static int vis_index(const struct finder *f, int entry_i) {
    for (int v = 0; v < f->nvis; v++) if (f->vis[v] == entry_i) return v;
    return -1;
}

static void move_cursor(struct finder *f, int v, int extend) {
    if (v < 0) v = 0;
    if (v >= f->nvis) v = f->nvis - 1;
    if (v < 0) return;
    int i = f->vis[v];
    if (extend && f->anchor >= 0) {
        int a = vis_index(f, f->anchor);
        if (a < 0) a = v;
        clear_selection(f);
        for (int k = a < v ? a : v; k <= (a < v ? v : a); k++) f->sel[f->vis[k]] = 1;
        f->cursor = i;
    } else {
        select_only(f, i);
    }
    f->reveal = i;
}

static void type_ahead(struct finder *f, const char *utf8, double now_ms) {
    if (now_ms - f->typed_ms > TYPEAHEAD_MS) f->typed[0] = 0;
    f->typed_ms = now_ms;
    size_t len = strlen(f->typed), add = strlen(utf8);
    if (len + add < sizeof f->typed) { memcpy(f->typed + len, utf8, add + 1); }
    size_t n = strlen(f->typed);
    for (int v = 0; v < f->nvis; v++) {
        const char *name = f->list.entries[f->vis[v]].name;
        if (strncasecmp(name, f->typed, n) == 0) { move_cursor(f, v, 0); return; }
    }
}

/* Returns 1 when the key was the Finder's. */
static int handle_key(struct finder *f, lp_desktop *d, lp_ctx *ctx, int cols) {
    const lp_input *in = &ctx->in;
    if (!in->key_pressed || !in->keysym) return 0;
    uint32_t sym = in->keysym;
    int cmd = (in->mods & (LP_MOD_CTRL | LP_MOD_LOGO)) != 0, shift = (in->mods & LP_MOD_SHIFT) != 0, alt = (in->mods & LP_MOD_ALT) != 0;
    if (f->renaming >= 0) {
        if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) { commit_rename(f, d); ctx->focus = 0; return 1; }
        if (sym == XKB_KEY_Escape) { f->renaming = -1; ctx->focus = 0; return 1; }
        return 0; /* the field edits */
    }
    if (cmd) {
        switch (sym) {
        case XKB_KEY_o: case XKB_KEY_O:
            if (shift) go_user_dir(f, d, LP_USER_DOCUMENTS); else open_selection(f, d);
            return 1;
        case XKB_KEY_Down: open_selection(f, d); return 1;
        case XKB_KEY_Up: go_up(f, d); return 1;
        case XKB_KEY_bracketleft: go_history(f, d, -1); return 1;
        case XKB_KEY_bracketright: go_history(f, d, 1); return 1;
        case XKB_KEY_a: case XKB_KEY_A: finder_command(f, d, LP_FINDER_SELECT_ALL); return 1;
        case XKB_KEY_c: case XKB_KEY_C: copy_selection(f, 0); return 1;
        case XKB_KEY_x: case XKB_KEY_X: copy_selection(f, 1); return 1;
        case XKB_KEY_v: case XKB_KEY_V: paste(f, d, alt); return 1;
        case XKB_KEY_d: case XKB_KEY_D: if (shift) go_user_dir(f, d, LP_USER_DESKTOP); else duplicate_selection(f, d); return 1;
        case XKB_KEY_i: case XKB_KEY_I: show_info(f, d); return 1;
        case XKB_KEY_n: case XKB_KEY_N: if (shift) { new_folder(f, d); return 1; } return 0;
        case XKB_KEY_h: case XKB_KEY_H: if (shift) { go_user_dir(f, d, LP_USER_HOME); return 1; } return 0;
        case XKB_KEY_l: case XKB_KEY_L: if (alt) { go_user_dir(f, d, LP_USER_DOWNLOADS); return 1; } return 0;
        case XKB_KEY_period: case XKB_KEY_greater: if (shift) { finder_command(f, d, LP_FINDER_TOGGLE_HIDDEN); return 1; } return 0;
        case XKB_KEY_1: finder_command(f, d, LP_FINDER_VIEW_ICONS); return 1;
        case XKB_KEY_2: finder_command(f, d, LP_FINDER_VIEW_LIST); return 1;
        case XKB_KEY_BackSpace: case XKB_KEY_Delete: trash_selection(f, d); return 1;
        default: return 0;
        }
    }
    int cur = f->cursor >= 0 ? vis_index(f, f->cursor) : -1;
    int step = f->view == 0 ? cols : 1;
    switch (sym) {
    case XKB_KEY_Down: move_cursor(f, cur < 0 ? 0 : cur + step, shift); return 1;
    case XKB_KEY_Up: move_cursor(f, cur < 0 ? 0 : cur - step, shift); return 1;
    case XKB_KEY_Right: if (f->view == 0) { move_cursor(f, cur < 0 ? 0 : cur + 1, shift); return 1; } return 0;
    case XKB_KEY_Left: if (f->view == 0) { move_cursor(f, cur < 0 ? 0 : cur - 1, shift); return 1; } return 0;
    case XKB_KEY_Home: move_cursor(f, 0, shift); return 1;
    case XKB_KEY_End: move_cursor(f, f->nvis - 1, shift); return 1;
    case XKB_KEY_Return: case XKB_KEY_KP_Enter: if (selected_count(f) == 1) begin_rename(f, first_selected(f)); return 1;
    case XKB_KEY_Escape: clear_selection(f); f->cursor = -1; f->status[0] = 0; return 1;
    case XKB_KEY_space: return 1;
    default: break;
    }
    if (in->utf8[0] && (unsigned char)in->utf8[0] >= 0x20 && in->utf8[0] != 0x7f) { type_ahead(f, in->utf8, ctx->now_ms); return 1; }
    return 0;
}

/* MARK: - Paint */

static void status_bar(struct finder *f, lp_ctx *ctx, lp_rect status) {
    cairo_t *cr = ctx->cr;
    lp_fill_vgradient(cr, status, LP_PLATINUM_2, LP_PLATINUM_3, 0);
    static const lp_shadow_layer top[] = { { 1, 0, 1, 0, 0, { 1, 1, 1, 0.78f } } };
    lp_draw_inset_shadows(cr, status, 0, top, 1);
    lp_draw_hairline(cr, status, LP_EDGE_TOP, LP_EDGE_DIVIDER);
    char text[LP_FILES_NAME_MAX + 200], avail[32];
    lp_files_format_size((int64_t)f->list.free_bytes, 0, avail, sizeof avail);
    int n = selected_count(f);
    if (f->status[0]) snprintf(text, sizeof text, "%s", f->status);
    else if (n == 1) snprintf(text, sizeof text, "“%s” selected · %d item%s, %s available", f->list.entries[first_selected(f)].name, f->nvis, f->nvis == 1 ? "" : "s", avail);
    else if (n > 1) snprintf(text, sizeof text, "%d of %d selected, %s available", n, f->nvis, avail);
    else snprintf(text, sizeof text, "%d item%s, %s available", f->nvis, f->nvis == 1 ? "" : "s", avail);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS; st.color = f->status[0] ? LP_INK_PRIMARY : LP_INK_SECONDARY; st.ellipsize = 1;
    lp_text_draw(cr, text, lp_rect_inset(status, LP_SPACE_3, 0), &st, LP_ALIGN_CENTER);
}

/* The crumbs: laid out (and remembered for hit-testing) in the DRAW pass; clicks use the remembered rects. */
static void path_bar(struct finder *f, lp_desktop *d, lp_ctx *ctx, lp_rect bar, lp_id base) {
    if (ctx->pass == LP_PASS_EVENT) {
        for (int i = 0; i < f->ncrumbs; i++) {
            if (lp_clicked(ctx, lp_id_index(base, 300 + i), f->crumbs[i]) && i < f->ncrumbs - 1) { navigate(f, d, f->crumb_paths[i]); ctx->dirty = 1; }
        }
        return;
    }
    if (!ctx->cr) return;
    cairo_t *cr = ctx->cr;
    lp_fill_solid(cr, bar, LP_PLATINUM_1, 0);
    lp_draw_hairline(cr, bar, LP_EDGE_TOP, LP_EDGE_DIVIDER);
    /* components from the root */
    const char *parts[MAX_CRUMBS];
    int n = 0;
    char copy[LP_FILES_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", f->path);
    parts[n++] = "/";
    for (char *p = strtok(copy, "/"); p && n < MAX_CRUMBS; p = strtok(NULL, "/")) parts[n++] = p;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS; st.color = LP_INK_SECONDARY; st.ellipsize = 1;
    lp_text_style sep = st;
    sep.color = LP_INK_TERTIARY;
    float x = bar.x + LP_SPACE_2;
    char path[LP_FILES_PATH_MAX] = "";
    f->ncrumbs = 0;
    lp_accent accent = lp_settings_accent(ctx->settings);
    for (int i = 0; i < n; i++) {
        if (i == 0) snprintf(path, sizeof path, "/");
        else { size_t l = strlen(path); snprintf(path + l, sizeof path - l, "%s%s", l > 1 ? "/" : "", parts[i]); }
        char name[LP_FILES_NAME_MAX];
        lp_files_display_name(path, name, sizeof name);
        lp_size ts = lp_text_measure(cr, name, &st);
        float w = ts.w + 2 * LP_SPACE_1;
        if (x + w > bar.x + bar.w - LP_SPACE_2) break;
        lp_rect r = LP_RECT(x, bar.y + 2, w, bar.h - 4);
        lp_id id = lp_id_index(base, 300 + f->ncrumbs);
        int last = i == n - 1;
        if (lp_is_hot(ctx, id) && !last) lp_fill_solid(cr, r, LP_RGBA(0, 0, 0, 0.06f), LP_RADIUS_XS);
        lp_text_style cs = st;
        if (last) { cs.color = LP_INK_PRIMARY; cs.weight = LP_TEXT_WEIGHT_MEDIUM; }
        if (f->drop_kind == DROP_BODY && last) cs.color = accent.base;
        lp_text_draw(cr, name, r, &cs, LP_ALIGN_CENTER);
        f->crumbs[f->ncrumbs] = r;
        snprintf(f->crumb_paths[f->ncrumbs], LP_FILES_PATH_MAX, "%s", path);
        f->ncrumbs++;
        x += w;
        if (!last) { lp_text_draw(cr, "›", LP_RECT(x, bar.y, 12, bar.h), &sep, LP_ALIGN_CENTER); x += 12; }
    }
}

static void drop_ring(cairo_t *cr, lp_rect r, float radius, lp_accent accent) {
    lp_fill_solid(cr, r, lp_color_with_alpha(accent.base, 0.12f), radius);
    lp_draw_focus_ring(cr, lp_rect_inset(r, 2, 2), radius, accent.base, 2);
}

static void finder_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct finder *preview;
    struct finder *f = state;
    if (!f) {
        if (!preview) { preview = finder_create(NULL, "preview"); lp_finder_set_preview(preview); }
        f = preview;
    }
    cairo_t *cr = ctx->cr;
    int event = ctx->pass == LP_PASS_EVENT, draw = ctx->pass == LP_PASS_DRAW && cr;
    lp_id base = LP_ID("finder"), search_id = lp_id_index(base, 4), rename_id = LP_ID("finder.rename"), items_id = LP_ID("finder.item");
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_input *in = &ctx->in;

    if (f->stale && !draw) reload(f);
    if (f->rename_focus_pending) { ctx->focus = rename_id; f->rename_focus_pending = 0; }
    if (event && state) sync_title(f, d);

    /* MARK: layout */
    lp_rect area = body;
    lp_rect bar = lp_toolbar(ctx, &area);
    lp_rect status = lp_rect_cut_bottom(&area, STATUS_H);
    lp_rect pathbar = lp_rect_cut_bottom(&area, PATHBAR_H);
    lp_rect col = lp_sidebar(ctx, &area);
    lp_rect content = area;

    grow(f);
    filter(f);

    float pad = LP_SPACE_3, gap = LP_SPACE_2;
    int cols = (int)fmaxf(1, floorf((content.w - 2 * pad + gap) / (TILE_MIN_W + gap)));
    float tile_w = (content.w - 2 * pad - gap * (cols - 1)) / cols;
    lp_size extent = f->view == 0
        ? (lp_size){ content.w, pad * 2 + ((f->nvis + cols - 1) / cols) * (TILE_H + gap) }
        : (lp_size){ content.w, LP_LIST_HEADER_H + f->nvis * LP_LIST_ROW_H };

    /* the rect of visible item v in content (scrolled) coordinates, given the content origin c */
    #define ITEM_RECT(v, c) (f->view == 0 \
        ? LP_RECT((c).x + pad + ((v) % cols) * (tile_w + gap), (c).y + pad + ((v) / cols) * (TILE_H + gap), tile_w, TILE_H) \
        : LP_RECT((c).x, (c).y + LP_LIST_HEADER_H + (v) * LP_LIST_ROW_H, (c).w, LP_LIST_ROW_H))

    /* scroll an entry into view before anything is hit-tested */
    if (f->reveal >= 0) {
        int v = vis_index(f, f->reveal);
        f->reveal = -1;
        if (v >= 0) {
            lp_rect r = ITEM_RECT(v, LP_RECT(0, 0, content.w, 0));
            float top = r.y, bottom = r.y + r.h, view_h = content.h - (f->view == 1 ? LP_LIST_HEADER_H : 0);
            if (f->view == 1) { top -= LP_LIST_HEADER_H; bottom -= LP_LIST_HEADER_H; }
            if (top < f->scroll.y) f->scroll.y = top;
            else if (bottom > f->scroll.y + view_h) f->scroll.y = bottom - view_h;
            if (f->scroll.y < 0) f->scroll.y = 0;
        }
    }

    /* MARK: keys (the search field, when focused, takes them) */
    if (event && in->keysym && ctx->focus != search_id && (f->renaming >= 0 ? ctx->focus == rename_id : 1)) {
        if (handle_key(f, d, ctx, cols)) { ctx->dirty = 1; if (f->stale) reload(f); }
    }

    /* MARK: the drag session over this window */
    lp_drag *drag = d ? &d->drag : NULL;
    int dragging_here = event && in->drag && drag && drag->active;
    enum drop_kind new_kind = DROP_NONE;
    int new_index = -1;
    char new_dir[LP_FILES_PATH_MAX] = "";

    /* MARK: toolbar: Back · Forward · view · spacer · search */
    float x = bar.x, cy = bar.y + bar.h / 2;
    if (lp_button(ctx, lp_id_index(base, 1), LP_RECT(x, cy - 9, 18, 18), "Back",
            (lp_button_opts){ .variant = LP_BUTTON_QUIET, .size = LP_CONTROL_SM, .icon = LP_ICON_CHEVRON_LEFT, .icon_only = 1, .disabled = f->hist_pos <= 0 })) { go_history(f, d, -1); ctx->dirty = 1; }
    x += 18 + LP_SPACE_1;
    if (lp_button(ctx, lp_id_index(base, 2), LP_RECT(x, cy - 9, 18, 18), "Forward",
            (lp_button_opts){ .variant = LP_BUTTON_QUIET, .size = LP_CONTROL_SM, .icon = LP_ICON_CHEVRON_RIGHT, .icon_only = 1, .disabled = f->hist_pos >= f->hist_count - 1 })) { go_history(f, d, 1); ctx->dirty = 1; }
    x += 18 + LP_SPACE_2;
    static const lp_segment views[2] = { { NULL, LP_ICON_GRID }, { NULL, LP_ICON_LIST } };
    lp_size seg = lp_segmented_measure(ctx, views, 2, LP_CONTROL_SM);
    if (lp_segmented(ctx, lp_id_index(base, 3), x, cy - seg.h / 2, views, 2, &f->view, LP_CONTROL_SM)) f->scroll.y = 0;
    lp_rect search = LP_RECT(bar.x + bar.w - 180, cy - LP_SIZE_CONTROL_HEIGHT / 2, 180, LP_SIZE_CONTROL_HEIGHT);
    if (lp_text_field(ctx, search_id, search, &f->query, (lp_text_field_opts){ .placeholder = "Search", .icon = LP_ICON_SEARCH, .round = 1 })) { f->scroll.y = 0; ctx->dirty = 1; }

    /* MARK: sidebar */
    lp_sidebar_section(ctx, &col, "Favorites");
    for (int i = 0; i < f->nside; i++) {
        if (i == f->nfavorites) { col.y += LP_SPACE_3; lp_sidebar_section(ctx, &col, "Locations"); }
        struct sidebar_entry *e = &f->side[i];
        lp_rect r = LP_RECT(col.x, col.y, col.w, 24);
        int selected = strcmp(e->path, f->path) == 0;
        if (dragging_here && lp_hit(ctx, r) && (e->trash || strcmp(e->path, drag->src_dir) != 0)) { new_kind = DROP_SIDEBAR; new_index = i; snprintf(new_dir, sizeof new_dir, "%s", e->path); }
        if (draw && f->drop_kind == DROP_SIDEBAR && f->drop_index == i) drop_ring(cr, r, LP_RADIUS_SM, accent);
        if (lp_sidebar_item(ctx, lp_id_index(base, 10 + i), &col, e->icon, e->label, selected) && !in->drag) {
            if (e->trash) lp_files_mkdir_p(e->path);
            else if (i < f->nfavorites) lp_files_user_dirs_ensure();
            navigate(f, d, e->path);
            ctx->dirty = 1;
        }
    }

    /* MARK: content */
    if (draw) lp_fill_solid(cr, content, LP_SURFACE_BODY, 0);
    int over_content = lp_hit(ctx, content);
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 5), content, extent, &f->scroll);
    if (f->view == 1) {
        static const char *const HEADER[] = { "Name", "Date Modified", "Size", "Kind" };
        int sort_col = f->sort == LP_FILE_SORT_NAME ? 0 : f->sort == LP_FILE_SORT_DATE ? 1 : f->sort == LP_FILE_SORT_SIZE ? 2 : 3;
        lp_rect hr = LP_RECT(c.x, content.y, c.w, LP_LIST_HEADER_H); /* the header stays put */
        if (draw) { cairo_save(cr); }
        int clicked = lp_list_header(ctx, lp_id_index(base, 6), hr, HEADER, 4, sort_col, f->sort_desc);
        if (draw) cairo_restore(cr);
        if (clicked >= 0 && !in->drag) { set_sort(f, clicked == 0 ? LP_FILE_SORT_NAME : clicked == 1 ? LP_FILE_SORT_DATE : clicked == 2 ? LP_FILE_SORT_SIZE : LP_FILE_SORT_KIND); reload(f); ctx->dirty = 1; }
        c.y += 0; /* rows start after the header, inside the scrolled content (ITEM_RECT adds LP_LIST_HEADER_H) */
    }
    lp_rect viewport = f->view == 1 ? LP_RECT(content.x, content.y + LP_LIST_HEADER_H, content.w, content.h - LP_LIST_HEADER_H) : content;
    int hit_item = -1;          /* the visible item under the pointer this event */
    time_t now = time(NULL);
    for (int v = 0; v < f->nvis; v++) {
        int i = f->vis[v];
        lp_file_entry *e = &f->list.entries[i];
        lp_rect r = ITEM_RECT(v, c);
        if (r.y + r.h < viewport.y || r.y > viewport.y + viewport.h) continue; /* culled */
        lp_id id = lp_id_index(items_id, (int)(lp_id_hash(e->name) & 0x7fffffff));
        int hot = lp_hit(ctx, viewport) && lp_hot(ctx, id, r);
        if (event && hot && !in->drag) hit_item = v;
        if (dragging_here && hot && e->is_dir && !(strcmp(f->path, drag->src_dir) == 0 && vis_index(f, i) >= 0 && f->sel[i])) {
            new_kind = DROP_ITEM; new_index = i; item_path(f, i, new_dir, sizeof new_dir);
        }
        if (!draw) continue;
        int is_drop = f->drop_kind == DROP_ITEM && f->drop_index == i;
        if (f->view == 0) {
            if (f->sel[i]) {
                lp_fill_solid(cr, r, accent.soft, LP_RADIUS_SM);
                static lp_shadow_layer ring[1] = { { 1, 0, 0, 0, 1, { 0, 0, 0, 1 } } };
                ring[0].color = accent.light;
                lp_draw_inset_shadows(cr, r, LP_RADIUS_SM, ring, 1);
            } else if (lp_is_hot(ctx, id)) {
                lp_fill_solid(cr, r, LP_RGBA(0, 0, 0, 0.04f), LP_RADIUS_SM);
            }
            if (is_drop) drop_ring(cr, r, LP_RADIUS_SM, accent);
            lp_rect ic = LP_RECT(r.x + (r.w - 52) / 2, r.y + LP_SPACE_2, 52, 52);
            lp_file_icon_paint(cr, ic, lp_files_kind_icon(e->kind), e->is_dir, ctx->settings);
            if (e->is_link) lp_icon_draw(cr, LP_ICON_CHEVRON_RIGHT, ic.x + 2, ic.y + ic.h - 16, 14, 2.2f, LP_INK_SECONDARY);
            lp_rect label = LP_RECT(r.x + 2, ic.y + 52 + LP_SPACE_1, r.w - 4, 16);
            if (f->renaming != i) {
                lp_text_style st = lp_text_style_default();
                st.size_px = LP_TEXT_SM; st.ellipsize = 1;
                if (e->is_hidden) st.color = LP_INK_TERTIARY;
                lp_text_draw(cr, e->name, label, &st, LP_ALIGN_CENTER);
            }
        } else {
            char size[32], date[40];
            lp_files_format_size(e->size, e->is_dir, size, sizeof size);
            lp_files_format_date(e->mtime, now, date, sizeof date);
            const char *cols_[3] = { date, size, lp_files_kind_label(e->kind) };
            lp_list_row(ctx, id, r, lp_files_kind_icon(e->kind), f->renaming == i ? "" : e->name, cols_, 3, f->sel[i], (v % 2) == 1);
            if (is_drop) drop_ring(cr, r, 0, accent);
        }
    }
    /* the rename field, over the name */
    if (f->renaming >= 0 && f->renaming < f->list.count) {
        int v = vis_index(f, f->renaming);
        if (v >= 0) {
            lp_rect r = ITEM_RECT(v, c);
            lp_rect field = f->view == 0 ? LP_RECT(r.x + 2, r.y + LP_SPACE_2 + 52 + 2, r.w - 4, 20) : LP_RECT(r.x + LP_SPACE_3 + 14 + LP_SPACE_2 - 4, r.y, 220, r.h);
            if (event && (in->pressed & LP_BUTTON_LEFT) && !lp_hit(ctx, field)) { commit_rename(f, d); ctx->focus = 0; ctx->dirty = 1; }
            else lp_text_field(ctx, rename_id, field, &f->rename_buf, (lp_text_field_opts){ .placeholder = "Name", .icon = LP_ICON_COUNT });
        }
    }
    /* the marquee */
    if (draw && f->marquee) {
        float x0 = fminf(f->mq_x0, f->mq_x1), y0 = fminf(f->mq_y0, f->mq_y1);
        lp_rect mq = LP_RECT(c.x + x0, c.y + y0, fabsf(f->mq_x1 - f->mq_x0), fabsf(f->mq_y1 - f->mq_y0));
        lp_fill_solid(cr, mq, lp_color_with_alpha(accent.base, 0.14f), 0);
        lp_draw_focus_ring(cr, lp_rect_inset(mq, 1, 1), 0, lp_color_with_alpha(accent.base, 0.6f), 1);
    }
    lp_scroll_end(ctx);
    if (draw && f->nvis == 0) {
        lp_text_style st = lp_text_style_default();
        st.color = LP_INK_TERTIARY;
        char msg[300];
        if (f->query.len) snprintf(msg, sizeof msg, "No items match “%s”.", f->query.text);
        else if (f->list.err) snprintf(msg, sizeof msg, "%s", strerror(f->list.err));
        else snprintf(msg, sizeof msg, "No items");
        lp_text_draw(cr, msg, LP_RECT(content.x, content.y + LP_SPACE_6, content.w, 20), &st, LP_ALIGN_CENTER);
    }
    if (draw && f->drop_kind == DROP_BODY) drop_ring(cr, lp_rect_inset(content, 2, 2), LP_RADIUS_SM, accent);

    /* MARK: the pointer gesture (EVENT) */
    if (event && !in->drag) {
        float cx = in->mx - c.x, cy2 = in->my - c.y; /* content coordinates */
        if (in->pressed & LP_BUTTON_LEFT) {
            f->status[0] = 0;
            if (hit_item >= 0) {
                int i = f->vis[hit_item];
                if (ctx->focus == search_id) ctx->focus = 0;
                if (f->renaming == i) { /* the field handles it */ }
                else if (in->double_click) { select_only(f, i); open_item(f, d, i); f->press_item = -1; }
                else {
                    int cmdk = (in->mods & (LP_MOD_CTRL | LP_MOD_LOGO)) != 0, shift = (in->mods & LP_MOD_SHIFT) != 0;
                    f->press_was_selected = f->sel[i];
                    if (cmdk) { f->sel[i] = !f->sel[i]; f->cursor = i; if (f->sel[i]) f->anchor = i; }
                    else if (shift && f->anchor >= 0) move_cursor(f, hit_item, 1);
                    else if (!f->sel[i]) select_only(f, i);
                    else f->cursor = i;
                    f->press_item = i;
                    f->press_x = in->mx;
                    f->press_y = in->my;
                    f->reveal = -1;
                }
                ctx->dirty = 1;
            } else if (over_content && lp_hit(ctx, viewport)) {
                if (ctx->focus == search_id) ctx->focus = 0;
                if (!(in->mods & (LP_MOD_CTRL | LP_MOD_LOGO | LP_MOD_SHIFT))) { clear_selection(f); f->cursor = -1; }
                f->marquee = 1;
                f->mq_x0 = f->mq_x1 = cx;
                f->mq_y0 = f->mq_y1 = cy2;
                f->press_item = -1;
                ctx->dirty = 1;
            }
        }
        if (in->pressed & LP_BUTTON_RIGHT) {
            if (hit_item >= 0) { int i = f->vis[hit_item]; if (!f->sel[i]) select_only(f, i); }
            else if (over_content) { clear_selection(f); f->cursor = -1; }
            if (hit_item >= 0 || over_content) { open_popup(f, d, in->mx, in->my); ctx->dirty = 1; }
            f->press_item = -1;
            f->marquee = 0;
        }
        if ((in->buttons & LP_BUTTON_LEFT) && f->press_item >= 0 && d && !d->drag.active) {
            if (hypotf(in->mx - f->press_x, in->my - f->press_y) > DRAG_THRESHOLD && !f->preview) {
                int n = selected_count(f);
                if (n == 0) { select_only(f, f->press_item); n = 1; }
                const char **names = calloc((size_t)n, sizeof *names);
                int k = 0, first = -1;
                for (int i = 0; i < f->list.count; i++) if (f->sel[i]) { names[k++] = f->list.entries[i].name; if (first < 0) first = i; }
                lp_desktop_drag_begin(d, f->window_id, f->path, names, n, lp_files_kind_icon(f->list.entries[first].kind), f->list.entries[first].is_dir);
                free(names);
                f->press_item = -1;
                ctx->dirty = 1;
            }
        }
        if ((in->buttons & LP_BUTTON_LEFT) && f->marquee) {
            f->mq_x1 = cx;
            f->mq_y1 = cy2;
            float x0 = fminf(f->mq_x0, f->mq_x1), y0 = fminf(f->mq_y0, f->mq_y1), x1 = fmaxf(f->mq_x0, f->mq_x1), y1 = fmaxf(f->mq_y0, f->mq_y1);
            for (int v = 0; v < f->nvis; v++) {
                lp_rect r = ITEM_RECT(v, LP_RECT(0, 0, content.w, 0));
                int inside = r.x < x1 && r.x + r.w > x0 && r.y < y1 && r.y + r.h > y0;
                f->sel[f->vis[v]] = inside;
            }
            lp_damage(ctx, content);
        }
        if (in->released & LP_BUTTON_LEFT) {
            if (f->press_item >= 0 && f->press_was_selected && !(in->mods & (LP_MOD_CTRL | LP_MOD_LOGO | LP_MOD_SHIFT))) select_only(f, f->press_item);
            f->press_item = -1;
            if (f->marquee) { f->marquee = 0; lp_damage(ctx, content); }
        }
    }
    /* the drop target and the drop itself */
    if (dragging_here) {
        if (new_kind == DROP_NONE && over_content && strcmp(f->path, drag->src_dir) != 0 && !in_trash(f)) { new_kind = DROP_BODY; snprintf(new_dir, sizeof new_dir, "%s", f->path); }
        if (in->drag == LP_DRAG_LEAVE) new_kind = DROP_NONE;
        if (new_kind != f->drop_kind || new_index != f->drop_index) {
            f->drop_kind = new_kind;
            f->drop_index = new_index;
            snprintf(f->drop_dir, sizeof f->drop_dir, "%s", new_dir);
            ctx->dirty = 1;
        }
        if (new_kind != DROP_NONE) {
            int alt = (in->mods & LP_MOD_ALT) != 0;
            int trash = new_kind == DROP_SIDEBAR && f->side[new_index].trash;
            drag->copy = alt || (!trash && !lp_files_same_device(drag->src_dir, new_dir));
        }
        if (in->drag == LP_DRAG_DROP) {
            if (new_kind != DROP_NONE) drop_into(f, d, new_dir, new_kind == DROP_SIDEBAR && f->side[new_index].trash);
            f->drop_kind = DROP_NONE;
            f->drop_index = -1;
            ctx->dirty = 1;
        }
    } else if (event && f->drop_kind != DROP_NONE && (!drag || !drag->active)) {
        f->drop_kind = DROP_NONE;
        f->drop_index = -1;
        ctx->dirty = 1;
    }
    if (event && f->stale) reload(f);

    /* MARK: path bar and status */
    path_bar(f, d, ctx, pathbar, base);
    if (draw) status_bar(f, ctx, status);
    if (event && state) sync_title(f, d);
    #undef ITEM_RECT
}

/* MARK: - Introspection (tests) */

const char *lp_finder_path(const void *state) { return ((const struct finder *)state)->path; }
int lp_finder_visible_count(const void *state) { return ((const struct finder *)state)->nvis; }
const char *lp_finder_visible_name(const void *state, int v) {
    const struct finder *f = state;
    return v >= 0 && v < f->nvis ? f->list.entries[f->vis[v]].name : NULL;
}
int lp_finder_is_selected(const void *state, const char *name) {
    const struct finder *f = state;
    int i = lp_files_find(&f->list, name);
    return i >= 0 && f->sel[i];
}
int lp_finder_selected_count(const void *state) { return selected_count(state); }
int lp_finder_renaming(const void *state) { return ((const struct finder *)state)->renaming >= 0; }
int lp_finder_view(const void *state) { return ((const struct finder *)state)->view; }
int lp_finder_sidebar_count(const void *state) { return ((const struct finder *)state)->nside; }

const lp_app lp_app_finder = {
    .id = "finder", .title = "Rao", .name = "Finder", .dock = 1, .icon = LP_ICON_FOLDER, .object = "appFinder", .default_rect = { 72, 72, 720, 460 }, .min_size = { 420, 240 }, .singleton = 0, .resizable = 1,
    .create = finder_create, .paint = finder_paint, .destroy = finder_destroy,
    .open = finder_open, .command = finder_command, .menu_entries = finder_menu_entries, .notify = finder_notify, .title_of = finder_title_of,
};
