/* Terminal's screen over libvterm — text and attributes land in cells, keys
 * come out as a terminal's bytes, rows report damage once, lines scroll back,
 * programs set the title, selections read back — and the app itself running a
 * real shell through the poll loop. Without libvterm, only that the build says
 * so. Linux only (PARITY D15). */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_term.h"
#include "maryui/lp_ui.h"

#ifndef HAVE_VTERM

LP_TEST(says_the_build_has_no_libvterm) {
    LP_ASSERT_EQ(lp_term_available(), 0);
    LP_ASSERT(lp_term_new(24, 80, NULL, NULL) == NULL);
}

int main(void) {
    LP_RUN(says_the_build_has_no_libvterm);
    LP_TEST_MAIN_END();
}

#else

#include "lp_test_loop.h"

static char sent[256];
static size_t sent_len;

static void capture(const char *bytes, size_t len, void *user) {
    if (sent_len + len >= sizeof sent) return;
    memcpy(sent + sent_len, bytes, len);
    sent_len += len;
    sent[sent_len] = 0;
}

static void clear_sent(void) { sent_len = 0; sent[0] = 0; }

static void feed(lp_term *t, const char *s) { lp_term_feed(t, s, strlen(s)); }

static void row_text(lp_term *t, int row, char *out, size_t n) { lp_term_text(t, row, 0, row, 499, out, n); }

LP_TEST(writes_text_into_cells) {
    lp_term *t = lp_term_new(5, 20, capture, NULL);
    LP_ASSERT(t != NULL);
    feed(t, "hello\r\nwörld");
    char s[128];
    row_text(t, 0, s, sizeof s);
    LP_ASSERT_STR(s, "hello");
    row_text(t, 1, s, sizeof s);
    LP_ASSERT_STR(s, "wörld");
    int row, col, visible;
    lp_term_cursor(t, &row, &col, &visible);
    LP_ASSERT_EQ(row, 1);
    LP_ASSERT_EQ(col, 5);
    LP_ASSERT(visible);
    lp_term_free(t);
}

LP_TEST(applies_colours_and_attributes) {
    lp_term *t = lp_term_new(3, 10, capture, NULL);
    feed(t, "\x1b[1;31mR\x1b[0m\x1b[7mV\x1b[0mn");
    lp_term_cell c;
    lp_term_cell_at(t, 0, 0, &c);
    LP_ASSERT_STR(c.text, "R");
    LP_ASSERT(c.bold);
    LP_ASSERT(c.fg.r > 0.5f && c.fg.r > c.fg.b * 2);   /* red, from the palette */
    LP_ASSERT(c.bg_default);
    lp_term_cell_at(t, 0, 1, &c);
    LP_ASSERT(c.reverse);
    LP_ASSERT(!c.bg_default);
    LP_ASSERT(c.bg.r < 0.3f);                          /* the ink became the background */
    lp_term_cell_at(t, 0, 2, &c);
    LP_ASSERT(c.fg.r < 0.3f && c.bg.r > 0.9f);        /* ink on the well */
    LP_ASSERT(c.bg_default);
    lp_term_free(t);
}

LP_TEST(sends_the_bytes_keys_stand_for) {
    lp_term *t = lp_term_new(3, 10, capture, NULL);
    static const struct { uint32_t sym; const char *utf8; uint32_t mods; const char *bytes; } KEYS[] = {
        { XKB_KEY_Return, "\r", 0, "\r" },
        { XKB_KEY_Up, "", 0, "\x1b[A" },
        { XKB_KEY_c, "\x03", LP_MOD_CTRL, "\x03" },
        { XKB_KEY_A, "A", LP_MOD_SHIFT, "A" },
        { XKB_KEY_b, "b", LP_MOD_ALT, "\x1b" "b" },
        { XKB_KEY_BackSpace, "\b", 0, "\x7f" },
        { XKB_KEY_ISO_Left_Tab, "", LP_MOD_SHIFT, "\x1b[Z" },
        { XKB_KEY_F1, "", 0, "\x1bOP" },
        { XKB_KEY_odiaeresis, "ö", 0, "ö" },
    };
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
        clear_sent();
        LP_ASSERT_EQ(lp_term_key(t, KEYS[i].sym, KEYS[i].utf8, KEYS[i].mods), 1);
        if (strcmp(sent, KEYS[i].bytes) != 0) LP_FAIL("key %zu sent %zu bytes, not the expected %zu", i, sent_len, strlen(KEYS[i].bytes));
    }
    clear_sent();
    LP_ASSERT_EQ(lp_term_key(t, XKB_KEY_Shift_L, "", LP_MOD_SHIFT), 0);
    LP_ASSERT_EQ(sent_len, 0);
    lp_term_free(t);
}

LP_TEST(reports_changed_rows_once) {
    lp_term *t = lp_term_new(4, 10, capture, NULL);
    unsigned char rows[4];
    lp_term_take_damage(t, rows, 4);
    feed(t, "\x1b[3;1Hx");
    LP_ASSERT_EQ(lp_term_take_damage(t, rows, 4), 1);
    LP_ASSERT_EQ(rows[2], 1);
    LP_ASSERT_EQ(rows[1], 0);
    LP_ASSERT_EQ(lp_term_take_damage(t, rows, 4), 0);
    lp_term_free(t);
}

LP_TEST(keeps_the_lines_that_scroll_off) {
    lp_term *t = lp_term_new(3, 10, capture, NULL);
    feed(t, "one\r\ntwo\r\nthree\r\nfour\r\n");
    LP_ASSERT_EQ(lp_term_scrollback_lines(t), 2);
    char s[128];
    row_text(t, -1, s, sizeof s);
    LP_ASSERT_STR(s, "two");
    row_text(t, -2, s, sizeof s);
    LP_ASSERT_STR(s, "one");
    row_text(t, 0, s, sizeof s);
    LP_ASSERT_STR(s, "three");
    lp_term_text(t, 0, 2, -2, 1, s, sizeof s);        /* either order; partial first and last lines */
    LP_ASSERT_STR(s, "ne\ntwo\nthr");
    feed(t, "\x1b[3J");
    LP_ASSERT_EQ(lp_term_scrollback_lines(t), 0);
    lp_term_free(t);
}

LP_TEST(takes_the_title_a_program_sets) {
    lp_term *t = lp_term_new(3, 10, capture, NULL);
    int changed = 0;
    LP_ASSERT_STR(lp_term_title(t, &changed), "");
    feed(t, "\x1b]2;make test\x07");
    LP_ASSERT_STR(lp_term_title(t, &changed), "make test");
    LP_ASSERT(changed);
    lp_term_title(t, &changed);
    LP_ASSERT(!changed);
    feed(t, "\x1b]0;vim\x1b\\");
    LP_ASSERT_STR(lp_term_title(t, &changed), "vim");
    lp_term_free(t);
}

LP_TEST(resizes_and_rewraps) {
    lp_term *t = lp_term_new(3, 10, capture, NULL);
    feed(t, "abcdefghijkl");
    char s[128];
    row_text(t, 1, s, sizeof s);
    LP_ASSERT_STR(s, "kl");
    lp_term_resize(t, 3, 20);
    int rows, cols;
    lp_term_size(t, &rows, &cols);
    LP_ASSERT_EQ(cols, 20);
    row_text(t, 0, s, sizeof s);
    LP_ASSERT_STR(s, "abcdefghijkl");
    lp_term_free(t);
}

LP_TEST(pastes_with_brackets_when_asked) {
    lp_term *t = lp_term_new(3, 10, capture, NULL);
    feed(t, "\x1b[?2004h");
    clear_sent();
    lp_term_paste(t, "a\r\nb", 4);
    LP_ASSERT_STR(sent, "\x1b[200~" "a\rb" "\x1b[201~");
    feed(t, "\x1b[?2004l");
    clear_sent();
    lp_term_paste(t, "x\n", 2);
    LP_ASSERT_STR(sent, "x\r");
    lp_term_free(t);
}

LP_TEST(rings_the_bell_and_notes_the_alternate_screen) {
    lp_term *t = lp_term_new(3, 10, capture, NULL);
    feed(t, "\x07");
    LP_ASSERT_EQ(lp_term_take_bell(t), 1);
    LP_ASSERT_EQ(lp_term_take_bell(t), 0);
    feed(t, "\x1b[?1049h");
    LP_ASSERT(lp_term_altscreen(t));
    feed(t, "\x1b[?1049l");
    LP_ASSERT(!lp_term_altscreen(t));
    lp_term_free(t);
}

/* MARK: - The app, with a real shell */

static lp_desktop desk;
static void *term_state;
static const char *wanted;

static int screen_shows(void) {
    for (int r = 0; r < 24; r++) {
        char s[512];
        lp_terminal_row_text(term_state, r, s, sizeof s);
        if (strstr(s, wanted)) return 1;
    }
    return 0;
}

static int has_exited(void) { return lp_terminal_exited(term_state); }

static int wait_gone(pid_t pid) {
    for (int i = 0; i < 300; i++) {
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || (r < 0 && errno == ECHILD)) return 1;
        poll(NULL, 0, 10);
    }
    return 0;
}

static void paste_command(const char *text) {
    lp_text_clipboard_set(lp_text_clipboard_shared(), text, (int)strlen(text));
    lp_desktop_run_command(&desk, LP_CMD_APP, LP_TERMINAL_PASTE);
}

LP_TEST(the_app_runs_a_shell_and_hangs_it_up_on_close) {
    lp_desktop_init(&desk, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&desk);
    lp_desktop_register_builtin_apps(&desk);
    char id[12];
    LP_ASSERT_EQ(lp_desktop_open_app_with(&desk, "terminal", NULL, NULL, id), 1);
    term_state = lp_desktop_instance(&desk, id)->state;
    pid_t pid = lp_terminal_pid(term_state);
    LP_ASSERT(pid > 0);
    paste_command("echo hi-$((20+22))\n");
    wanted = "hi-42";
    lp_test_loop_run(5000, screen_shows);
    LP_ASSERT(screen_shows());
    lp_desktop_close_window(&desk, id);
    LP_ASSERT_EQ(desk.wm.count, 0);
    LP_ASSERT(wait_gone(pid));
}

LP_TEST(the_app_notices_when_the_shell_exits) {
    lp_desktop_init(&desk, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&desk);
    lp_desktop_register_builtin_apps(&desk);
    char id[12];
    LP_ASSERT_EQ(lp_desktop_open_app_with(&desk, "terminal", NULL, NULL, id), 1);
    term_state = lp_desktop_instance(&desk, id)->state;
    paste_command("exit\n");
    lp_test_loop_run(5000, has_exited);
    LP_ASSERT(lp_terminal_exited(term_state));
    wanted = "[Process completed]";
    LP_ASSERT(screen_shows());
    lp_desktop_close_window(&desk, id);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    setenv("SHELL", "/bin/sh", 1);   /* the same shell everywhere these run */
    LP_RUN(writes_text_into_cells);
    LP_RUN(applies_colours_and_attributes);
    LP_RUN(sends_the_bytes_keys_stand_for);
    LP_RUN(reports_changed_rows_once);
    LP_RUN(keeps_the_lines_that_scroll_off);
    LP_RUN(takes_the_title_a_program_sets);
    LP_RUN(resizes_and_rewraps);
    LP_RUN(pastes_with_brackets_when_asked);
    LP_RUN(rings_the_bell_and_notes_the_alternate_screen);
    LP_RUN(the_app_runs_a_shell_and_hangs_it_up_on_close);
    LP_RUN(the_app_notices_when_the_shell_exits);
    LP_TEST_MAIN_END();
}

#endif
