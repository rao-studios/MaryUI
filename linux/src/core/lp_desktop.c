#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"
#include "maryui/lp_tokens.h"

void lp_desktop_init(lp_desktop *d, lp_rect bounds, void *host) {
    memset(d, 0, sizeof *d);
    lp_wm_init(&d->wm, bounds);
    d->settings = lp_settings_load();
    d->branding = lp_branding_load();
    d->open_menu = -1;
    d->menu_active = -1;
    lp_spotlight_init(&d->spotlight);
    d->host = host;
    snprintf(d->about_label, sizeof d->about_label, "About %s", d->branding.pretty_name);
}

void lp_desktop_register_app(lp_desktop *d, const lp_app *app) {
    if (d->app_count < LP_DESKTOP_MAX_APPS) d->apps[d->app_count++] = app;
}

void lp_desktop_register_builtin_apps(lp_desktop *d) {
    lp_desktop_register_app(d, &lp_app_finder);
    lp_desktop_register_app(d, &lp_app_gallery);
    lp_desktop_register_app(d, &lp_app_about);
    lp_desktop_register_app(d, &lp_app_textedit);
    lp_desktop_register_app(d, &lp_app_info);
    lp_desktop_register_app(d, &lp_app_calculator);
    lp_desktop_register_app(d, &lp_app_preview);
}

const lp_app *lp_desktop_find_app(const lp_desktop *d, const char *app_id) {
    for (int i = 0; i < d->app_count; i++) if (strcmp(d->apps[i]->id, app_id) == 0) return d->apps[i];
    return NULL;
}

lp_app_instance *lp_desktop_instance(lp_desktop *d, const char *window_id) {
    for (int i = 0; i < d->instance_count; i++) if (strcmp(d->instances[i].window_id, window_id) == 0) return &d->instances[i];
    return NULL;
}

/* Instances follow the WM: one per window of a built-in app, created on open, destroyed on close. */
static void sync_instances(lp_desktop *d) {
    for (int i = 0; i < d->instance_count;) {
        if (lp_wm_find(&d->wm, d->instances[i].window_id) < 0) {
            if (d->instances[i].app->destroy) d->instances[i].app->destroy(d->instances[i].state);
            d->instances[i] = d->instances[--d->instance_count];
        } else {
            i++;
        }
    }
    for (int w = 0; w < d->wm.count; w++) {
        const lp_window_record *rec = &d->wm.windows[w];
        if (lp_desktop_instance(d, rec->id)) continue;
        const lp_app *app = lp_desktop_find_app(d, rec->app_id);
        if (!app || d->instance_count >= LP_WM_MAX_WINDOWS) continue;
        lp_app_instance *inst = &d->instances[d->instance_count++];
        snprintf(inst->window_id, sizeof inst->window_id, "%s", rec->id);
        inst->app = app;
        inst->state = app->create ? app->create(d, rec->id) : NULL;
        /* The path OPEN was asked to hand over (lp_desktop_open_app_with). */
        if (d->pending_open[0] && app->open) app->open(inst->state, d, d->pending_open);
        d->pending_open[0] = 0;
    }
}

uint64_t lp_desktop_dispatch(lp_desktop *d, const lp_wm_action *action) {
    uint64_t changed = lp_wm_reduce(&d->wm, action);
    if (!changed) return 0;
    if (changed & (LP_WM_CHANGED_OPENED | LP_WM_CHANGED_CLOSED)) sync_instances(d);
    if (d->on_change) d->on_change(d, changed);
    return changed;
}

int lp_desktop_open_app_with(lp_desktop *d, const char *app_id, const char *path, const char *title, char out_id[12]) {
    const lp_app *app = lp_desktop_find_app(d, app_id);
    if (!app) return 0;
    snprintf(d->pending_open, sizeof d->pending_open, "%s", path ? path : "");
    lp_wm_action a = { .type = LP_WM_OPEN, .spec = lp_open_spec_default(app->id, title ? title : app->title) };
    a.spec.rect = app->default_rect;
    a.spec.min_size = app->min_size;
    a.spec.singleton = app->singleton;
    a.spec.resizable = app->resizable;
    int before = d->wm.count;
    lp_desktop_dispatch(d, &a);
    d->pending_open[0] = 0;
    const lp_window_record *rec = NULL;
    if (d->wm.count == before + 1) {
        rec = &d->wm.windows[d->wm.count - 1];
        if (!title && app->title_of) {
            lp_app_instance *inst = lp_desktop_instance(d, rec->id);
            char name[128] = "";
            if (inst) app->title_of(inst->state, name, sizeof name);
            if (name[0]) {
                char id[12];
                snprintf(id, sizeof id, "%s", rec->id);
                lp_wm_action t = { .type = LP_WM_SET_TITLE, .id = id, .title = name };
                lp_desktop_dispatch(d, &t);
                rec = &d->wm.windows[lp_wm_find(&d->wm, id)];
            }
        }
    } else {
        /* a singleton brought forward: hand the path to the instance it already has */
        rec = lp_wm_focused(&d->wm);
        if (!rec || strcmp(rec->app_id, app->id) != 0) return 0;
        lp_app_instance *inst = lp_desktop_instance(d, rec->id);
        if (inst && app->open && path) app->open(inst->state, d, path);
        if (title) { lp_wm_action t = { .type = LP_WM_SET_TITLE, .id = rec->id, .title = title }; lp_desktop_dispatch(d, &t); }
    }
    if (out_id) snprintf(out_id, 12, "%s", rec->id);
    return 1;
}

void lp_desktop_open_app(lp_desktop *d, const char *app_id) { lp_desktop_open_app_with(d, app_id, NULL, NULL, NULL); }

int lp_desktop_open_path(lp_desktop *d, const char *path) {
    int is_dir = 0;
    if (!path || !lp_files_exists(path, &is_dir)) return 0;
    char name[LP_FILES_NAME_MAX];
    lp_files_display_name(path, name, sizeof name);
    if (is_dir) return lp_desktop_open_app_with(d, "finder", path, name, NULL);
    /* The viewers first: an SVG is text too, but it is a picture before it is a document. */
    enum lp_file_kind kind = lp_files_kind(path, 0);
    const char *viewer = kind == LP_FILE_IMAGE || kind == LP_FILE_PDF ? "preview"
                       : kind == LP_FILE_MUSIC || kind == LP_FILE_VIDEO ? "media" : NULL;
    if (viewer && lp_desktop_find_app(d, viewer)) return lp_desktop_open_app_with(d, viewer, path, name, NULL);
    /* Audio, video and PDFs are never text, however they sniff (an empty .mp4 does); an image may be (SVG). */
    if (kind == LP_FILE_MUSIC || kind == LP_FILE_VIDEO || kind == LP_FILE_PDF) return 0;
    if (lp_files_is_text(path)) return lp_desktop_open_app_with(d, "textedit", path, name, NULL);
    return 0;
}

void lp_desktop_files_changed(lp_desktop *d, const char *dir) {
    for (int i = 0; i < d->instance_count; i++) {
        lp_app_instance *inst = &d->instances[i];
        if (!inst->app->notify) continue;
        if (inst->app->notify(inst->state, d, dir) && d->on_app_dirty) d->on_app_dirty(d, inst->window_id);
    }
}

lp_source *lp_desktop_add_fd(lp_desktop *d, int fd, uint32_t mask, lp_source_fn fn, void *data) {
    return d->add_fd ? d->add_fd(d, fd, mask, fn, data) : NULL;
}

lp_source *lp_desktop_add_timer(lp_desktop *d, int ms, lp_source_fn fn, void *data) {
    lp_source *s = d->add_timer ? d->add_timer(d, fn, data) : NULL;
    if (s && ms > 0) lp_desktop_update_timer(d, s, ms);
    return s;
}

void lp_desktop_update_timer(lp_desktop *d, lp_source *source, int ms) {
    if (source && d->update_timer) d->update_timer(d, source, ms > 0 ? ms : 0);
}

void lp_desktop_remove_source(lp_desktop *d, lp_source *source) {
    if (source && d->remove_source) d->remove_source(d, source);
}

void lp_desktop_open_popup(lp_desktop *d, const char *window_id, float x, float y, const lp_menu_model *model) {
    d->menus[LP_DESKTOP_MENU_POPUP] = *model;
    d->menus[LP_DESKTOP_MENU_POPUP].id = "popup";
    d->menus[LP_DESKTOP_MENU_POPUP].label = "";
    snprintf(d->popup_window, sizeof d->popup_window, "%s", window_id ? window_id : "");
    d->popup_x = x;
    d->popup_y = y;
    lp_spotlight_close(&d->spotlight);
    d->open_menu = LP_DESKTOP_MENU_POPUP;
    d->menu_active = -1;
}

static void settings_changed(lp_desktop *d) {
    lp_settings_save(&d->settings);
    if (d->on_settings) d->on_settings(d);
}

int lp_desktop_run_command(lp_desktop *d, enum lp_command command, int arg) {
    const lp_window_record *focused = lp_wm_focused(&d->wm);
    lp_wm_action a = { 0 };
    switch (command) {
    case LP_CMD_NONE: return 0;
    case LP_CMD_OPEN_APP:
        if (arg >= 0 && arg < d->app_count) lp_desktop_open_app(d, d->apps[arg]->id);
        return 1;
    case LP_CMD_CLOSE_FOCUSED:
        if (!focused) return 0;
        lp_desktop_close_window(d, focused->id);
        return 1;
    case LP_CMD_TOGGLE_SHADE_FOCUSED:
        if (!focused) return 0;
        a.type = LP_WM_TOGGLE_SHADE; a.id = focused->id;
        lp_desktop_dispatch(d, &a);
        return 1;
    case LP_CMD_TOGGLE_ZOOM_FOCUSED:
        if (!focused) return 0;
        a.type = LP_WM_TOGGLE_ZOOM; a.id = focused->id;
        lp_desktop_dispatch(d, &a);
        return 1;
    case LP_CMD_FOCUS_NEXT:
        a.type = LP_WM_FOCUS_NEXT;
        lp_desktop_dispatch(d, &a);
        return 1;
    case LP_CMD_FOCUS_WINDOW: {
        if (arg < 0 || arg >= d->wm.count) return 0;
        char id[12];
        snprintf(id, sizeof id, "%s", d->wm.windows[arg].id);
        if (d->wm.windows[arg].state == LP_WIN_SHADED) { a.type = LP_WM_TOGGLE_SHADE; a.id = id; lp_desktop_dispatch(d, &a); }
        a.type = LP_WM_FOCUS; a.id = id;
        lp_desktop_dispatch(d, &a);
        return 1;
    }
    case LP_CMD_SET_ACCENT: d->settings.accent = (enum lp_accent_kind)arg; settings_changed(d); return 1;
    case LP_CMD_SET_FOLDERS: d->settings.folders = (enum lp_folder_appearance)arg; settings_changed(d); return 1;
    case LP_CMD_TOGGLE_GOO: d->settings.goo = !d->settings.goo; settings_changed(d); return 1;
    case LP_CMD_SET_WALLPAPER: d->settings.wallpaper = (enum lp_wallpaper_mode)arg; settings_changed(d); return 1;
    case LP_CMD_SET_MOLTEN_TONE: d->settings.molten_tone = (enum lp_molten_tone)arg; settings_changed(d); return 1;
    case LP_CMD_TOGGLE_REDUCED_MOTION: d->settings.reduced_motion = !d->settings.reduced_motion; settings_changed(d); return 1;
    case LP_CMD_TOGGLE_CLOCK: d->settings.clock = !d->settings.clock; settings_changed(d); return 1;
    case LP_CMD_NEW_TERMINAL:
        /* the native Terminal once it is registered; foot, a Wayland client, until then */
        if (lp_desktop_find_app(d, "terminal")) lp_desktop_open_app(d, "terminal");
        else if (d->spawn) d->spawn(d, "foot");
        return 1;
    case LP_CMD_HELP: return 1;
    case LP_CMD_APP: {
        const char *target = d->command_target[0] ? d->command_target : focused ? focused->id : NULL;
        lp_app_instance *inst = target ? lp_desktop_instance(d, target) : NULL;
        if (!inst || !inst->app->command) return 0;
        char id[12];
        snprintf(id, sizeof id, "%s", inst->window_id);
        inst->app->command(inst->state, d, arg);
        if (d->on_app_dirty && lp_wm_find(&d->wm, id) >= 0) d->on_app_dirty(d, id);
        return 1;
    }
    case LP_CMD_GO: {
        char path[LP_FILES_PATH_MAX];
        lp_files_user_dir((enum lp_user_dir)arg, path, sizeof path);
        return lp_desktop_open_path(d, path);
    }
    }
    return 0;
}

static lp_menu_entry *add(lp_menu_model *m, const char *label, const char *shortcut, enum lp_command cmd, int arg) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return &m->entries[LP_MENU_MAX_ENTRIES - 1];
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "%s", label);
    e->shortcut = shortcut;
    e->command = cmd;
    e->arg = arg;
    return e;
}
static void sep(lp_menu_model *m) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    e->separator = 1;
}
static int app_index(const lp_desktop *d, const char *id) {
    for (int i = 0; i < d->app_count; i++) if (strcmp(d->apps[i]->id, id) == 0) return i;
    return -1;
}

/* menus.ts, as data, plus the focused app's entries (File, Edit, View, Go). The Mac's ⌘ is the guest's Super key. */
void lp_desktop_build_menus(lp_desktop *d) {
    const lp_window_record *focused = lp_wm_focused(&d->wm);
    lp_app_instance *inst = focused ? lp_desktop_instance(d, focused->id) : NULL;
    if (inst && !inst->app->menu_entries) inst = NULL;
    lp_menu_model *m;
    for (int i = 0; i < LP_DESKTOP_MENU_COUNT; i++) d->menus[i].count = 0;

    m = &d->menus[LP_MENU_RAO]; m->id = "rao"; m->label = "Rao";
    add(m, d->about_label, NULL, LP_CMD_OPEN_APP, app_index(d, "about"));
    sep(m);
    add(m, "Design System…", NULL, LP_CMD_OPEN_APP, app_index(d, "gallery"));
    sep(m);
    add(m, "Reduce Motion", NULL, LP_CMD_TOGGLE_REDUCED_MOTION, 0)->checked = d->settings.reduced_motion;

    m = &d->menus[LP_MENU_FILE]; m->id = "file"; m->label = "File";
    add(m, "New Finder Window", "⌘N", LP_CMD_OPEN_APP, app_index(d, "finder"));
    add(m, "New Terminal", "⌘T", LP_CMD_NEW_TERMINAL, 0);
    int before = m->count;
    if (inst) inst->app->menu_entries(inst->state, d, LP_MENU_FILE, m);
    int app_file = m->count > before;
    if (!app_file) add(m, "Open…", "⌘O", LP_CMD_NONE, 0)->disabled = 1;
    sep(m);
    add(m, "Close Window", "⌘W", LP_CMD_CLOSE_FOCUSED, 0)->disabled = !focused;
    if (!app_file) { sep(m); add(m, "Get Info", "⌘I", LP_CMD_NONE, 0)->disabled = 1; }

    m = &d->menus[LP_MENU_EDIT]; m->id = "edit"; m->label = "Edit";
    before = m->count;
    if (inst) inst->app->menu_entries(inst->state, d, LP_MENU_EDIT, m);
    if (m->count == before) {
        add(m, "Undo", "⌘Z", LP_CMD_NONE, 0)->disabled = 1;
        add(m, "Redo", "⇧⌘Z", LP_CMD_NONE, 0)->disabled = 1;
        sep(m);
        add(m, "Cut", "⌘X", LP_CMD_NONE, 0)->disabled = 1;
        add(m, "Copy", "⌘C", LP_CMD_NONE, 0)->disabled = 1;
        add(m, "Paste", "⌘V", LP_CMD_NONE, 0)->disabled = 1;
        sep(m);
        add(m, "Select All", "⌘A", LP_CMD_NONE, 0)->disabled = 1;
    }

    m = &d->menus[LP_MENU_VIEW]; m->id = "view"; m->label = "View";
    before = m->count;
    if (inst) inst->app->menu_entries(inst->state, d, LP_MENU_VIEW, m);
    if (m->count > before) sep(m);
    /* The desktop's two switches. Show Clock is C only (PARITY D13): the one thing left on the desktop itself. */
    add(m, "Show Clock", NULL, LP_CMD_TOGGLE_CLOCK, 0)->checked = d->settings.clock;
    add(m, "Liquid Merge", NULL, LP_CMD_TOGGLE_GOO, 0)->checked = d->settings.goo;
    sep(m);
    add(m, "Molten Wallpaper", NULL, LP_CMD_SET_WALLPAPER, LP_WALLPAPER_MOLTEN)->checked = d->settings.wallpaper == LP_WALLPAPER_MOLTEN;
    add(m, "Procedural Wallpaper", NULL, LP_CMD_SET_WALLPAPER, LP_WALLPAPER_PROCEDURAL)->checked = d->settings.wallpaper == LP_WALLPAPER_PROCEDURAL;
    add(m, "Raster Wallpaper", NULL, LP_CMD_SET_WALLPAPER, LP_WALLPAPER_RASTER)->checked = d->settings.wallpaper == LP_WALLPAPER_RASTER;
    sep(m);
    {
        /* The two grades only mean anything while the shader is what is showing. */
        int molten = d->settings.wallpaper == LP_WALLPAPER_MOLTEN;
        lp_menu_entry *it = add(m, "Molten · Platinum", NULL, LP_CMD_SET_MOLTEN_TONE, LP_MOLTEN_PLATINUM);
        it->checked = d->settings.molten_tone == LP_MOLTEN_PLATINUM;
        it->disabled = !molten;
        it = add(m, "Molten · Faithful", NULL, LP_CMD_SET_MOLTEN_TONE, LP_MOLTEN_FAITHFUL);
        it->checked = d->settings.molten_tone == LP_MOLTEN_FAITHFUL;
        it->disabled = !molten;
    }
    sep(m);
    add(m, "Blue Appearance", NULL, LP_CMD_SET_ACCENT, LP_ACCENT_BLUE)->checked = d->settings.accent == LP_ACCENT_BLUE;
    add(m, "Graphite Appearance", NULL, LP_CMD_SET_ACCENT, LP_ACCENT_GRAPHITE)->checked = d->settings.accent == LP_ACCENT_GRAPHITE;
    sep(m);
    /* The folder material's two appearances, as menus.ts has them. */
    add(m, "Folders · Manila", NULL, LP_CMD_SET_FOLDERS, LP_FOLDER_MANILA)->checked = d->settings.folders == LP_FOLDER_MANILA;
    add(m, "Folders · Slate", NULL, LP_CMD_SET_FOLDERS, LP_FOLDER_SLATE)->checked = d->settings.folders == LP_FOLDER_SLATE;

    m = &d->menus[LP_MENU_GO]; m->id = "go"; m->label = "Go";
    before = m->count;
    if (inst) inst->app->menu_entries(inst->state, d, LP_MENU_GO, m);
    if (m->count == before) {
        add(m, lp_files_user_dir_label(LP_USER_HOME), "⇧⌘H", LP_CMD_GO, LP_USER_HOME);
        add(m, "Desktop", "⇧⌘D", LP_CMD_GO, LP_USER_DESKTOP);
        add(m, "Documents", "⇧⌘O", LP_CMD_GO, LP_USER_DOCUMENTS);
        add(m, "Downloads", "⌥⌘L", LP_CMD_GO, LP_USER_DOWNLOADS);
        sep(m);
        add(m, "Trash", NULL, LP_CMD_GO, LP_USER_TRASH);
    }

    m = &d->menus[LP_MENU_WINDOW]; m->id = "window"; m->label = "Window";
    add(m, focused && focused->state == LP_WIN_SHADED ? "Unshade" : "Shade", "⌘M", LP_CMD_TOGGLE_SHADE_FOCUSED, 0)->disabled = !focused;
    add(m, focused && focused->state == LP_WIN_ZOOMED ? "Restore" : "Zoom", NULL, LP_CMD_TOGGLE_ZOOM_FOCUSED, 0)->disabled = !focused;
    add(m, "Cycle Through Windows", "⌃`", LP_CMD_FOCUS_NEXT, 0)->disabled = d->wm.count < 2;
    sep(m);
    for (int i = 0; i < d->app_count; i++) {
        if (d->apps[i]->hidden || d->apps[i]->internal) continue; /* Spotlight-only apps; `i` stays the real index */
        char label[80];
        snprintf(label, sizeof label, "Open %s", d->apps[i]->title);
        add(m, label, NULL, LP_CMD_OPEN_APP, i);
    }
    if (d->wm.count) sep(m);
    for (int i = 0; i < d->wm.count && m->count < LP_MENU_MAX_ENTRIES - 1; i++) {
        char label[160];
        snprintf(label, sizeof label, "%s%s", d->wm.windows[i].state == LP_WIN_SHADED ? "◇ " : "", d->wm.windows[i].title);
        add(m, label, NULL, LP_CMD_FOCUS_WINDOW, i)->checked = d->wm.focused == i;
    }

    m = &d->menus[LP_MENU_HELP]; m->id = "help"; m->label = "Help";
    add(m, "Liquid Platinum Help", NULL, LP_CMD_HELP, 0)->disabled = 1;
    add(m, "Read the README", NULL, LP_CMD_HELP, 1);
}

void lp_desktop_close_window(lp_desktop *d, const char *window_id) {
    char id[12];
    snprintf(id, sizeof id, "%s", window_id);
    if (d->request_close && d->request_close(d, id)) return;
    lp_wm_action a = { .type = LP_WM_CLOSE, .id = id };
    lp_desktop_dispatch(d, &a);
}

/*
 * The commands are Spotlight's pills now, so opening one no longer dismisses
 * the panel they are drawn in — the invariant is the other way round: an open
 * menu below LP_DESKTOP_MENU_POPUP implies Spotlight is up.
 */
void lp_desktop_toggle_menu(lp_desktop *d, int index) {
    if (index < 0 || index >= LP_DESKTOP_MENU_COUNT || d->open_menu == index) {
        d->open_menu = -1;
    } else {
        lp_desktop_build_menus(d);
        d->open_menu = index;
    }
    d->menu_active = -1;
}

void lp_desktop_close_menu(lp_desktop *d) {
    d->open_menu = -1;
    d->menu_active = -1;
}

void lp_desktop_select_menu_entry(lp_desktop *d, int entry) {
    if (d->open_menu < 0) return;
    lp_menu_model *m = &d->menus[d->open_menu];
    if (entry < 0 || entry >= m->count || m->entries[entry].separator || m->entries[entry].disabled) return;
    enum lp_command cmd = (enum lp_command)m->entries[entry].command;
    int arg = m->entries[entry].arg;
    int popup = d->open_menu == LP_DESKTOP_MENU_POPUP;
    lp_desktop_close_menu(d);
    if (popup) snprintf(d->command_target, sizeof d->command_target, "%s", d->popup_window);
    lp_desktop_run_command(d, cmd, arg);
    d->command_target[0] = 0;
}

static int step_enabled(const lp_menu_model *m, int from, int delta) {
    if (m->count == 0) return -1;
    int i = from;
    for (int n = 0; n < m->count; n++) {
        i = (i + delta + m->count) % m->count;
        if (from < 0 && delta > 0 && n == 0) i = 0;
        if (!m->entries[i].separator && !m->entries[i].disabled) return i;
    }
    return -1;
}

/* MARK: - Spotlight */

/* In the dock (a blank query) ←/→ step through the tiles; with text they move the caret. */
static int lp_spotlight_query_is_blank_key(const lp_desktop *d) {
    for (const char *p = d->spotlight.query.text; *p; p++) if (*p != ' ' && *p != '\t') return 0;
    return 1;
}

int lp_desktop_spotlight_results(const lp_desktop *d, lp_spotlight_item *out, int max) {
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS];
    int n = lp_spotlight_items(d, items, LP_SPOTLIGHT_MAX_ITEMS);
    return lp_spotlight_results(items, n, d->spotlight.query.text, out, max);
}

/*
 * The query changed under the bar: rank from the top again, and drop the open
 * command menu, which only shows while the query is blank (the web hides the
 * whole commands section as soon as anything is typed).
 */
void lp_desktop_spotlight_query_changed(lp_desktop *d) {
    d->spotlight.selection = 0;
    if (!lp_spotlight_query_is_blank_key(d)) lp_desktop_close_menu(d);
}

/*
 * The view the desktop paints: the results, the frontmost app's commands, and
 * whose commands they are. One place so the compositor and lp-render cannot
 * drift; `width`, `focus_bar` and `max_h` belong to the host.
 */
lp_spotlight_view lp_desktop_spotlight_view(lp_desktop *d, const lp_spotlight_item *items, int count) {
    /* The menus are built lazily, as the menu bar used to do before it drew. */
    if (d->menus[LP_MENU_FILE].label == NULL) lp_desktop_build_menus(d);
    const lp_window_record *f = lp_wm_focused(&d->wm);
    const lp_app *app = f ? lp_desktop_find_app(d, f->app_id) : NULL;
    return (lp_spotlight_view){
        .query = (lp_text_buffer *)&d->spotlight.query,
        .items = items,
        .count = count,
        .selection = d->spotlight.selection,
        .menus = d->menus,
        .menu_count = LP_DESKTOP_MENU_COUNT,
        /* The Finder's context menu is a floating panel, never a pill. */
        .open_menu = d->open_menu < LP_DESKTOP_MENU_COUNT ? d->open_menu : -1,
        .menu_active = d->menu_active,
        .context_name = f ? (app ? (app->name ? app->name : app->title) : f->app_id) : NULL,
        .context_icon = app ? app->icon : LP_ICON_DOCUMENT,
    };
}

void lp_desktop_spotlight_activate(lp_desktop *d, int index) {
    lp_spotlight_item results[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS);
    lp_spotlight_close(&d->spotlight);
    lp_desktop_close_menu(d);   /* the panel goes: so does its open pill */
    if (index < 0 || index >= n) return;
    const lp_spotlight_item *it = &results[index];
    switch (it->kind) {
    case LP_SPOT_APP: lp_desktop_open_app(d, it->id); break;
    case LP_SPOT_WINDOW: lp_desktop_run_command(d, LP_CMD_FOCUS_WINDOW, it->index); break;
    case LP_SPOT_COMMAND: if (strcmp(it->id, "terminal") == 0) lp_desktop_run_command(d, LP_CMD_NEW_TERMINAL, 0); break;
    }
}

/* MARK: - Keys */

int lp_desktop_key(lp_desktop *d, uint32_t keysym, uint32_t mods) {
    int mod = (mods & (4 | 64)) != 0; /* Ctrl or Logo (⌘) */
    if (mod && keysym == XKB_KEY_space) {
        lp_desktop_close_menu(d);
        lp_spotlight_toggle(&d->spotlight);
        return 1;
    }
    if (d->spotlight.open) {
        lp_spotlight_item results[LP_SPOTLIGHT_MAX_RESULTS];
        /*
         * A command pill is open: it owns the arrows and Enter, the way the
         * dropdown under the menu bar used to. Two deliberate differences from
         * the popup block below, both required because this menu lives inside
         * a text field: space types a space rather than selecting, and an
         * unhandled key falls through to the bar instead of being swallowed.
         */
        if (d->open_menu >= 0 && d->open_menu < LP_DESKTOP_MENU_COUNT) {
            lp_menu_model *m = &d->menus[d->open_menu];
            switch (keysym) {
            case XKB_KEY_Escape: lp_desktop_close_menu(d); return 1;   /* the pill, not the panel */
            case XKB_KEY_Down: d->menu_active = step_enabled(m, d->menu_active, 1); return 1;
            case XKB_KEY_Up: d->menu_active = step_enabled(m, d->menu_active < 0 ? m->count : d->menu_active, -1); return 1;
            case XKB_KEY_Left: lp_desktop_toggle_menu(d, (d->open_menu + LP_DESKTOP_MENU_COUNT - 1) % LP_DESKTOP_MENU_COUNT); return 1;
            case XKB_KEY_Right: lp_desktop_toggle_menu(d, (d->open_menu + 1) % LP_DESKTOP_MENU_COUNT); return 1;
            case XKB_KEY_Return: case XKB_KEY_KP_Enter:
                lp_desktop_select_menu_entry(d, d->menu_active);
                lp_spotlight_close(&d->spotlight);
                return 1;
            default: break;   /* the bar still types, and typing closes the pill */
            }
        }
        switch (keysym) {
        case XKB_KEY_Escape: lp_spotlight_close(&d->spotlight); return 1;
        case XKB_KEY_Down: lp_spotlight_move(&d->spotlight, 1, lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS)); return 1;
        case XKB_KEY_Up: lp_spotlight_move(&d->spotlight, -1, lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS)); return 1;
        case XKB_KEY_Right: if (lp_spotlight_query_is_blank_key(d)) { lp_spotlight_move(&d->spotlight, 1, lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS)); return 1; } return 0;
        case XKB_KEY_Left: if (lp_spotlight_query_is_blank_key(d)) { lp_spotlight_move(&d->spotlight, -1, lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS)); return 1; } return 0;
        case XKB_KEY_Return: case XKB_KEY_KP_Enter: lp_desktop_spotlight_activate(d, d->spotlight.selection); return 1;
        default: return 0; /* the bar edits the query */
        }
    }
    if (d->open_menu >= 0) {
        lp_menu_model *m = &d->menus[d->open_menu];
        switch (keysym) {
        case XKB_KEY_Escape: lp_desktop_close_menu(d); return 1;
        case XKB_KEY_Down: d->menu_active = step_enabled(m, d->menu_active, 1); return 1;
        case XKB_KEY_Up: d->menu_active = step_enabled(m, d->menu_active < 0 ? m->count : d->menu_active, -1); return 1;
        case XKB_KEY_Left: if (d->open_menu < LP_DESKTOP_MENU_COUNT) lp_desktop_toggle_menu(d, (d->open_menu + LP_DESKTOP_MENU_COUNT - 1) % LP_DESKTOP_MENU_COUNT); return 1;
        case XKB_KEY_Right: if (d->open_menu < LP_DESKTOP_MENU_COUNT) lp_desktop_toggle_menu(d, (d->open_menu + 1) % LP_DESKTOP_MENU_COUNT); return 1;
        case XKB_KEY_Return: case XKB_KEY_KP_Enter: case XKB_KEY_space:
            lp_desktop_select_menu_entry(d, d->menu_active);
            return 1;
        default: return 1; /* the menu swallows other keys */
        }
    }
    if (!mod) return 0;
    switch (keysym) {
    case XKB_KEY_grave: return lp_desktop_run_command(d, LP_CMD_FOCUS_NEXT, 0);
    case XKB_KEY_w: case XKB_KEY_W: return lp_desktop_run_command(d, LP_CMD_CLOSE_FOCUSED, 0);
    case XKB_KEY_m: case XKB_KEY_M: return lp_desktop_run_command(d, LP_CMD_TOGGLE_SHADE_FOCUSED, 0);
    case XKB_KEY_n: case XKB_KEY_N:
        if (mods & LP_MOD_SHIFT) return 0; /* ⇧⌘N is the Finder's New Folder */
        return lp_desktop_run_command(d, LP_CMD_OPEN_APP, app_index(d, "finder"));
    case XKB_KEY_t: case XKB_KEY_T: return lp_desktop_run_command(d, LP_CMD_NEW_TERMINAL, 0);
    default: return 0;
    }
}
