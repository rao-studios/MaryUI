/* A built-in application (web/src/desktop/apps/registry.ts): how its window
 * opens and the function that paints its body with the immediate-mode API. */
#ifndef MARYUI_LP_APP_H
#define MARYUI_LP_APP_H

#include <stddef.h>

#include "maryui/lp_menus.h"
#include "maryui/lp_icons.h"
#include "maryui/lp_ui.h"

struct lp_desktop;

typedef struct lp_app {
    const char *id;
    const char *title;      /* the window title */
    const char *name;       /* the display name in Spotlight (NULL: the title) */
    const char *aka;        /* another name Spotlight finds it by (Settings: "System Settings"), or NULL */
    lp_icon icon;           /* the Spotlight tile (LP_ICON_COUNT: a document) */
    const char *object;     /* the object-tier mark when it is not the glyph's own name (lp_objects.h) */
    int hidden;             /* reachable from Spotlight only: not in Window › Open … */
    int internal;           /* not in Spotlight either: opened by other apps (Info) */
    int dock;               /* pinned: in Spotlight's dock (a blank query); the rest are found by typing */
    int raw_ctrl;           /* Ctrl chords are the app's own (Terminal): only Super (⌘) reaches the desktop's shortcuts while it is in front */
    lp_rect default_rect;   /* NAN fields take the cascade default */
    lp_size min_size;       /* 0 = LP_DEFAULT_MIN_SIZE */
    int singleton;
    int resizable;
    void *(*create)(struct lp_desktop *desktop, const char *window_id);
    /* Paints (or, in the EVENT pass, handles input for) the window body. */
    void (*paint)(void *state, lp_ctx *ctx, lp_rect body, struct lp_desktop *desktop);
    void (*destroy)(void *state);
    /* Optional. A path handed over at open time (lp_desktop_open_path); never dispatches. */
    void (*open)(void *state, struct lp_desktop *desktop, const char *path);
    /* An app command (LP_CMD_APP's arg) from a menu, the popup or a shortcut. */
    void (*command)(void *state, struct lp_desktop *desktop, int cmd);
    /* Entries for the File, Edit, View and Go menus (LP_MENU_*) while the window is focused. */
    void (*menu_entries)(void *state, struct lp_desktop *desktop, int menu, lp_menu_model *m);
    /* A directory changed (lp_desktop_files_changed); return 1 to be repainted. */
    int (*notify)(void *state, struct lp_desktop *desktop, const char *dir);
    /* The title a fresh window should carry when none was given (the Finder: its folder's name). */
    void (*title_of)(void *state, char *out, size_t n);
} lp_app;

extern const lp_app lp_app_about;
extern const lp_app lp_app_finder;
extern const lp_app lp_app_gallery;
extern const lp_app lp_app_textedit;
extern const lp_app lp_app_info;
extern const lp_app lp_app_calculator;
extern const lp_app lp_app_preview;
extern const lp_app lp_app_terminal;
extern const lp_app lp_app_activity;
extern const lp_app lp_app_diskutil;
extern const lp_app lp_app_player;
extern const lp_app lp_app_calendar;
extern const lp_app lp_app_prefs;

/* TextEdit: replaces the document (for previews). */
void lp_textedit_set_text(void *state, const char *name, const char *text);
const char *lp_textedit_path(const void *state);
const char *lp_textedit_name(const void *state);
const char *lp_textedit_text(const void *state);
/* The Finder: shows the mock listing instead of a real directory (for previews). */
void lp_finder_set_preview(void *state);
/* The Finder's state, for tests. */
const char *lp_finder_path(const void *state);
int lp_finder_visible_count(const void *state);
const char *lp_finder_visible_name(const void *state, int v);
int lp_finder_is_selected(const void *state, const char *name);
int lp_finder_selected_count(const void *state);
int lp_finder_renaming(const void *state);
int lp_finder_view(const void *state);
int lp_finder_sidebar_count(const void *state);

enum lp_finder_command {
    LP_FINDER_OPEN, LP_FINDER_NEW_FOLDER, LP_FINDER_RENAME, LP_FINDER_DUPLICATE, LP_FINDER_TRASH, LP_FINDER_INFO,
    LP_FINDER_COPY, LP_FINDER_CUT, LP_FINDER_PASTE, LP_FINDER_MOVE_HERE, LP_FINDER_SELECT_ALL,
    LP_FINDER_VIEW_ICONS, LP_FINDER_VIEW_LIST, LP_FINDER_TOGGLE_HIDDEN,
    LP_FINDER_BACK, LP_FINDER_FORWARD, LP_FINDER_UP,
    LP_FINDER_GO_HOME, LP_FINDER_GO_DESKTOP, LP_FINDER_GO_DOCUMENTS, LP_FINDER_GO_DOWNLOADS, LP_FINDER_GO_TRASH,
    LP_FINDER_EMPTY_TRASH,
    LP_FINDER_SORT_NAME, LP_FINDER_SORT_DATE, LP_FINDER_SORT_SIZE, LP_FINDER_SORT_KIND,
};
enum lp_textedit_command { LP_TEXTEDIT_OPEN, LP_TEXTEDIT_SAVE };
enum lp_calculator_command { LP_CALCULATOR_COPY, LP_CALCULATOR_PASTE };
/* The Calculator's model, for tests. */
struct lp_calc;
const struct lp_calc *lp_calculator_calc(const void *state);
/* The Calculator: types keys in (digits . + - * / = %), for previews. */
void lp_calculator_type(void *state, const char *keys);

enum lp_preview_command {
    LP_PREVIEW_PREVIOUS, LP_PREVIEW_NEXT, LP_PREVIEW_PREVIOUS_PAGE, LP_PREVIEW_NEXT_PAGE,
    LP_PREVIEW_ZOOM_IN, LP_PREVIEW_ZOOM_OUT, LP_PREVIEW_ACTUAL_SIZE, LP_PREVIEW_ZOOM_TO_FIT,
    LP_PREVIEW_ROTATE_LEFT, LP_PREVIEW_ROTATE_RIGHT, LP_PREVIEW_SHOW_IN_FINDER,
};
/* Preview's state, for tests. */
const char *lp_preview_path(const void *state);
int lp_preview_error(const void *state);   /* lp_image_open's, 0 when the document opened */
int lp_preview_page(const void *state);
int lp_preview_turns(const void *state);
int lp_preview_fits(const void *state);
float lp_preview_scale(const void *state);

enum lp_terminal_command { LP_TERMINAL_COPY, LP_TERMINAL_PASTE, LP_TERMINAL_CLEAR_SCROLLBACK };
/* Terminal: feeds bytes to its screen as if the shell wrote them (NULL: the sample session previews show). */
void lp_terminal_feed(void *state, const char *bytes);
/* Its state, for tests: a screen row's text, the shell's pid (0 once it is gone), whether it has exited. */
void lp_terminal_row_text(const void *state, int row, char *out, size_t n);
int lp_terminal_pid(const void *state);
int lp_terminal_exited(const void *state);

enum lp_activity_command { LP_ACTIVITY_QUIT_PROCESS, LP_ACTIVITY_VIEW_CPU, LP_ACTIVITY_VIEW_MEMORY };
/* Activity Monitor, for tests: read another /proc tree, stand in for kill(2), search, select, and look. */
void lp_activity_set_root(void *state, const char *root);
void lp_activity_set_signal(void *state, int (*signal_fn)(int pid, int force));
void lp_activity_search(void *state, const char *query);
void lp_activity_select(void *state, int pid);
int lp_activity_selected(const void *state);
int lp_activity_visible_count(const void *state);
int lp_activity_visible_pid(const void *state, int v);
double lp_activity_cpu(const void *state, int pid);   /* -1 when the pid is not listed */
int lp_activity_sheet_open(const void *state);
const char *lp_activity_status(const void *state);
lp_rect lp_activity_sheet_button(const void *state, lp_ctx *ctx, lp_rect body, int button);

enum lp_diskutil_command { LP_DISKUTIL_MOUNT, LP_DISKUTIL_UNMOUNT, LP_DISKUTIL_EJECT, LP_DISKUTIL_SHOW_IN_FINDER };
/* Disk Utility, for tests: read fixture sysfs/udev/mounts paths, stand in for lp_job_run, select, and ask what it offers. */
struct lp_job;
void lp_diskutil_set_paths(void *state, const char *sys_block, const char *udev_data, const char *mounts_path);
void lp_diskutil_set_runner(void *state, struct lp_job *(*run)(struct lp_desktop *d, const char *const *argv,
                            void (*done)(int status, const char *output, void *user), void *user));
void lp_diskutil_select(void *state, const char *device);
const char *lp_diskutil_selected(const void *state);
int lp_diskutil_can(const void *state, int command);
int lp_diskutil_busy(const void *state);
const char *lp_diskutil_status(const void *state);
void lp_diskutil_refresh(void *state);

enum lp_player_command {
    LP_PLAYER_PLAY_PAUSE, LP_PLAYER_PREVIOUS, LP_PLAYER_NEXT, LP_PLAYER_SKIP_BACK, LP_PLAYER_SKIP_FORWARD,
    LP_PLAYER_VOLUME_UP, LP_PLAYER_VOLUME_DOWN, LP_PLAYER_SHOW_IN_FINDER,
};
/* The Media Player (app id "media"), for tests: its file, its state (an lp_media_state, or -errno when the
 * file would not open), and its place in the folder's queue. */
const char *lp_player_path(const void *state);
int lp_player_state(const void *state);
int lp_player_queue_position(const void *state, int *count);

enum lp_calendar_command {
    LP_CALENDAR_NEW_EVENT, LP_CALENDAR_VIEW_DAY, LP_CALENDAR_VIEW_WEEK, LP_CALENDAR_VIEW_MONTH, LP_CALENDAR_TODAY,
    LP_CALENDAR_PREVIOUS, LP_CALENDAR_NEXT, LP_CALENDAR_SAVE_EVENT, LP_CALENDAR_DELETE_EVENT, LP_CALENDAR_CANCEL_EDIT,
};
/* Calendar, for tests: another calendar folder, the day shown, the view (0 day, 1 week, 2 month), the editor
 * filled in as a person would type it, and what it says back. */
void lp_calendar_app_set_dir(void *state, const char *dir);
void lp_calendar_app_show(void *state, int year, int month, int day);
void lp_calendar_app_shown(const void *state, int *year, int *month, int *day);
int lp_calendar_app_view(const void *state);
int lp_calendar_app_editing(const void *state);
int lp_calendar_app_event_count(const void *state);
const char *lp_calendar_app_message(const void *state);
void lp_calendar_app_fill(void *state, const char *title, const char *start_date, const char *start_time,
                          const char *end_date, const char *end_time, int all_day, int repeat);
void lp_calendar_app_edit(void *state, int index);

/* System Settings (app id "settings"): each command shows that pane. */
enum lp_prefs_command {
    LP_PREFS_GENERAL, LP_PREFS_DOCK, LP_PREFS_DISPLAYS, LP_PREFS_KEYBOARD, LP_PREFS_SOUND,
    LP_PREFS_NETWORK, LP_PREFS_TIME, LP_PREFS_USERS, LP_PREFS_ABOUT,
};
/* Its actions as a person takes them, and what it shows, for tests; job runs stand in for lp_job_run. */
void lp_prefs_set_runner(void *state, struct lp_job *(*run)(struct lp_desktop *d, const char *const *argv,
                        void (*done)(int status, const char *output, void *user), void *user));
void lp_prefs_set_layout(void *state, const char *layout);
void lp_prefs_set_zone(void *state, const char *zone);
void lp_prefs_set_hostname(void *state, const char *name);
void lp_prefs_join(void *state, int network, const char *passphrase);
void lp_prefs_set_volume(void *state, int volume);
int lp_prefs_pane(const void *state);
const char *lp_prefs_message(const void *state);
int lp_prefs_link_count(const void *state);
int lp_prefs_network_count(const void *state);
int lp_prefs_volume(const void *state);        /* 0 … 100, or -1 while unknown */
const char *lp_prefs_timezone(const void *state);

#endif
