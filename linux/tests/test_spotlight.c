/* Spotlight's model: the dock, ranking, windows, selection, the keys, and the
 * app commands it carries now that there is no menu bar. The first nine case
 * names mirror web/src/desktop/spotlight.test.ts; the rest are C-only. */
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_spotlight.h"

static lp_desktop d;

static void setup(void) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
}

static int results(const char *q, lp_spotlight_item *out) {
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS];
    int n = lp_spotlight_items(&d, items, LP_SPOTLIGHT_MAX_ITEMS);
    return lp_spotlight_results(items, n, q, out, LP_SPOTLIGHT_MAX_RESULTS);
}

LP_TEST(empty_query_shows_dock) {
    setup();
    lp_desktop_open_app(&d, "finder");
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = results("", r);
    LP_ASSERT_EQ(n, 4);   /* the pinned apps only: Gallery, About and Calculator are found by typing */
    LP_ASSERT_STR(r[0].title, "Finder");
    LP_ASSERT_STR(r[1].title, "TextEdit");
    LP_ASSERT_STR(r[2].title, "Preview");
    LP_ASSERT_STR(r[3].title, "Terminal");
    LP_ASSERT_EQ(r[3].kind, LP_SPOT_COMMAND);
    LP_ASSERT_EQ(r[0].running, 1);
    LP_ASSERT_EQ(r[1].running, 0);
    for (int i = 0; i < n; i++) LP_ASSERT(r[i].kind != LP_SPOT_WINDOW);
    LP_ASSERT_EQ(results("   ", r), 4);
}

LP_TEST(filters_apps_by_title_prefix_and_substring) {
    setup();
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = results("te", r);
    LP_ASSERT_EQ(n, 2);
    LP_ASSERT_STR(r[0].title, "TextEdit");
    LP_ASSERT_STR(r[1].title, "Terminal");
    n = results("edit", r);
    LP_ASSERT_EQ(n, 1);
    LP_ASSERT_STR(r[0].title, "TextEdit");
    LP_ASSERT_STR(r[0].subtitle, "Application");
}

LP_TEST(ranks_prefix_matches_first) {
    setup();
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = results("a", r);
    LP_ASSERT_EQ(n, 4);
    LP_ASSERT_STR(r[0].title, "About");
    LP_ASSERT_STR(r[1].title, "Gallery");
    LP_ASSERT_STR(r[2].title, "Calculator");
    LP_ASSERT_STR(r[3].title, "Terminal");
}

LP_TEST(is_case_insensitive) {
    setup();
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = results("GAL", r);
    LP_ASSERT_EQ(n, 1);
    LP_ASSERT_STR(r[0].title, "Gallery");
}

LP_TEST(includes_open_windows) {
    setup();
    lp_desktop_open_app(&d, "finder");
    /* A Finder window is titled after the folder it shows, so ask the record what to search for. */
    char title[128];
    snprintf(title, sizeof title, "%s", d.wm.windows[0].title);
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = results(title, r);
    LP_ASSERT_EQ(n, 1);
    LP_ASSERT_EQ(r[0].kind, LP_SPOT_WINDOW);
    LP_ASSERT_STR(r[0].id, "w1");
    LP_ASSERT_STR(r[0].title, title);
    LP_ASSERT_STR(r[0].subtitle, "Window · Finder");
}

LP_TEST(wraps_selection) {
    lp_spotlight s;
    lp_spotlight_init(&s);
    s.selection = 4;
    lp_spotlight_move(&s, 1, 5);
    LP_ASSERT_EQ(s.selection, 0);
    lp_spotlight_move(&s, -1, 5);
    LP_ASSERT_EQ(s.selection, 4);
    lp_spotlight_move(&s, 1, 0);
    LP_ASSERT_EQ(s.selection, 0);
}

LP_TEST(resets_the_selection_when_the_query_changes) {
    lp_spotlight s;
    lp_spotlight_init(&s);
    lp_spotlight_open(&s);
    s.selection = 3;
    lp_spotlight_set_query(&s, "te");
    LP_ASSERT_EQ(s.selection, 0);
    LP_ASSERT_STR(s.query.text, "te");
}

LP_TEST(toggle_opens_with_an_empty_query_and_closes) {
    lp_spotlight s;
    lp_spotlight_init(&s);
    lp_spotlight_set_query(&s, "stale");
    s.selection = 2;
    lp_spotlight_toggle(&s);
    LP_ASSERT(s.open);
    LP_ASSERT_STR(s.query.text, "");
    LP_ASSERT_EQ(s.selection, 0);
    lp_spotlight_toggle(&s);
    LP_ASSERT(!s.open);
}

LP_TEST(caps_results_at_eight) {
    setup();
    for (int i = 0; i < 10; i++) lp_desktop_open_app(&d, "finder");
    LP_ASSERT_EQ(d.wm.count, 10);
    char title[128];
    snprintf(title, sizeof title, "%s", d.wm.windows[0].title);
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    LP_ASSERT_EQ(results(title, r), LP_SPOTLIGHT_MAX_RESULTS);
}

LP_TEST(ctrl_space_toggles_and_escape_closes) {
    setup();
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_space, LP_MOD_CTRL), 1);
    LP_ASSERT(d.spotlight.open);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Down, 0), 1);
    LP_ASSERT_EQ(d.spotlight.selection, 1);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Up, 0), 1);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Up, 0), 1);
    LP_ASSERT_EQ(d.spotlight.selection, 3);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_x, 0), 0); /* the bar's */
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_w, LP_MOD_CTRL), 0); /* window shortcuts are suspended */
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Escape, 0), 1);
    LP_ASSERT(!d.spotlight.open);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_space, LP_MOD_LOGO), 1);
    LP_ASSERT(d.spotlight.open);
    lp_desktop_key(&d, XKB_KEY_space, LP_MOD_LOGO);
    LP_ASSERT(!d.spotlight.open);
}

LP_TEST(enter_activates_the_selection) {
    setup();
    lp_desktop_key(&d, XKB_KEY_space, LP_MOD_CTRL);
    lp_spotlight_set_query(&d.spotlight, "textedit");
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, 0), 1);
    LP_ASSERT(!d.spotlight.open);
    LP_ASSERT_EQ(d.wm.count, 1);
    LP_ASSERT_STR(d.wm.windows[0].app_id, "textedit");
    LP_ASSERT_STR(d.wm.windows[0].title, "Untitled");
    /* hidden from Window › Open …, but registered */
    lp_desktop_build_menus(&d);
    for (int i = 0; i < d.menus[LP_MENU_WINDOW].count; i++) LP_ASSERT(strcmp(d.menus[LP_MENU_WINDOW].entries[i].label, "Open Untitled") != 0);
    LP_ASSERT(lp_desktop_find_app(&d, "textedit") != NULL);
    /* Enter on a window result focuses it */
    lp_desktop_open_app(&d, "finder");
    LP_ASSERT_EQ(d.wm.focused, 1);
    lp_desktop_key(&d, XKB_KEY_space, LP_MOD_CTRL);
    lp_spotlight_set_query(&d.spotlight, "untitled");
    lp_desktop_key(&d, XKB_KEY_Return, 0);
    LP_ASSERT_EQ(d.wm.focused, 0);
}

static const lp_app terminal_app = { .id = "terminal", .title = "Terminal", .name = "Terminal", .icon = LP_ICON_TERMINAL, .dock = 1, .resizable = 1 };

LP_TEST(pins_the_dock_and_finds_the_rest_by_typing) {
    setup();
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = results("", r);
    for (int i = 0; i < n; i++) {
        LP_ASSERT(r[i].dock);
        LP_ASSERT(strcmp(r[i].title, "Gallery") != 0);
    }
    LP_ASSERT_EQ(results("gal", r), 1);
    LP_ASSERT_STR(r[0].title, "Gallery");
    LP_ASSERT_EQ(r[0].dock, 0);
}

LP_TEST(a_terminal_app_replaces_the_terminal_command) {
    setup();
    lp_desktop_register_app(&d, &terminal_app);
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    LP_ASSERT_EQ(results("", r), 4);
    LP_ASSERT_STR(r[3].title, "Terminal");
    LP_ASSERT_EQ(r[3].kind, LP_SPOT_APP);
    LP_ASSERT_EQ(results("term", r), 1);
    lp_desktop_key(&d, XKB_KEY_space, LP_MOD_CTRL);
    lp_spotlight_set_query(&d.spotlight, "term");
    lp_desktop_key(&d, XKB_KEY_Return, 0);
    LP_ASSERT_EQ(d.wm.count, 1);
    LP_ASSERT_STR(d.wm.windows[0].app_id, "terminal");
}

/* MARK: - The commands, folded into the panel */

static void open_spotlight(void) {
    setup();
    lp_desktop_open_app(&d, "gallery");
    lp_desktop_key(&d, XKB_KEY_space, LP_MOD_CTRL);
    LP_ASSERT(d.spotlight.open);
}

LP_TEST(command_pills_open_switch_and_close) {
    open_spotlight();
    lp_desktop_toggle_menu(&d, LP_MENU_FILE);
    LP_ASSERT_EQ(d.open_menu, LP_MENU_FILE);
    LP_ASSERT(d.spotlight.open);      /* the pills live inside the panel: it stays up */
    lp_desktop_toggle_menu(&d, LP_MENU_VIEW);
    LP_ASSERT_EQ(d.open_menu, LP_MENU_VIEW);
    LP_ASSERT(d.spotlight.open);
    lp_desktop_toggle_menu(&d, LP_MENU_VIEW);
    LP_ASSERT_EQ(d.open_menu, -1);
    LP_ASSERT(d.spotlight.open);
}

LP_TEST(typing_closes_the_open_command_menu) {
    open_spotlight();
    lp_desktop_toggle_menu(&d, LP_MENU_FILE);
    lp_spotlight_set_query(&d.spotlight, "te");
    lp_desktop_spotlight_query_changed(&d);
    LP_ASSERT_EQ(d.open_menu, -1);    /* the commands only show for a blank query */
    LP_ASSERT_EQ(d.spotlight.selection, 0);
    lp_spotlight_set_query(&d.spotlight, "");
    lp_desktop_spotlight_query_changed(&d);
    LP_ASSERT_EQ(d.open_menu, -1);    /* and blanking it does not bring the menu back */
}

LP_TEST(escape_closes_the_command_menu_before_spotlight) {
    open_spotlight();
    lp_desktop_toggle_menu(&d, LP_MENU_FILE);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Escape, 0), 1);
    LP_ASSERT_EQ(d.open_menu, -1);
    LP_ASSERT(d.spotlight.open);      /* the pill went, the panel stayed */
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Escape, 0), 1);
    LP_ASSERT(!d.spotlight.open);
}

LP_TEST(arrows_walk_the_open_command_menu) {
    open_spotlight();
    /* With no pill open the arrows still step the dock. */
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Right, 0), 1);
    LP_ASSERT_EQ(d.spotlight.selection, 1);
    lp_desktop_toggle_menu(&d, LP_MENU_FILE);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Down, 0), 1);
    LP_ASSERT_EQ(d.menu_active, 0);
    LP_ASSERT_EQ(d.spotlight.selection, 1);   /* the menu took it, not the dock */
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Right, 0), 1);
    LP_ASSERT_EQ(d.open_menu, LP_MENU_EDIT);  /* ←/→ step between pills */
    /* A character still reaches the bar rather than being swallowed. */
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_x, 0), 0);
}

LP_TEST(choosing_a_command_runs_it_and_closes_spotlight) {
    open_spotlight();
    int goo = d.settings.goo;
    lp_desktop_toggle_menu(&d, LP_MENU_VIEW);
    int entry = -1;
    for (int i = 0; i < d.menus[LP_MENU_VIEW].count; i++) {
        if (strcmp(d.menus[LP_MENU_VIEW].entries[i].label, "Liquid Merge") == 0) entry = i;
    }
    LP_ASSERT(entry >= 0);
    d.menu_active = entry;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, 0), 1);
    LP_ASSERT_EQ(d.settings.goo, !goo);
    LP_ASSERT_EQ(d.open_menu, -1);
    LP_ASSERT(!d.spotlight.open);
}

LP_TEST(the_view_names_the_frontmost_app) {
    open_spotlight();
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_desktop_spotlight_results(&d, r, LP_SPOTLIGHT_MAX_RESULTS);
    lp_spotlight_view v = lp_desktop_spotlight_view(&d, r, n);
    LP_ASSERT_STR(v.context_name, "Gallery");
    LP_ASSERT_EQ(v.menu_count, LP_DESKTOP_MENU_COUNT);
    LP_ASSERT_EQ(v.open_menu, -1);
    /* The Finder's context menu is a floating panel, never one of the pills. */
    d.open_menu = LP_DESKTOP_MENU_POPUP;
    v = lp_desktop_spotlight_view(&d, r, n);
    LP_ASSERT_EQ(v.open_menu, -1);
}

LP_TEST(the_panel_never_reallocates_while_typing) {
    open_spotlight();
    lp_spotlight_item r[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_desktop_spotlight_results(&d, r, LP_SPOTLIGHT_MAX_RESULTS);
    lp_spotlight_view v = lp_desktop_spotlight_view(&d, r, n);
    lp_size max = lp_spotlight_max_size(&v);
    /* The dock with its pills fits the one buffer the host allocates at open. */
    LP_ASSERT(lp_spotlight_measure(&v).h <= max.h);
    /* So does a full page of results, which is what typing produces. */
    lp_spotlight_set_query(&d.spotlight, "e");
    lp_spotlight_view typed = v;
    typed.count = LP_SPOTLIGHT_MAX_RESULTS;
    LP_ASSERT(lp_spotlight_measure(&typed).h <= max.h);
    lp_spotlight_set_query(&d.spotlight, "");
    /* An open menu is the one thing that does not fit: the host resizes for it. */
    v.open_menu = LP_MENU_VIEW;
    LP_ASSERT(lp_spotlight_measure(&v).h > max.h);
}

int main(void) {
    LP_RUN(empty_query_shows_dock);
    LP_RUN(filters_apps_by_title_prefix_and_substring);
    LP_RUN(ranks_prefix_matches_first);
    LP_RUN(is_case_insensitive);
    LP_RUN(includes_open_windows);
    LP_RUN(wraps_selection);
    LP_RUN(resets_the_selection_when_the_query_changes);
    LP_RUN(toggle_opens_with_an_empty_query_and_closes);
    LP_RUN(caps_results_at_eight);
    LP_RUN(ctrl_space_toggles_and_escape_closes);
    LP_RUN(enter_activates_the_selection);
    LP_RUN(pins_the_dock_and_finds_the_rest_by_typing);
    LP_RUN(a_terminal_app_replaces_the_terminal_command);
    LP_RUN(command_pills_open_switch_and_close);
    LP_RUN(typing_closes_the_open_command_menu);
    LP_RUN(escape_closes_the_command_menu_before_spotlight);
    LP_RUN(arrows_walk_the_open_command_menu);
    LP_RUN(choosing_a_command_runs_it_and_closes_spotlight);
    LP_RUN(the_view_names_the_frontmost_app);
    LP_RUN(the_panel_never_reallocates_while_typing);
    LP_TEST_MAIN_END();
}
