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
    lp_icon icon;           /* the Spotlight tile (LP_ICON_COUNT: a document) */
    const char *object;     /* the object-tier mark when it is not the glyph's own name (lp_objects.h) */
    int hidden;             /* reachable from Spotlight only: not in Window › Open … */
    int internal;           /* not in Spotlight either: opened by other apps (Info) */
    int dock;               /* pinned: in Spotlight's dock (a blank query); the rest are found by typing */
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

#endif
