/* The desktop model (Desktop.tsx + menus.ts + useDesktopKeys.ts without
 * React): the window-manager state, the settings, the built-in app instances,
 * the app commands as data, the commands they bind, and the keyboard
 * shortcuts. Backend-agnostic: the compositor is one host, lp-render another.
 * The commands are shown as Spotlight's pills; there is no menu bar. */
#ifndef MARYUI_LP_DESKTOP_H
#define MARYUI_LP_DESKTOP_H

#include <stdint.h>

#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/lp_app.h"
#include "maryui/lp_drag.h"
#include "maryui/lp_menus.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_spotlight.h"
#include "maryui/lp_wm.h"

#define LP_DESKTOP_MAX_APPS 16
#define LP_DESKTOP_MENU_COUNT 7           /* Rao, File, Edit, View, Go, Window, Help */
#define LP_DESKTOP_MENU_POPUP LP_DESKTOP_MENU_COUNT /* open_menu for the context menu (menus[7]) */
enum lp_menu_slot { LP_MENU_RAO, LP_MENU_FILE, LP_MENU_EDIT, LP_MENU_VIEW, LP_MENU_GO, LP_MENU_WINDOW, LP_MENU_HELP };

enum lp_command {
    LP_CMD_NONE,
    LP_CMD_OPEN_APP,          /* arg: app index */
    LP_CMD_CLOSE_FOCUSED,
    LP_CMD_TOGGLE_SHADE_FOCUSED,
    LP_CMD_TOGGLE_ZOOM_FOCUSED,
    LP_CMD_FOCUS_NEXT,
    LP_CMD_FOCUS_WINDOW,      /* arg: window index */
    LP_CMD_SET_ACCENT,        /* arg: lp_accent_kind */
    LP_CMD_SET_FOLDERS,       /* arg: lp_folder_appearance */
    LP_CMD_TOGGLE_GOO,
    LP_CMD_SET_WALLPAPER,     /* arg: lp_wallpaper_mode */
    LP_CMD_SET_MOLTEN_TONE,   /* arg: lp_molten_tone */
    LP_CMD_TOGGLE_REDUCED_MOTION,
    LP_CMD_TOGGLE_CLOCK,
    LP_CMD_NEW_TERMINAL,
    LP_CMD_HELP,
    LP_CMD_APP,               /* arg: an app command id, for the focused (or the popup's) window */
    LP_CMD_GO,                /* arg: lp_user_dir — a new Finder window there */
};

typedef struct lp_app_instance {
    char window_id[12];
    const lp_app *app;
    void *state;
} lp_app_instance;

struct lp_desktop;
typedef void (*lp_desktop_change_fn)(struct lp_desktop *d, uint64_t changed);
typedef void (*lp_desktop_spawn_fn)(struct lp_desktop *d, const char *command);

/* Event sources an app asks the host for: a terminal's pty, a player's frame
 * eventfd, a monitor's refresh tick. The compositor backs them with its
 * wl_event_loop (the mask bits are WL_EVENT_*'s), the tests with
 * tests/lp_test_loop.h; lp-render has none, so every app must cope with NULL.
 * Callbacks run on the host's thread and return 0. A timer's fd is -1. */
enum { LP_SOURCE_READABLE = 1, LP_SOURCE_WRITABLE = 2, LP_SOURCE_HANGUP = 4, LP_SOURCE_ERROR = 8 };
typedef struct lp_source lp_source;
typedef int (*lp_source_fn)(int fd, uint32_t mask, void *data);

typedef struct lp_desktop {
    lp_wm_state wm;
    lp_settings settings;
    lp_branding branding;
    const lp_app *apps[LP_DESKTOP_MAX_APPS];
    int app_count;
    lp_app_instance instances[LP_WM_MAX_WINDOWS];
    int instance_count;
    lp_menu_model menus[LP_DESKTOP_MENU_COUNT + 1];  /* [LP_DESKTOP_MENU_POPUP] is the context menu */
    int open_menu;            /* -1 when closed; LP_DESKTOP_MENU_POPUP for the context menu */
    char popup_window[12];    /* the window the context menu belongs to */
    float popup_x, popup_y;   /* its anchor, in that window's chrome coordinates */
    char command_target[12];  /* LP_CMD_APP goes here when set (a popup entry), else to the focused window */
    char pending_open[1024];  /* the path for the instance OPEN is about to create */
    lp_drag drag;             /* the drag session, when active */
    int menu_active;          /* highlighted entry of the open menu, -1 */
    lp_spotlight spotlight;   /* Ctrl+Space: the search bar and dock */
    char about_label[160];
    lp_desktop_change_fn on_change;   /* the WM changed: the host syncs its windows */
    void (*on_settings)(struct lp_desktop *d);
    lp_desktop_spawn_fn spawn;
    /* Asked before CLOSE; return 1 to take over (a Wayland client is asked to close itself). */
    int (*request_close)(struct lp_desktop *d, const char *window_id);
    /* A drag began (1) or ended (0): the host shows or hides the ghost and grabs the pointer. */
    void (*on_drag)(struct lp_desktop *d, int begin);
    /* A window's app wants a repaint (a directory changed, a command ran). Damage only: this may run inside an EVENT pass. */
    void (*on_app_dirty)(struct lp_desktop *d, const char *window_id);
    /* Watch (on) or stop watching a directory for changes; the host calls lp_desktop_files_changed. */
    void (*watch)(struct lp_desktop *d, const char *dir, int on);
    /* Event sources (see lp_source_fn). A timer is one-shot: update_timer arms it, 0 disarms. */
    lp_source *(*add_fd)(struct lp_desktop *d, int fd, uint32_t mask, lp_source_fn fn, void *data);
    lp_source *(*add_timer)(struct lp_desktop *d, lp_source_fn fn, void *data);
    void (*update_timer)(struct lp_desktop *d, lp_source *source, int ms);
    void (*remove_source)(struct lp_desktop *d, lp_source *source);
    void *host;
} lp_desktop;

void lp_desktop_init(lp_desktop *d, lp_rect bounds, void *host);
void lp_desktop_register_app(lp_desktop *d, const lp_app *app);
/* Registers finder, gallery, about, textedit (hidden: Spotlight only), info (internal)
 * and the system apps (calculator, preview, terminal, activity, diskutil); Finder, TextEdit, Preview and Terminal are pinned. */
void lp_desktop_register_builtin_apps(lp_desktop *d);
/* Runs the WM reducer, syncs app instances, calls on_change. Returns the change mask. */
uint64_t lp_desktop_dispatch(lp_desktop *d, const lp_wm_action *action);
void lp_desktop_open_app(lp_desktop *d, const char *app_id);
/* Opens the app with a path (its `open` hook) and a title; the window id comes back in out_id. Returns 1 on success. */
int lp_desktop_open_app_with(lp_desktop *d, const char *app_id, const char *path, const char *title, char out_id[12]);
/* A folder opens in a new Finder window; an image or a PDF in Preview and audio or
 * video in the Media Player once those apps are registered; a text file in
 * TextEdit. 0 when nothing can open it. */
int lp_desktop_open_path(lp_desktop *d, const char *path);
/* A directory changed: every instance's `notify`, then on_app_dirty for those that want a repaint. */
void lp_desktop_files_changed(lp_desktop *d, const char *dir);
/* The host's event sources, NULL-safe: each returns NULL (or does nothing) when the host has none.
 * lp_desktop_add_timer arms the timer when ms > 0. */
lp_source *lp_desktop_add_fd(lp_desktop *d, int fd, uint32_t mask, lp_source_fn fn, void *data);
lp_source *lp_desktop_add_timer(lp_desktop *d, int ms, lp_source_fn fn, void *data);
void lp_desktop_update_timer(lp_desktop *d, lp_source *source, int ms);
void lp_desktop_remove_source(lp_desktop *d, lp_source *source);
/* Opens a context menu for a window at (x, y) in its chrome coordinates. */
void lp_desktop_open_popup(lp_desktop *d, const char *window_id, float x, float y, const lp_menu_model *model);
const lp_app *lp_desktop_find_app(const lp_desktop *d, const char *app_id);
lp_app_instance *lp_desktop_instance(lp_desktop *d, const char *window_id);
/* Rebuilds d->menus from the current state (menus.ts). */
void lp_desktop_build_menus(lp_desktop *d);
int lp_desktop_run_command(lp_desktop *d, enum lp_command command, int arg);
/* Menu keyboard navigation, Spotlight (Ctrl/⌘+Space toggles; Esc, ↑/↓, Enter
 * while open) and desktop shortcuts. Returns 1 when handled; while Spotlight
 * is open, unhandled keys are for its bar (the host routes them there). */
int lp_desktop_key(lp_desktop *d, uint32_t keysym, uint32_t mods);
/* Spotlight: the results for the current query (the dock when it is blank), and launching one. */
int lp_desktop_spotlight_results(const lp_desktop *d, lp_spotlight_item *out, int max);
/* Opens the app, focuses the window or runs the command at `index` of the results, then closes Spotlight. */
void lp_desktop_spotlight_activate(lp_desktop *d, int index);
/* The bar edited the query: re-rank from the top, and close the open command
 * menu, which shows only while the query is blank. */
void lp_desktop_spotlight_query_changed(lp_desktop *d);
/* Everything the panel paints for this desktop: the results plus the command
 * pills and the "Searching <app>" line. The host fills in width/focus_bar/max_h. */
lp_spotlight_view lp_desktop_spotlight_view(lp_desktop *d, const lp_spotlight_item *items, int count);
/* Command-pill interaction: open/close/switch. */
void lp_desktop_toggle_menu(lp_desktop *d, int index);
void lp_desktop_close_menu(lp_desktop *d);
/* Selects an entry of the open menu (runs its command, closes the menu). */
void lp_desktop_select_menu_entry(lp_desktop *d, int entry);
/* Closes a window through request_close when the host wants a say. */
void lp_desktop_close_window(lp_desktop *d, const char *window_id);

#endif
