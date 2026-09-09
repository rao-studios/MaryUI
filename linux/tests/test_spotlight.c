/* Spotlight's model: the dock, ranking, windows, selection, the keys. Case
 * names mirror web/src/desktop/spotlight.test.ts. */
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_spotlight.h"

static lp_desktop d;

static void setup(void) {
    lp_desktop_init(&d, LP_RECT(0, 24, 1280, 776), NULL);
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
    LP_ASSERT_EQ(n, 5);
    LP_ASSERT_STR(r[0].title, "Finder");
    LP_ASSERT_STR(r[1].title, "Gallery");
    LP_ASSERT_STR(r[2].title, "About");
    LP_ASSERT_STR(r[3].title, "TextEdit");
    LP_ASSERT_STR(r[4].title, "Terminal");
    LP_ASSERT_EQ(r[4].kind, LP_SPOT_COMMAND);
    LP_ASSERT_EQ(r[0].running, 1);
    LP_ASSERT_EQ(r[1].running, 0);
    for (int i = 0; i < n; i++) LP_ASSERT(r[i].kind != LP_SPOT_WINDOW);
    LP_ASSERT_EQ(results("   ", r), 5);
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
    LP_ASSERT_EQ(n, 3);
    LP_ASSERT_STR(r[0].title, "About");
    LP_ASSERT_STR(r[1].title, "Gallery");
    LP_ASSERT_STR(r[2].title, "Terminal");
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
    LP_ASSERT_EQ(d.spotlight.selection, 4);
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
    LP_TEST_MAIN_END();
}
