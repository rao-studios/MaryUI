/* The desktop's app hooks: opening a path, the popup, per-app menu entries,
 * change broadcasts, and the shortcuts an app keeps for itself. */
#define _DARWIN_C_SOURCE 1
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"

static char root[512];
static lp_desktop d;

/* A stub app that records what the desktop hands it. */
static char opened_path[1024];
static int last_command = -1, notified = 0, dirty_calls = 0, drag_calls = 0;
static char dirty_id[12];
static void *stub_create(lp_desktop *desk, const char *id) { return strdup(id); }
static void stub_destroy(void *s) { free(s); }
static void stub_paint(void *s, lp_ctx *ctx, lp_rect body, lp_desktop *desk) {}
static void stub_open(void *s, lp_desktop *desk, const char *path) { snprintf(opened_path, sizeof opened_path, "%s", path); }
static void stub_command(void *s, lp_desktop *desk, int cmd) { last_command = cmd; }
static void stub_menu(void *s, lp_desktop *desk, int menu, lp_menu_model *m) {
    if (menu == LP_MENU_FILE) { lp_menu_entry *e = &m->entries[m->count++]; memset(e, 0, sizeof *e); snprintf(e->label, sizeof e->label, "Stub Open"); e->command = LP_CMD_APP; e->arg = 42; }
    if (menu == LP_MENU_GO) { lp_menu_entry *e = &m->entries[m->count++]; memset(e, 0, sizeof *e); snprintf(e->label, sizeof e->label, "Stub Go"); e->command = LP_CMD_APP; e->arg = 7; }
}
static int stub_notify(void *s, lp_desktop *desk, const char *dir) { notified++; return 1; }
static const lp_app stub = {
    .id = "stub", .title = "Stub", .name = "Stub", .icon = LP_ICON_STAR, .hidden = 1, .internal = 1,
    .default_rect = { 10, 40, 300, 200 }, .resizable = 1,
    .create = stub_create, .paint = stub_paint, .destroy = stub_destroy,
    .open = stub_open, .command = stub_command, .menu_entries = stub_menu, .notify = stub_notify,
};
static void on_dirty(lp_desktop *desk, const char *id) { dirty_calls++; snprintf(dirty_id, sizeof dirty_id, "%s", id); }
static void on_drag(lp_desktop *desk, int begin) { drag_calls += begin ? 1 : 10; }
static char spawned[64];
static void on_spawn(lp_desktop *desk, const char *command) { snprintf(spawned, sizeof spawned, "%s", command); }
static lp_app preview_app, media_app, terminal_app;   /* the stub under the ids the desktop routes to */

static void write_file(const char *rel, const char *text, size_t n) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    FILE *f = fopen(path, "wb");
    fwrite(text, 1, n, f);
    fclose(f);
}

static void setup(void) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_desktop_register_app(&d, &stub);
    d.on_app_dirty = on_dirty;
    d.on_drag = on_drag;
    opened_path[0] = 0;
    last_command = -1;
    notified = dirty_calls = drag_calls = 0;
}

LP_TEST(opens_a_folder_in_the_finder_and_a_text_file_in_textedit) {
    setup();
    char docs[600];
    snprintf(docs, sizeof docs, "%s/Documents", root);
    LP_ASSERT_EQ(lp_desktop_open_path(&d, docs), 1);
    LP_ASSERT_EQ(d.wm.count, 1);
    LP_ASSERT_STR(d.wm.windows[0].app_id, "finder");
    LP_ASSERT_STR(d.wm.windows[0].title, "Documents");
    char note[600];
    snprintf(note, sizeof note, "%s/Documents/note.md", root);
    LP_ASSERT_EQ(lp_desktop_open_path(&d, note), 1);
    LP_ASSERT_EQ(d.wm.count, 2);
    LP_ASSERT_STR(d.wm.windows[1].app_id, "textedit");
    LP_ASSERT_STR(d.wm.windows[1].title, "note.md");
    char png[600];
    snprintf(png, sizeof png, "%s/Documents/photo.png", root);
    LP_ASSERT_EQ(lp_desktop_open_path(&d, png), 0);
    LP_ASSERT_EQ(lp_desktop_open_path(&d, "/nowhere/at/all"), 0);
    LP_ASSERT_EQ(d.wm.count, 2);
    LP_ASSERT_STR(d.pending_open, "");
}

LP_TEST(routes_pictures_and_media_to_their_viewers_once_registered) {
    setup();
    write_file("Documents/photo.png", "\x89PNG", 4);
    write_file("Documents/paper.pdf", "%PDF-1.4", 8);
    write_file("Documents/song.mp3", "ID3", 3);
    write_file("Documents/clip.mp4", "", 0);
    char png[600], pdf[600], mp3[600], mp4[600];
    snprintf(png, sizeof png, "%s/Documents/photo.png", root);
    snprintf(pdf, sizeof pdf, "%s/Documents/paper.pdf", root);
    snprintf(mp3, sizeof mp3, "%s/Documents/song.mp3", root);
    snprintf(mp4, sizeof mp4, "%s/Documents/clip.mp4", root);
    LP_ASSERT_EQ(lp_desktop_open_path(&d, mp4), 0);   /* nothing plays it yet */
    preview_app = stub; preview_app.id = "preview"; preview_app.title = "Preview";
    media_app = stub; media_app.id = "media"; media_app.title = "Media Player";
    lp_desktop_register_app(&d, &preview_app);
    lp_desktop_register_app(&d, &media_app);
    const char *const paths[4] = { png, pdf, mp3, mp4 };
    const char *const apps[4] = { "preview", "preview", "media", "media" };
    for (int i = 0; i < 4; i++) {
        LP_ASSERT_EQ(lp_desktop_open_path(&d, paths[i]), 1);
        LP_ASSERT_STR(d.wm.windows[d.wm.count - 1].app_id, apps[i]);
        LP_ASSERT_STR(opened_path, paths[i]);
    }
    LP_ASSERT_EQ(d.wm.count, 4);
}

LP_TEST(new_terminal_runs_foot_until_a_terminal_app_is_registered) {
    setup();
    d.spawn = on_spawn;
    spawned[0] = 0;
    LP_ASSERT_EQ(lp_desktop_run_command(&d, LP_CMD_NEW_TERMINAL, 0), 1);
    LP_ASSERT_STR(spawned, "foot");
    LP_ASSERT_EQ(d.wm.count, 0);
    terminal_app = stub; terminal_app.id = "terminal"; terminal_app.title = "Terminal";
    lp_desktop_register_app(&d, &terminal_app);
    spawned[0] = 0;
    LP_ASSERT_EQ(lp_desktop_run_command(&d, LP_CMD_NEW_TERMINAL, 0), 1);
    LP_ASSERT_STR(spawned, "");
    LP_ASSERT_EQ(d.wm.count, 1);
    LP_ASSERT_STR(d.wm.windows[0].app_id, "terminal");
}

LP_TEST(hands_the_path_to_the_app_open_hook_with_the_window_id) {
    setup();
    char id[12] = "";
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "stub", "/some/where", "Somewhere", id), 1);
    LP_ASSERT_STR(opened_path, "/some/where");
    LP_ASSERT_STR(id, "w1");
    LP_ASSERT_STR(d.wm.windows[0].title, "Somewhere");
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "nope", NULL, NULL, NULL), 0);
    /* a singleton brought forward still gets the path */
    lp_desktop_open_app_with(&d, "about", NULL, NULL, id);
    LP_ASSERT_STR(id, "w2");
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "about", NULL, "About again", id), 1);
    LP_ASSERT_EQ(d.wm.count, 2);
    LP_ASSERT_STR(d.wm.windows[1].title, "About again");
}

LP_TEST(routes_app_commands_to_the_focused_window) {
    setup();
    lp_desktop_open_app(&d, "stub");
    LP_ASSERT_EQ(lp_desktop_run_command(&d, LP_CMD_APP, 5), 1);
    LP_ASSERT_EQ(last_command, 5);
    LP_ASSERT_EQ(dirty_calls, 1);
    LP_ASSERT_STR(dirty_id, "w1");
    lp_desktop_open_app(&d, "gallery"); /* no command hook */
    LP_ASSERT_EQ(lp_desktop_run_command(&d, LP_CMD_APP, 6), 0);
    LP_ASSERT_EQ(last_command, 5);
}

LP_TEST(lets_the_focused_app_fill_the_file_and_go_menus) {
    setup();
    lp_desktop_build_menus(&d);
    LP_ASSERT_EQ(LP_DESKTOP_MENU_COUNT, 7);
    LP_ASSERT_STR(d.menus[LP_MENU_GO].label, "Go");
    LP_ASSERT_STR(d.menus[LP_MENU_GO].entries[1].label, "Desktop");
    LP_ASSERT_EQ(d.menus[LP_MENU_GO].entries[1].command, LP_CMD_GO);
    int has_open = 0;
    for (int i = 0; i < d.menus[LP_MENU_FILE].count; i++) if (strcmp(d.menus[LP_MENU_FILE].entries[i].label, "Open…") == 0) has_open = d.menus[LP_MENU_FILE].entries[i].disabled ? 1 : 2;
    LP_ASSERT_EQ(has_open, 1);
    lp_desktop_open_app(&d, "stub");
    lp_desktop_build_menus(&d);
    int stub_open = 0, stub_go = 0;
    for (int i = 0; i < d.menus[LP_MENU_FILE].count; i++) if (strcmp(d.menus[LP_MENU_FILE].entries[i].label, "Stub Open") == 0) stub_open = 1;
    for (int i = 0; i < d.menus[LP_MENU_GO].count; i++) if (strcmp(d.menus[LP_MENU_GO].entries[i].label, "Stub Go") == 0) stub_go = 1;
    LP_ASSERT(stub_open);
    LP_ASSERT(stub_go);
    LP_ASSERT_EQ(d.menus[LP_MENU_GO].count, 1);
    /* the Window menu never lists internal apps */
    for (int i = 0; i < d.menus[LP_MENU_WINDOW].count; i++) LP_ASSERT(strcmp(d.menus[LP_MENU_WINDOW].entries[i].label, "Open Stub") != 0);
    lp_desktop_open_app(&d, "gallery");
    lp_desktop_build_menus(&d);
    LP_ASSERT(d.menus[LP_MENU_GO].count > 1);
    /* Go › Documents opens a Finder there */
    LP_ASSERT_EQ(lp_desktop_run_command(&d, LP_CMD_GO, LP_USER_DOCUMENTS), 1);
    LP_ASSERT_STR(lp_wm_focused(&d.wm)->app_id, "finder");
    LP_ASSERT_STR(lp_wm_focused(&d.wm)->title, "Documents");
}

LP_TEST(opens_a_popup_that_runs_its_entry_on_its_window) {
    setup();
    lp_desktop_open_app(&d, "stub");
    lp_desktop_open_app(&d, "gallery"); /* focused now */
    lp_menu_model m = { .id = "x", .label = "", .count = 0 };
    lp_menu_entry *e = &m.entries[m.count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "Do it");
    e->command = LP_CMD_APP; e->arg = 9;
    lp_desktop_open_popup(&d, "w1", 100, 50, &m);
    LP_ASSERT_EQ(d.open_menu, LP_DESKTOP_MENU_POPUP);
    LP_ASSERT_STR(d.popup_window, "w1");
    LP_ASSERT_NEAR(d.popup_x, 100, 0.01);
    LP_ASSERT_STR(d.menus[LP_DESKTOP_MENU_POPUP].entries[0].label, "Do it");
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Down, 0), 1);
    LP_ASSERT_EQ(d.menu_active, 0);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Left, 0), 1); /* no menu switching from a popup */
    LP_ASSERT_EQ(d.open_menu, LP_DESKTOP_MENU_POPUP);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, 0), 1);
    LP_ASSERT_EQ(d.open_menu, -1);
    LP_ASSERT_EQ(last_command, 9); /* reached the stub, not the focused gallery */
    LP_ASSERT_STR(d.command_target, "");
    lp_desktop_open_popup(&d, "w1", 1, 1, &m);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Escape, 0), 1);
    LP_ASSERT_EQ(d.open_menu, -1);
}

LP_TEST(broadcasts_directory_changes_to_every_instance) {
    setup();
    lp_desktop_open_app(&d, "stub");
    lp_desktop_open_app(&d, "stub");
    lp_desktop_open_app(&d, "gallery");
    lp_desktop_files_changed(&d, "/tmp");
    LP_ASSERT_EQ(notified, 2);
    LP_ASSERT_EQ(dirty_calls, 2);
}

LP_TEST(leaves_shift_cmd_n_and_other_keys_to_the_app) {
    setup();
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_N, LP_MOD_LOGO | LP_MOD_SHIFT), 0);
    LP_ASSERT_EQ(d.wm.count, 0);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_n, LP_MOD_LOGO), 1);
    LP_ASSERT_EQ(d.wm.count, 1);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_o, LP_MOD_LOGO), 0);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Up, LP_MOD_LOGO), 0);
}

LP_TEST(keeps_a_drag_session_and_tells_the_host) {
    setup();
    const char *names[2] = { "a.txt", "b.txt" };
    LP_ASSERT_EQ(lp_desktop_drag_begin(&d, "w1", "/tmp", names, 2, LP_ICON_DOCUMENT, 0), 1);
    LP_ASSERT(d.drag.active);
    LP_ASSERT_EQ(d.drag.count, 2);
    LP_ASSERT_STR(d.drag.names[1], "b.txt");
    LP_ASSERT_STR(d.drag.label, "2 items");
    LP_ASSERT_STR(d.drag.src_dir, "/tmp");
    LP_ASSERT_EQ(drag_calls, 1);
    LP_ASSERT_EQ(lp_desktop_drag_begin(&d, "w1", "/tmp", names, 1, LP_ICON_FOLDER, 1), 1);
    LP_ASSERT_STR(d.drag.label, "a.txt");
    LP_ASSERT_EQ(drag_calls, 12); /* ended the first, began again */
    lp_desktop_drag_end(&d);
    LP_ASSERT(!d.drag.active);
    LP_ASSERT(d.drag.names == NULL);
    LP_ASSERT_EQ(drag_calls, 22);
    lp_desktop_drag_end(&d);
    LP_ASSERT_EQ(drag_calls, 22);
    LP_ASSERT_EQ(lp_desktop_drag_begin(&d, "w1", "/tmp", names, 0, LP_ICON_DOCUMENT, 0), 0);
}

LP_TEST(spotlight_skips_internal_apps) {
    setup();
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS];
    int n = lp_spotlight_items(&d, items, LP_SPOTLIGHT_MAX_ITEMS);
    for (int i = 0; i < n; i++) { LP_ASSERT(strcmp(items[i].id, "info") != 0); LP_ASSERT(strcmp(items[i].id, "stub") != 0); }
    LP_ASSERT_EQ(n, 5); /* finder, gallery, about, textedit + Terminal */
}

static int view_entry(const char *label) {
    const lp_menu_model *m = &d.menus[LP_MENU_VIEW];
    for (int i = 0; i < m->count; i++) if (!m->entries[i].separator && strcmp(m->entries[i].label, label) == 0) return i;
    return -1;
}

LP_TEST(the_clock_setting_round_trips) {
    lp_settings s = lp_settings_defaults();
    LP_ASSERT_EQ(s.clock, 1);
    s.clock = 0;
    LP_ASSERT_EQ(lp_settings_save(&s), 0);
    LP_ASSERT_EQ(lp_settings_load().clock, 0);
    /* a file written before the key existed keeps the clock showing */
    char path[600];
    snprintf(path, sizeof path, "%s/maryui/settings.conf", getenv("XDG_CONFIG_HOME"));
    FILE *f = fopen(path, "w");
    fputs("accent=graphite\n", f);
    fclose(f);
    lp_settings old = lp_settings_load();
    LP_ASSERT_EQ(old.clock, 1);
    LP_ASSERT_EQ(old.accent, LP_ACCENT_GRAPHITE);
    remove(path);
}

LP_TEST(view_menu_toggles_the_clock) {
    setup();
    lp_desktop_build_menus(&d);
    int entry = view_entry("Show Clock");
    LP_ASSERT(entry >= 0);
    if (entry < 0) return;
    LP_ASSERT(d.menus[LP_MENU_VIEW].entries[entry].checked);
    LP_ASSERT_EQ(lp_desktop_run_command(&d, d.menus[LP_MENU_VIEW].entries[entry].command, d.menus[LP_MENU_VIEW].entries[entry].arg), 1);
    LP_ASSERT_EQ(d.settings.clock, 0);
    LP_ASSERT_EQ(lp_settings_load().clock, 0);
    lp_desktop_build_menus(&d);
    LP_ASSERT(!d.menus[LP_MENU_VIEW].entries[view_entry("Show Clock")].checked);
    /* the Finder's own View entries and the desktop's still fit under the cap */
    lp_desktop_open_app(&d, "finder");
    lp_desktop_build_menus(&d);
    LP_ASSERT(view_entry("Show Clock") >= 0);
    LP_ASSERT(view_entry("Folders · Slate") >= 0);
    LP_ASSERT(d.menus[LP_MENU_VIEW].count < LP_MENU_MAX_ENTRIES);
    lp_desktop_run_command(&d, LP_CMD_TOGGLE_CLOCK, 0);
    LP_ASSERT_EQ(d.settings.clock, 1);
}

static void textedit_pass(void *state, lp_input in) {
    static lp_ctx ctx;
    ctx.settings = &d.settings;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 560, 392), 1000);
    lp_app_textedit.paint(state, &ctx, LP_RECT(0, 0, 560, 392), &d);
    lp_ctx_end(&ctx);
}

LP_TEST(textedit_opens_a_path_and_saves_back_to_it) {
    setup();
    char note[600];
    snprintf(note, sizeof note, "%s/Documents/note.md", root);
    char id[12];
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "textedit", note, "note.md", id), 1);
    void *t = lp_desktop_instance(&d, id)->state;
    LP_ASSERT_STR(lp_textedit_path(t), note);
    LP_ASSERT_STR(lp_textedit_name(t), "note.md");
    LP_ASSERT_STR(lp_textedit_text(t), "# hi\n");
    /* ⌘S writes the document where it came from and tells the desktop */
    notified = 0;
    lp_app_textedit.command(t, &d, LP_TEXTEDIT_SAVE);
    char *text = NULL;
    LP_ASSERT_EQ(lp_files_read(note, &text, NULL), 0);
    LP_ASSERT_STR(text, "# hi\n");
    free(text);
    lp_input in = { .mx = NAN, .my = NAN, .keysym = XKB_KEY_s, .mods = LP_MOD_LOGO, .key_pressed = 1, .utf8 = "s" };
    textedit_pass(t, in);
    /* ⌘O opens a Finder at the document's folder */
    in.keysym = XKB_KEY_o; in.utf8[0] = 'o';
    textedit_pass(t, in);
    const lp_window_record *w = lp_wm_focused(&d.wm);
    LP_ASSERT_STR(w->app_id, "finder");
    LP_ASSERT_STR(w->title, "Documents");
    /* a new document lives in ~/Documents and gets .txt only without an extension */
    lp_desktop_open_app_with(&d, "textedit", NULL, NULL, id);
    void *fresh = lp_desktop_instance(&d, id)->state;
    LP_ASSERT_STR(lp_textedit_path(fresh), "");
    lp_textedit_set_text(fresh, "Plans", "go");
    lp_app_textedit.command(fresh, &d, LP_TEXTEDIT_SAVE);
    char plans[600];
    snprintf(plans, sizeof plans, "%s/Documents/Plans.txt", root);
    LP_ASSERT(lp_files_exists(plans, NULL));
    lp_textedit_set_text(fresh, "readme.md", "x");
    lp_app_textedit.command(fresh, &d, LP_TEXTEDIT_SAVE);
    snprintf(plans, sizeof plans, "%s/Documents/readme.md", root);
    LP_ASSERT(lp_files_exists(plans, NULL));
    LP_ASSERT_STR(lp_textedit_path(fresh), plans);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_desktop_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    setenv("HOME", root, 1);
    /* Settings commands save as they run; keep them out of the real ~/.config. */
    char config[600];
    snprintf(config, sizeof config, "%s/.config", root);
    setenv("XDG_CONFIG_HOME", config, 1);
    char docs[600];
    snprintf(docs, sizeof docs, "%s/Documents", root);
    mkdir(docs, 0755);
    write_file("Documents/note.md", "# hi\n", 5);
    write_file("Documents/photo.png", "\x89PNG\0\0", 6);
    LP_RUN(opens_a_folder_in_the_finder_and_a_text_file_in_textedit);
    LP_RUN(routes_pictures_and_media_to_their_viewers_once_registered);
    LP_RUN(new_terminal_runs_foot_until_a_terminal_app_is_registered);
    LP_RUN(hands_the_path_to_the_app_open_hook_with_the_window_id);
    LP_RUN(routes_app_commands_to_the_focused_window);
    LP_RUN(lets_the_focused_app_fill_the_file_and_go_menus);
    LP_RUN(opens_a_popup_that_runs_its_entry_on_its_window);
    LP_RUN(broadcasts_directory_changes_to_every_instance);
    LP_RUN(leaves_shift_cmd_n_and_other_keys_to_the_app);
    LP_RUN(keeps_a_drag_session_and_tells_the_host);
    LP_RUN(spotlight_skips_internal_apps);
    LP_RUN(textedit_opens_a_path_and_saves_back_to_it);
    LP_RUN(the_clock_setting_round_trips);
    LP_RUN(view_menu_toggles_the_clock);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
