/* The app commands' models: what each menu says and what it does, as data.
 * Mirrors web/src/desktop/menus.ts. There is no menu bar on either side any
 * more — lp_desktop builds these and the Spotlight panel renders them as the
 * pills under its dock (components/lp_spotlight_panel.h); the Finder's context
 * menu is the one that still opens as a floating panel (lp_menu_item.h). */
#ifndef MARYUI_LP_MENUS_H
#define MARYUI_LP_MENUS_H

#define LP_MENU_MAX_ENTRIES 24

typedef struct lp_menu_entry {
    int separator;
    char label[64];
    const char *shortcut;
    int checked, disabled;
    int command;      /* the desktop's command id */
    int arg;
} lp_menu_entry;

typedef struct lp_menu_model {
    const char *id;
    const char *label;
    lp_menu_entry entries[LP_MENU_MAX_ENTRIES];
    int count;
} lp_menu_model;

/* menus.ts isSeparator. */
static inline int lp_menu_is_separator(const lp_menu_entry *e) { return e->separator; }

#endif
