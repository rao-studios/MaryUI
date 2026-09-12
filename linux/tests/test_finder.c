/* The Finder, driven headlessly: EVENT passes with synthetic input over a
 * throwaway home folder. The body is 720×432 at the origin, so the list's
 * rows sit at y = 60 + 22·v (toolbar 40, header 20) from x = 180, and the
 * icon tiles at (192 + 104.8·k, 52). */
#define _DARWIN_C_SOURCE 1
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"

static char root[512], docs[600];
static lp_desktop d;
static lp_ctx ctx;              /* persists across passes like a chrome's */
static double now_ms = 1000;
static int drag_hook_calls;
static void *finder;            /* the state of the Finder under test */
static char finder_id[12];

static void on_drag(lp_desktop *desk, int begin) { drag_hook_calls++; }

static void write_file(const char *rel, const char *text) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fputs(text, f);
    fclose(f);
}
static int exists(const char *rel) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    struct stat st;
    return lstat(path, &st) == 0;
}

static void pass(void *state, lp_input in) {
    now_ms += 50;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 720, 432), now_ms);
    lp_app_finder.paint(state, &ctx, LP_RECT(0, 0, 720, 432), &d);
    lp_ctx_end(&ctx);
    ctx.in.mx = in.mx;
    ctx.in.my = in.my;
    ctx.in.buttons = in.buttons;
}
static void move(void *state, float x, float y) { pass(state, (lp_input){ .mx = x, .my = y }); }
static void click_at(void *state, float x, float y, uint32_t mods) {
    pass(state, (lp_input){ .mx = x, .my = y, .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT, .mods = mods });
    pass(state, (lp_input){ .mx = x, .my = y, .released = LP_BUTTON_LEFT, .mods = mods });
}
static void dblclick_at(void *state, float x, float y) {
    click_at(state, x, y, 0);
    pass(state, (lp_input){ .mx = x, .my = y, .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT, .double_click = 1 });
    pass(state, (lp_input){ .mx = x, .my = y, .released = LP_BUTTON_LEFT });
}
static void rclick_at(void *state, float x, float y) {
    pass(state, (lp_input){ .mx = x, .my = y, .buttons = LP_BUTTON_RIGHT, .pressed = LP_BUTTON_RIGHT });
    pass(state, (lp_input){ .mx = x, .my = y, .released = LP_BUTTON_RIGHT });
}
static void key(void *state, uint32_t sym, uint32_t mods, const char *utf8) {
    lp_input in = { .mx = NAN, .my = NAN, .keysym = sym, .mods = mods, .key_pressed = 1 };
    if (utf8) snprintf(in.utf8, sizeof in.utf8, "%s", utf8);
    pass(state, in);
    in.key_pressed = 0;
    pass(state, in);
}
static void type_text(void *state, const char *text) {
    for (; *text; text++) { char u[2] = { *text, 0 }; key(state, (uint32_t)(unsigned char)*text, 0, u); }
}
static float row_y(int v) { return 60 + 22 * v + 11; }
static float tile_x(int k) { return 192 + 104.8f * k + 48; }

static void open_finder(void) {
    memset(&ctx, 0, sizeof ctx);
    ctx.settings = &d.settings;
    ctx.active_window = 1;
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "finder", docs, "Documents", finder_id), 1);
    finder = lp_desktop_instance(&d, finder_id)->state;
    lp_app_finder.command(finder, &d, LP_FINDER_VIEW_LIST);
    move(finder, NAN, NAN);
}

static void reset_tree(void) {
    lp_files_delete_tree(root);
    mkdir(root, 0755);
    mkdir(docs, 0755);
    write_file("Documents/alpha.txt", "alpha");
    write_file("Documents/beta.txt", "beta beta");
    char notes[700];
    snprintf(notes, sizeof notes, "%s/Notes", docs);
    mkdir(notes, 0755);
    write_file("Documents/Notes/inner.txt", "inner");
    char target[700];
    snprintf(target, sizeof target, "%s/Target", root);
    mkdir(target, 0755);
}

static void setup(void) {
    reset_tree();
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    d.on_drag = on_drag;
    drag_hook_calls = 0;
    open_finder();
}

LP_TEST(opens_at_the_folder_and_lists_it_sorted) {
    setup();
    LP_ASSERT_STR(lp_finder_path(finder), docs);
    LP_ASSERT_STR(d.wm.windows[0].title, "Documents");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 3);
    LP_ASSERT_STR(lp_finder_visible_name(finder, 0), "alpha.txt");
    LP_ASSERT_STR(lp_finder_visible_name(finder, 1), "beta.txt");
    LP_ASSERT_STR(lp_finder_visible_name(finder, 2), "Notes");
    LP_ASSERT_EQ(lp_finder_view(finder), 1);
}

LP_TEST(double_click_enters_a_folder_and_back_forward_and_up_walk_the_history) {
    setup();
    dblclick_at(finder, 400, row_y(2));
    char notes[700];
    snprintf(notes, sizeof notes, "%s/Notes", docs);
    LP_ASSERT_STR(lp_finder_path(finder), notes);
    LP_ASSERT_STR(d.wm.windows[0].title, "Notes");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 1);
    click_at(finder, 21, 20, 0); /* Back */
    LP_ASSERT_STR(lp_finder_path(finder), docs);
    LP_ASSERT_STR(d.wm.windows[0].title, "Documents");
    click_at(finder, 43, 20, 0); /* Forward */
    LP_ASSERT_STR(lp_finder_path(finder), notes);
    key(finder, XKB_KEY_Up, LP_MOD_LOGO, NULL); /* Enclosing Folder selects where we came from */
    LP_ASSERT_STR(lp_finder_path(finder), docs);
    LP_ASSERT(lp_finder_is_selected(finder, "Notes"));
    key(finder, XKB_KEY_bracketleft, LP_MOD_LOGO, "[");
    LP_ASSERT_STR(lp_finder_path(finder), notes);
    key(finder, XKB_KEY_bracketright, LP_MOD_LOGO, "]");
    LP_ASSERT_STR(lp_finder_path(finder), docs);
    /* the sidebar's home item, and a crumb */
    click_at(finder, 90, 80, 0);
    LP_ASSERT_STR(lp_finder_path(finder), root);
    LP_ASSERT_STR(d.wm.windows[0].title, lp_files_basename(root));
}

LP_TEST(clicks_and_keys_move_the_selection) {
    setup();
    click_at(finder, 400, row_y(1), 0);
    LP_ASSERT(lp_finder_is_selected(finder, "beta.txt"));
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 1);
    click_at(finder, 400, row_y(2), LP_MOD_LOGO); /* ⌘-click adds */
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 2);
    click_at(finder, 400, row_y(0), LP_MOD_SHIFT); /* ⇧-click ranges from the anchor */
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 3);
    click_at(finder, 400, 300, 0); /* empty space clears */
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 0);
    key(finder, XKB_KEY_Down, 0, NULL);
    LP_ASSERT(lp_finder_is_selected(finder, "alpha.txt"));
    key(finder, XKB_KEY_Down, 0, NULL);
    LP_ASSERT(lp_finder_is_selected(finder, "beta.txt"));
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 1);
    key(finder, XKB_KEY_End, 0, NULL);
    LP_ASSERT(lp_finder_is_selected(finder, "Notes"));
    key(finder, XKB_KEY_Home, 0, NULL);
    LP_ASSERT(lp_finder_is_selected(finder, "alpha.txt"));
    key(finder, XKB_KEY_Down, LP_MOD_SHIFT, NULL);
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 2);
    type_text(finder, "no"); /* type-ahead */
    LP_ASSERT(lp_finder_is_selected(finder, "Notes"));
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 1);
    key(finder, XKB_KEY_a, LP_MOD_LOGO, "a");
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 3);
    key(finder, XKB_KEY_Escape, 0, NULL);
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 0);
    /* the marquee: press on empty space, sweep over two rows */
    pass(finder, (lp_input){ .mx = 400, .my = 300, .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT });
    pass(finder, (lp_input){ .mx = 410, .my = row_y(1) - 5, .buttons = LP_BUTTON_LEFT });
    pass(finder, (lp_input){ .mx = 410, .my = row_y(1) - 5, .released = LP_BUTTON_LEFT });
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 2);
    LP_ASSERT(lp_finder_is_selected(finder, "beta.txt"));
    LP_ASSERT(lp_finder_is_selected(finder, "Notes"));
    /* icon view: arrows move in two dimensions */
    lp_app_finder.command(finder, &d, LP_FINDER_VIEW_ICONS);
    click_at(finder, tile_x(0), 96, 0);
    LP_ASSERT(lp_finder_is_selected(finder, "alpha.txt"));
    key(finder, XKB_KEY_Right, 0, NULL);
    LP_ASSERT(lp_finder_is_selected(finder, "beta.txt"));
    key(finder, XKB_KEY_Right, 0, NULL);
    LP_ASSERT(lp_finder_is_selected(finder, "Notes"));
    key(finder, XKB_KEY_Down, 0, NULL); /* no row below: stays */
    LP_ASSERT(lp_finder_is_selected(finder, "Notes"));
    dblclick_at(finder, tile_x(2), 96);
    LP_ASSERT_STR(lp_files_basename(lp_finder_path(finder)), "Notes");
}

LP_TEST(double_click_opens_a_text_file_in_textedit) {
    setup();
    dblclick_at(finder, 400, row_y(0));
    LP_ASSERT_EQ(d.wm.count, 2);
    LP_ASSERT_STR(d.wm.windows[1].app_id, "textedit");
    LP_ASSERT_STR(d.wm.windows[1].title, "alpha.txt");
    /* ⌘O on a selection does the same; ⌘↓ too */
    lp_wm_action a = { .type = LP_WM_FOCUS, .id = finder_id };
    lp_desktop_dispatch(&d, &a);
    click_at(finder, 400, row_y(1), 0);
    key(finder, XKB_KEY_o, LP_MOD_LOGO, "o");
    LP_ASSERT_EQ(d.wm.count, 3);
    LP_ASSERT_STR(d.wm.windows[2].title, "beta.txt");
}

LP_TEST(the_search_filters_and_hidden_files_toggle) {
    setup();
    write_file("Documents/.secret", "");
    lp_desktop_files_changed(&d, docs);
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 3);
    click_at(finder, 600, 20, 0); /* the search field */
    type_text(finder, "be");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 1);
    LP_ASSERT_STR(lp_finder_visible_name(finder, 0), "beta.txt");
    key(finder, XKB_KEY_BackSpace, 0, "");
    key(finder, XKB_KEY_BackSpace, 0, "");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 3);
    click_at(finder, 400, 300, 0); /* back to the list */
    key(finder, XKB_KEY_greater, LP_MOD_LOGO | LP_MOD_SHIFT, ">");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 4);
    LP_ASSERT_STR(lp_finder_visible_name(finder, 0), ".secret");
    key(finder, XKB_KEY_period, LP_MOD_LOGO | LP_MOD_SHIFT, ".");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 3);
}

LP_TEST(the_list_header_sorts) {
    setup();
    float xs[16], ws[16];
    lp_list_columns(540, 3, xs, ws);
    click_at(finder, 180 + xs[2] + 10, 50, 0); /* Size */
    LP_ASSERT_STR(lp_finder_visible_name(finder, 0), "Notes");  /* folders first ascending */
    LP_ASSERT_STR(lp_finder_visible_name(finder, 2), "beta.txt");
    click_at(finder, 180 + xs[2] + 10, 50, 0); /* again: descending */
    LP_ASSERT_STR(lp_finder_visible_name(finder, 0), "beta.txt");
    click_at(finder, 180 + xs[0] + 10, 50, 0); /* Name */
    LP_ASSERT_STR(lp_finder_visible_name(finder, 0), "alpha.txt");
}

LP_TEST(new_folder_rename_and_trash) {
    setup();
    key(finder, XKB_KEY_N, LP_MOD_LOGO | LP_MOD_SHIFT, "N");
    LP_ASSERT(exists("Documents/untitled folder"));
    LP_ASSERT(lp_finder_renaming(finder));
    LP_ASSERT(lp_finder_is_selected(finder, "untitled folder"));
    type_text(finder, "Ideas"); /* the name starts selected: typing replaces it */
    key(finder, XKB_KEY_Return, 0, "\r");
    LP_ASSERT(!lp_finder_renaming(finder));
    LP_ASSERT(exists("Documents/Ideas"));
    LP_ASSERT(!exists("Documents/untitled folder"));
    LP_ASSERT(lp_finder_is_selected(finder, "Ideas"));
    /* Return renames the selection; Escape cancels */
    key(finder, XKB_KEY_Return, 0, "\r");
    LP_ASSERT(lp_finder_renaming(finder));
    type_text(finder, "X");
    key(finder, XKB_KEY_Escape, 0, "");

    LP_ASSERT(!lp_finder_renaming(finder));
    LP_ASSERT(exists("Documents/Ideas"));
    /* a click elsewhere commits */
    key(finder, XKB_KEY_Return, 0, "\r");
    type_text(finder, "Ideas2");
    click_at(finder, 400, 300, 0);
    LP_ASSERT(exists("Documents/Ideas2"));
    /* ⌘⌫ trashes */
    click_at(finder, 400, row_y(0), 0);
    LP_ASSERT(lp_finder_is_selected(finder, "alpha.txt"));
    key(finder, XKB_KEY_BackSpace, LP_MOD_LOGO, "");
    LP_ASSERT(!exists("Documents/alpha.txt"));
    LP_ASSERT(exists(".local/share/Trash/files/alpha.txt"));
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 3);
    /* the Trash in the sidebar shows it; Empty Trash clears it */
    lp_app_finder.command(finder, &d, LP_FINDER_GO_TRASH);
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 1);
    LP_ASSERT_STR(d.wm.windows[0].title, "Trash");
    lp_app_finder.command(finder, &d, LP_FINDER_EMPTY_TRASH);
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 0);
    LP_ASSERT(!exists(".local/share/Trash/files/alpha.txt"));
}

LP_TEST(copy_paste_duplicate_and_move) {
    setup();
    click_at(finder, 400, row_y(0), 0);
    key(finder, XKB_KEY_c, LP_MOD_LOGO, "c");
    dblclick_at(finder, 400, row_y(2)); /* into Notes */
    key(finder, XKB_KEY_v, LP_MOD_LOGO, "v");
    LP_ASSERT(exists("Documents/Notes/alpha.txt"));
    LP_ASSERT(exists("Documents/alpha.txt"));
    LP_ASSERT(lp_finder_is_selected(finder, "alpha.txt"));
    key(finder, XKB_KEY_d, LP_MOD_LOGO, "d");
    LP_ASSERT(exists("Documents/Notes/alpha copy.txt"));
    LP_ASSERT(lp_finder_is_selected(finder, "alpha copy.txt"));
    /* cut + paste moves */
    key(finder, XKB_KEY_x, LP_MOD_LOGO, "x");
    key(finder, XKB_KEY_Up, LP_MOD_LOGO, NULL);
    key(finder, XKB_KEY_v, LP_MOD_LOGO, "v");
    LP_ASSERT(exists("Documents/alpha copy.txt"));
    LP_ASSERT(!exists("Documents/Notes/alpha copy.txt"));
    LP_ASSERT_EQ(lp_files_clipboard_shared()->count, 0);
    /* ⌥⌘V moves a copied item */
    click_at(finder, 400, row_y(0), 0); /* alpha copy.txt sorts first */
    LP_ASSERT(lp_finder_is_selected(finder, "alpha copy.txt"));
    key(finder, XKB_KEY_c, LP_MOD_LOGO, "c");
    dblclick_at(finder, 400, row_y(3)); /* Notes is last now */
    LP_ASSERT_STR(lp_files_basename(lp_finder_path(finder)), "Notes");
    key(finder, XKB_KEY_v, LP_MOD_LOGO | LP_MOD_ALT, "v");
    LP_ASSERT(exists("Documents/Notes/alpha copy.txt"));
    LP_ASSERT(!exists("Documents/alpha copy.txt"));
}

LP_TEST(right_click_opens_a_context_menu_for_the_item) {
    setup();
    rclick_at(finder, 400, row_y(1));
    LP_ASSERT(lp_finder_is_selected(finder, "beta.txt"));
    LP_ASSERT_EQ(d.open_menu, LP_DESKTOP_MENU_POPUP);
    LP_ASSERT_STR(d.popup_window, finder_id);
    LP_ASSERT_STR(d.menus[LP_DESKTOP_MENU_POPUP].entries[0].label, "Open");
    LP_ASSERT_STR(d.menus[LP_DESKTOP_MENU_POPUP].entries[5].label, "Move to Trash");
    lp_desktop_key(&d, XKB_KEY_Down, 0);
    lp_desktop_key(&d, XKB_KEY_Return, 0); /* Open */
    LP_ASSERT_EQ(d.wm.count, 2);
    LP_ASSERT_STR(d.wm.windows[1].title, "beta.txt");
    lp_wm_action a = { .type = LP_WM_FOCUS, .id = finder_id };
    lp_desktop_dispatch(&d, &a);
    rclick_at(finder, 400, 300);
    LP_ASSERT_EQ(lp_finder_selected_count(finder), 0);
    LP_ASSERT_STR(d.menus[LP_DESKTOP_MENU_POPUP].entries[0].label, "New Folder");
    lp_desktop_key(&d, XKB_KEY_Escape, 0);
    /* the File menu follows the selection */
    click_at(finder, 400, row_y(0), 0);
    lp_desktop_build_menus(&d);
    LP_ASSERT_STR(d.menus[LP_MENU_FILE].entries[2].label, "Open");
    LP_ASSERT(!d.menus[LP_MENU_FILE].entries[2].disabled);
    LP_ASSERT_STR(d.menus[LP_MENU_GO].entries[0].label, "Back");
}

LP_TEST(a_change_broadcast_reloads_the_listing) {
    setup();
    write_file("Documents/gamma.txt", "new");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 3);
    lp_desktop_files_changed(&d, docs);   /* no input needed: notify re-reads */
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 4);
    LP_ASSERT_STR(lp_finder_visible_name(finder, 2), "gamma.txt");
    lp_desktop_files_changed(&d, "/elsewhere");
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 4);
}

LP_TEST(drags_files_and_drops_them_on_folders_windows_and_the_trash) {
    setup();
    /* press alpha.txt, move 10 px with the button held: a drag begins with the selection */
    pass(finder, (lp_input){ .mx = 400, .my = row_y(0), .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT });
    pass(finder, (lp_input){ .mx = 410, .my = row_y(0) + 2, .buttons = LP_BUTTON_LEFT });
    LP_ASSERT(d.drag.active);
    LP_ASSERT_EQ(d.drag.count, 1);
    LP_ASSERT_STR(d.drag.names[0], "alpha.txt");
    LP_ASSERT_STR(d.drag.src_dir, docs);
    LP_ASSERT_STR(d.drag.source_window, finder_id);
    LP_ASSERT_EQ(drag_hook_calls, 1);
    /* hovering the Notes row targets it; dropping moves the file */
    pass(finder, (lp_input){ .mx = 400, .my = row_y(2), .buttons = LP_BUTTON_LEFT, .drag = LP_DRAG_HOVER });
    pass(finder, (lp_input){ .mx = 400, .my = row_y(2), .drag = LP_DRAG_DROP });
    lp_desktop_drag_end(&d);
    LP_ASSERT(exists("Documents/Notes/alpha.txt"));
    LP_ASSERT(!exists("Documents/alpha.txt"));
    move(finder, NAN, NAN);
    LP_ASSERT_EQ(lp_finder_visible_count(finder), 2);
    /* a second window at Target: a drop on its empty content area moves there; with Alt it copies */
    char target[700], id2[12];
    snprintf(target, sizeof target, "%s/Target", root);
    lp_desktop_open_app_with(&d, "finder", target, "Target", id2);
    void *second = lp_desktop_instance(&d, id2)->state;
    lp_app_finder.command(second, &d, LP_FINDER_VIEW_LIST);
    pass(finder, (lp_input){ .mx = 400, .my = row_y(0), .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT }); /* beta.txt */
    pass(finder, (lp_input){ .mx = 420, .my = row_y(0), .buttons = LP_BUTTON_LEFT });
    LP_ASSERT(d.drag.active);
    pass(finder, (lp_input){ .mx = NAN, .my = NAN, .drag = LP_DRAG_LEAVE });
    pass(second, (lp_input){ .mx = 400, .my = 300, .buttons = LP_BUTTON_LEFT, .mods = LP_MOD_ALT, .drag = LP_DRAG_HOVER });
    LP_ASSERT(d.drag.copy);
    pass(second, (lp_input){ .mx = 400, .my = 300, .mods = LP_MOD_ALT, .drag = LP_DRAG_DROP });
    lp_desktop_drag_end(&d);
    LP_ASSERT(exists("Target/beta.txt"));
    LP_ASSERT(exists("Documents/beta.txt"));
    LP_ASSERT_EQ(lp_finder_visible_count(second), 1);
    /* a drop onto the window's own folder is a no-op */
    pass(finder, (lp_input){ .mx = 400, .my = row_y(0), .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT });
    pass(finder, (lp_input){ .mx = 420, .my = row_y(0), .buttons = LP_BUTTON_LEFT });
    pass(finder, (lp_input){ .mx = 400, .my = 300, .buttons = LP_BUTTON_LEFT, .drag = LP_DRAG_HOVER });
    pass(finder, (lp_input){ .mx = 400, .my = 300, .drag = LP_DRAG_DROP });
    lp_desktop_drag_end(&d);
    LP_ASSERT(exists("Documents/beta.txt"));
    /* a drop onto the sidebar's Trash trashes */
    int nside = lp_finder_sidebar_count(finder);
    float trash_y = 48 + 20 + 24 * 4 + 12 + 20 + 24 * (nside - 5) + 12;
    pass(finder, (lp_input){ .mx = 400, .my = row_y(0), .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT });
    pass(finder, (lp_input){ .mx = 420, .my = row_y(0), .buttons = LP_BUTTON_LEFT });
    LP_ASSERT(d.drag.active);
    pass(finder, (lp_input){ .mx = 90, .my = trash_y, .buttons = LP_BUTTON_LEFT, .drag = LP_DRAG_HOVER });
    pass(finder, (lp_input){ .mx = 90, .my = trash_y, .drag = LP_DRAG_DROP });
    lp_desktop_drag_end(&d);
    LP_ASSERT(!exists("Documents/beta.txt"));
    LP_ASSERT(exists(".local/share/Trash/files/beta.txt"));
    LP_ASSERT(!d.drag.active);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_finder_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    snprintf(docs, sizeof docs, "%s/Documents", root);
    setenv("HOME", root, 1);
    char xdg[600];
    snprintf(xdg, sizeof xdg, "%s/.local/share", root);
    setenv("XDG_DATA_HOME", xdg, 1);
    LP_RUN(opens_at_the_folder_and_lists_it_sorted);
    LP_RUN(double_click_enters_a_folder_and_back_forward_and_up_walk_the_history);
    LP_RUN(clicks_and_keys_move_the_selection);
    LP_RUN(double_click_opens_a_text_file_in_textedit);
    LP_RUN(the_search_filters_and_hidden_files_toggle);
    LP_RUN(the_list_header_sorts);
    LP_RUN(new_folder_rename_and_trash);
    LP_RUN(copy_paste_duplicate_and_move);
    LP_RUN(right_click_opens_a_context_menu_for_the_item);
    LP_RUN(a_change_broadcast_reloads_the_listing);
    LP_RUN(drags_files_and_drops_them_on_folders_windows_and_the_trash);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
