/* Reopening windows at login: the open app windows written in stacking order with their place, size, state and
 * document, opened again the same way; a gone document opens its app empty, an unknown app or a bad line is
 * skipped, no file opens nothing; the switch is off by default and survives a save. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE   /* mkdtemp */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_session.h"
#include "maryui/lp_settings.h"

static char root[64], folder[128], note[160], session[160];

static void fresh(lp_desktop *d) {
    lp_desktop_init(d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(d);
}

static const lp_window_record *record_of(lp_desktop *d, const char *app) {
    for (int i = 0; i < d->wm.count; i++) if (strcmp(d->wm.windows[i].app_id, app) == 0) return &d->wm.windows[i];
    return NULL;
}

LP_TEST(the_windows_come_back_where_they_were) {
    static lp_desktop d, e;
    fresh(&d);
    char f[12], t[12], c[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "finder", folder, NULL, f));
    LP_ASSERT(lp_desktop_open_app_with(&d, "textedit", note, NULL, t));
    LP_ASSERT(lp_desktop_open_app_with(&d, "calculator", NULL, NULL, c));
    lp_wm_action a = { .type = LP_WM_RESIZE, .id = f, .rect = LP_RECT(100, 120, 700, 460) };
    lp_desktop_dispatch(&d, &a);
    a = (lp_wm_action){ .type = LP_WM_RESIZE, .id = t, .rect = LP_RECT(300, 200, 620, 480) };
    lp_desktop_dispatch(&d, &a);
    a = (lp_wm_action){ .type = LP_WM_TOGGLE_ZOOM, .id = t };
    lp_desktop_dispatch(&d, &a);
    a = (lp_wm_action){ .type = LP_WM_TOGGLE_SHADE, .id = c };
    lp_desktop_dispatch(&d, &a);
    a = (lp_wm_action){ .type = LP_WM_FOCUS, .id = f };                 /* the Finder in front */
    lp_desktop_dispatch(&d, &a);
    const lp_window_record *calc_before = record_of(&d, "calculator");
    lp_rect calc_rect = calc_before->rect;
    LP_ASSERT_EQ(lp_session_save(&d), 3);

    fresh(&e);
    LP_ASSERT_EQ(lp_session_restore(&e), 3);
    LP_ASSERT_EQ(e.wm.count, 3);
    const lp_window_record *finder = record_of(&e, "finder"), *text = record_of(&e, "textedit"), *calc = record_of(&e, "calculator");
    LP_ASSERT(finder && text && calc);
    if (!finder || !text || !calc) return;
    LP_ASSERT_EQ(finder->state, LP_WIN_NORMAL);
    LP_ASSERT_NEAR(finder->rect.x, 100, 0.5);
    LP_ASSERT_NEAR(finder->rect.y, 120, 0.5);
    LP_ASSERT_NEAR(finder->rect.w, 700, 0.5);
    LP_ASSERT_NEAR(finder->rect.h, 460, 0.5);
    LP_ASSERT_EQ(text->state, LP_WIN_ZOOMED);                           /* zoomed again, and unzooms to its own size */
    LP_ASSERT(text->has_prev);
    LP_ASSERT_NEAR(text->prev_rect.w, 620, 0.5);
    LP_ASSERT_NEAR(text->prev_rect.h, 480, 0.5);
    LP_ASSERT_EQ(calc->state, LP_WIN_SHADED);
    LP_ASSERT_NEAR(calc->rect.x, calc_rect.x, 0.5);
    LP_ASSERT(lp_wm_focused(&e.wm) == finder);                          /* stacking order kept: the front window is in front */
    char doc[1024];
    LP_ASSERT(lp_session_document_path(&e, finder->id, doc, sizeof doc));
    LP_ASSERT_STR(doc, folder);
    LP_ASSERT(lp_session_document_path(&e, text->id, doc, sizeof doc));
    LP_ASSERT_STR(doc, note);
    LP_ASSERT(!lp_session_document_path(&e, calc->id, doc, sizeof doc));
}

LP_TEST(a_gone_document_opens_its_app_empty_and_what_cannot_open_is_skipped) {
    static lp_desktop d;
    FILE *f = fopen(session, "w");
    LP_ASSERT(f != NULL);
    if (!f) return;
    fprintf(f, "# a hand-edited session\n");
    fprintf(f, "textedit 120 90 500 400 normal %s/gone.txt\n", root);
    fprintf(f, "notanapp 10 10 300 200 normal\n");
    fprintf(f, "finder 50 60 nonsense\n");
    fprintf(f, "info 50 60 300 200 normal\n");                                /* internal: opened by other apps only */
    fprintf(f, "finder 50 60 600 400 shaded %s\n", folder);
    fclose(f);
    fresh(&d);
    LP_ASSERT_EQ(lp_session_restore(&d), 2);
    LP_ASSERT_EQ(d.wm.count, 2);
    const lp_window_record *text = record_of(&d, "textedit"), *finder = record_of(&d, "finder");
    LP_ASSERT(text && finder);
    if (!text || !finder) return;
    char doc[1024];
    LP_ASSERT(!lp_session_document_path(&d, text->id, doc, sizeof doc));    /* empty: the document went */
    LP_ASSERT_EQ(finder->state, LP_WIN_SHADED);
    LP_ASSERT(lp_session_document_path(&d, finder->id, doc, sizeof doc));
    LP_ASSERT_STR(doc, folder);
}

LP_TEST(no_file_opens_nothing_and_an_empty_desktop_saves_an_empty_session) {
    static lp_desktop d;
    unlink(session);
    fresh(&d);
    LP_ASSERT_EQ(lp_session_restore(&d), 0);
    LP_ASSERT_EQ(d.wm.count, 0);
    LP_ASSERT_EQ(lp_session_save(&d), 0);                                 /* everything closed: nothing comes back */
    LP_ASSERT_EQ(lp_session_restore(&d), 0);
}

LP_TEST(reopening_is_a_setting_off_by_default) {
    LP_ASSERT_EQ(lp_settings_defaults().restore_windows, 0);
    lp_settings s = lp_settings_defaults();
    s.restore_windows = 1;
    LP_ASSERT_EQ(lp_settings_save(&s), 0);
    LP_ASSERT_EQ(lp_settings_load().restore_windows, 1);
    s.restore_windows = 0;
    LP_ASSERT_EQ(lp_settings_save(&s), 0);
    LP_ASSERT_EQ(lp_settings_load().restore_windows, 0);
}

int main(void) {
    snprintf(root, sizeof root, "/tmp/lp-session-XXXXXX");
    if (!mkdtemp(root)) return 1;
    setenv("HOME", root, 1);
    setenv("XDG_CONFIG_HOME", root, 1);
    snprintf(folder, sizeof folder, "%s/Tides", root);
    mkdir(folder, 0755);
    snprintf(note, sizeof note, "%s/notes.txt", root);
    FILE *f = fopen(note, "w");
    if (f) { fputs("The tide comes in.\n", f); fclose(f); }
    snprintf(session, sizeof session, "%s/maryui/%s", root, LP_SESSION_FILE);
    LP_RUN(the_windows_come_back_where_they_were);
    LP_RUN(a_gone_document_opens_its_app_empty_and_what_cannot_open_is_skipped);
    LP_RUN(no_file_opens_nothing_and_an_empty_desktop_saves_an_empty_session);
    LP_RUN(reopening_is_a_setting_off_by_default);
    LP_TEST_MAIN_END();
}
