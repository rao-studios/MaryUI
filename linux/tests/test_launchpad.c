/* The Launchpad (PARITY D32): every application on a grid, alphabetical until typed at, walked by the
 * arrows, launched by Return, closed by Esc or the scrim; Spotlight steps aside when it opens. */
#include <cairo.h>
#include <string.h>
#include <strings.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "lp_test.h"
#include "maryui/lp_launchpad.h"
#include "maryui/lp_desktop.h"

static lp_desktop d;

static void setup(void) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
}

LP_TEST(every_app_is_listed_alphabetically_and_typing_ranks_them) {
    setup();
    lp_desktop_launchpad_open(&d);
    LP_ASSERT(d.launchpad.open);
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS];
    int n = lp_desktop_launchpad_items(&d, items, LP_SPOTLIGHT_MAX_ITEMS);
    int apps = 0;
    for (int i = 0; i < d.app_count; i++) apps += !d.apps[i]->internal;
    LP_ASSERT_EQ(n, apps);                       /* the internal ones (Info, "From the thread", the desktop) never show */
    for (int i = 1; i < n; i++) LP_ASSERT(strcasecmp(items[i - 1].title, items[i].title) <= 0);
    for (int i = 0; i < n; i++) LP_ASSERT(items[i].kind != LP_SPOT_WINDOW);
    lp_text_buffer_set(&d.launchpad.query, "te");
    n = lp_desktop_launchpad_items(&d, items, LP_SPOTLIGHT_MAX_ITEMS);
    LP_ASSERT_EQ(n, 3);                          /* the two that start with it, then Settings through "System Settings" */
    LP_ASSERT_STR(items[0].title, "Terminal");
    LP_ASSERT_STR(items[1].title, "TextEdit");
    LP_ASSERT_STR(items[2].title, "Settings");
}

LP_TEST(opening_it_closes_spotlight_and_the_keys_walk_the_grid) {
    setup();
    lp_desktop_key(&d, XKB_KEY_space, LP_MOD_SHIFT);
    LP_ASSERT(d.spotlight.open);
    lp_desktop_launchpad_open(&d);
    LP_ASSERT(!d.spotlight.open);
    LP_ASSERT_EQ(d.launchpad.selection, 0);
    int cols = lp_launchpad_columns(1280);
    LP_ASSERT_EQ(cols, 7);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Right, 0), 1);
    LP_ASSERT_EQ(d.launchpad.selection, 1);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Down, 0), 1);
    LP_ASSERT_EQ(d.launchpad.selection, 1 + cols);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Up, 0), 1);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Left, 0), 1);
    LP_ASSERT_EQ(d.launchpad.selection, 0);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Left, 0), 1);        /* stays put at the first */
    LP_ASSERT_EQ(d.launchpad.selection, 0);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_a, 0), 0);           /* typing is the field's */
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Escape, 0), 1);
    LP_ASSERT(!d.launchpad.open);
}

LP_TEST(return_launches_the_selected_app_and_closes_the_grid) {
    setup();
    lp_desktop_launchpad_open(&d);
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS];
    int n = lp_desktop_launchpad_items(&d, items, LP_SPOTLIGHT_MAX_ITEMS);
    int calc = -1;
    for (int i = 0; i < n; i++) if (strcmp(items[i].id, "calculator") == 0) calc = i;
    LP_ASSERT(calc >= 0);
    d.launchpad.selection = calc;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, 0), 1);
    LP_ASSERT(!d.launchpad.open);
    const lp_window_record *front = lp_wm_focused(&d.wm);
    LP_ASSERT(front != NULL);
    LP_ASSERT_STR(front->app_id, "calculator");
    /* Shift+Space over the grid (its query empty) hands over to Spotlight */
    lp_desktop_launchpad_open(&d);
    lp_desktop_key(&d, XKB_KEY_space, LP_MOD_SHIFT);
    LP_ASSERT(!d.launchpad.open && d.spotlight.open);
}

LP_TEST(the_panel_lays_the_tiles_out_and_a_click_on_the_scrim_dismisses) {
    setup();
    lp_desktop_launchpad_open(&d);
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS];
    int n = lp_desktop_launchpad_items(&d, items, LP_SPOTLIGHT_MAX_ITEMS);
    lp_launchpad_view v = { .query = &d.launchpad.query, .items = items, .count = n, .selection = 0, .width = 1280, .height = 800 };
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    lp_launchpad_result res;
    /* a press on the scrim's corner, far from the field and the grid */
    lp_input in = { 0 };
    in.mx = 8; in.my = 790; in.pressed = LP_BUTTON_LEFT; in.buttons = LP_BUTTON_LEFT;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 1280, 800), 0);
    lp_launchpad_panel(&ctx, &v, &res);
    lp_ctx_end(&ctx);
    LP_ASSERT_EQ(res.dismissed, 1);
    LP_ASSERT_EQ(res.columns, 7);
    LP_ASSERT_EQ(res.pages, 1);
    /* the draw pass paints without a backdrop and without a crash */
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1280, 800);
    cairo_t *cr = cairo_create(s);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, 1280, 800), 0);
    lp_launchpad_panel(&ctx, &v, &res);
    lp_ctx_end(&ctx);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

int main(void) {
    LP_RUN(every_app_is_listed_alphabetically_and_typing_ranks_them);
    LP_RUN(opening_it_closes_spotlight_and_the_keys_walk_the_grid);
    LP_RUN(return_launches_the_selected_app_and_closes_the_grid);
    LP_RUN(the_panel_lays_the_tiles_out_and_a_click_on_the_scrim_dismisses);
    LP_TEST_MAIN_END();
}
