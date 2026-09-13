/* Activity Monitor, driven headlessly over a fixture /proc: the list and its
 * order, search, the refresh that stops while the window is shaded, and Quit
 * Process… through its sheet — Esc cancels, Return quits, Force Quit kills —
 * with the signal injected, so no real process is ever touched. Init and the
 * desktop itself cannot be quit. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "lp_test_loop.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"

static char root[512];
static lp_desktop d;
static lp_ctx ctx;
static double now_ms = 1000;
static const lp_rect BODY = { 0, 0, 720, 438 };

static int signalled_pid, signalled_force, signal_calls;
static int fake_signal(int pid, int force) { signalled_pid = pid; signalled_force = force; signal_calls++; return 0; }

static void put(const char *rel, const char *text, size_t len) {
    char path[1024], dir[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = 0; mkdir(dir, 0755); }
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fwrite(text, 1, len, f);
    fclose(f);
}

static void process(int pid, const char *comm, int ticks, int rss_kb, unsigned uid) {
    char text[512], rel[64];
    snprintf(text, sizeof text, "%d (%s) S 1 %d %d 0 -1 0 0 0 0 0 %d 0 0 0 20 0 3 0 100 0 0\n", pid, comm, pid, pid, ticks);
    snprintf(rel, sizeof rel, "%d/stat", pid);
    put(rel, text, strlen(text));
    snprintf(text, sizeof text, "Name:\t%s\nUid:\t%u\t%u\t%u\t%u\nThreads:\t3\nVmRSS:\t%d kB\n", comm, uid, uid, uid, uid, rss_kb);
    snprintf(rel, sizeof rel, "%d/status", pid);
    put(rel, text, strlen(text));
    snprintf(rel, sizeof rel, "%d/cmdline", pid);
    put(rel, comm, strlen(comm) + 1);
}

/* 400 ticks across 4 CPUs each time `busy` grows by one; the editor uses 50 of them. */
static void write_fixture(int step) {
    char stat[256];
    snprintf(stat, sizeof stat, "cpu  %d 0 0 %d 0 0 0 0\ncpu0 0\ncpu1 0\ncpu2 0\ncpu3 0\n", 1000 + 100 * step, 9000 + 300 * step);
    put("stat", stat, strlen(stat));
    const char *mem = "MemTotal: 4000000 kB\nMemAvailable: 1000000 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n";
    put("meminfo", mem, strlen(mem));
    put("loadavg", "0.10 0.20 0.30 1/3 9\n", 21);
    process(1, "systemd", 10, 12000, 0);
    process(4242, "editor", 100 + 50 * step, 204800, (unsigned)getuid());
    process(3131, "backup", 20, 51200, (unsigned)getuid());
}

static void pass(void *state, lp_input in) {
    now_ms += 50;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, BODY, now_ms);
    lp_app_activity.paint(state, &ctx, BODY, &d);
    lp_ctx_end(&ctx);
    ctx.in.mx = in.mx;
    ctx.in.my = in.my;
    ctx.in.buttons = in.buttons;
}
static void key(void *state, uint32_t sym, uint32_t mods) {
    lp_input in = { .mx = NAN, .my = NAN, .keysym = sym, .mods = mods, .key_pressed = 1 };
    pass(state, in);
    in.key_pressed = 0;
    pass(state, in);
}
static void click(void *state, lp_rect r) {
    float x = r.x + r.w / 2, y = r.y + r.h / 2;
    pass(state, (lp_input){ .mx = x, .my = y });
    pass(state, (lp_input){ .mx = x, .my = y, .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT });
    pass(state, (lp_input){ .mx = x, .my = y, .released = LP_BUTTON_LEFT });
}

static void *open_monitor(char *id) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&d);
    lp_desktop_register_builtin_apps(&d);
    memset(&ctx, 0, sizeof ctx);
    ctx.settings = &d.settings;
    ctx.active_window = 1;
    write_fixture(0);
    if (lp_desktop_open_app_with(&d, "activity", NULL, NULL, id) != 1) return NULL;
    void *state = lp_desktop_instance(&d, id)->state;
    lp_activity_set_root(state, root);
    lp_activity_set_signal(state, fake_signal);
    signal_calls = 0;
    return state;
}

static int ticked_state(void) { return 0; }

LP_TEST(lists_the_processes_and_searches_them) {
    char id[12];
    void *a = open_monitor(id);
    LP_ASSERT(a != NULL);
    LP_ASSERT_EQ(lp_activity_visible_count(a), 3);
    LP_ASSERT_EQ(lp_activity_visible_pid(a, 0), 1);          /* no CPU measured yet: equals keep pid order */
    lp_desktop_run_command(&d, LP_CMD_APP, LP_ACTIVITY_VIEW_MEMORY);
    LP_ASSERT_EQ(lp_activity_visible_pid(a, 0), 4242);        /* the most memory first */
    LP_ASSERT_EQ(lp_activity_visible_pid(a, 2), 1);
    lp_activity_search(a, "BACK");
    LP_ASSERT_EQ(lp_activity_visible_count(a), 1);
    LP_ASSERT_EQ(lp_activity_visible_pid(a, 0), 3131);
    lp_activity_search(a, "");
    key(a, XKB_KEY_Down, 0);
    LP_ASSERT_EQ(lp_activity_selected(a), 4242);
    key(a, XKB_KEY_Down, 0);
    LP_ASSERT_EQ(lp_activity_selected(a), 3131);
    lp_desktop_build_menus(&d);
    int found = 0;
    for (int i = 0; i < d.menus[LP_MENU_FILE].count; i++) found |= strcmp(d.menus[LP_MENU_FILE].entries[i].label, "Quit Process…") == 0;
    LP_ASSERT(found);
    lp_desktop_close_window(&d, id);
    (void)ticked_state;
}

LP_TEST(refreshes_on_its_timer_but_not_while_shaded) {
    char id[12];
    void *a = open_monitor(id);
    LP_ASSERT_NEAR(lp_activity_cpu(a, 4242), 0, 1e-9);
    write_fixture(1);
    lp_desktop_run_command(&d, LP_CMD_TOGGLE_SHADE_FOCUSED, 0);
    lp_test_loop_run(2300, NULL);
    LP_ASSERT_NEAR(lp_activity_cpu(a, 4242), 0, 1e-9);        /* shaded: nothing read */
    lp_desktop_run_command(&d, LP_CMD_TOGGLE_SHADE_FOCUSED, 0);
    lp_test_loop_run(2300, NULL);
    LP_ASSERT_NEAR(lp_activity_cpu(a, 4242), 50, 1e-6);       /* 50 of the 100 ticks one CPU lived through */
    lp_desktop_close_window(&d, id);
    LP_ASSERT_EQ(d.wm.count, 0);
}

LP_TEST(quits_through_a_sheet_that_can_be_cancelled_or_forced) {
    char id[12];
    void *a = open_monitor(id);
    lp_activity_select(a, 4242);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_ACTIVITY_QUIT_PROCESS);
    LP_ASSERT(lp_activity_sheet_open(a));
    key(a, XKB_KEY_Down, 0);                                  /* the list beneath holds still */
    LP_ASSERT_EQ(lp_activity_selected(a), 4242);
    key(a, XKB_KEY_Escape, 0);
    LP_ASSERT(!lp_activity_sheet_open(a));
    LP_ASSERT_EQ(signal_calls, 0);

    lp_desktop_run_command(&d, LP_CMD_APP, LP_ACTIVITY_QUIT_PROCESS);
    key(a, XKB_KEY_Return, 0);
    LP_ASSERT_EQ(signal_calls, 1);
    LP_ASSERT_EQ(signalled_pid, 4242);
    LP_ASSERT_EQ(signalled_force, 0);
    LP_ASSERT(strstr(lp_activity_status(a), "Asked") != NULL);

    lp_desktop_run_command(&d, LP_CMD_APP, LP_ACTIVITY_QUIT_PROCESS);
    click(a, lp_activity_sheet_button(a, &ctx, BODY, 2));
    LP_ASSERT_EQ(signal_calls, 2);
    LP_ASSERT_EQ(signalled_force, 1);
    LP_ASSERT(!lp_activity_sheet_open(a));
    lp_desktop_close_window(&d, id);
}

LP_TEST(will_not_quit_init_or_the_desktop) {
    char id[12];
    void *a = open_monitor(id);
    lp_activity_select(a, 1);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_ACTIVITY_QUIT_PROCESS);
    LP_ASSERT(!lp_activity_sheet_open(a));
    LP_ASSERT(strstr(lp_activity_status(a), "cannot be quit") != NULL);
    char pid_dir[64];
    snprintf(pid_dir, sizeof pid_dir, "%d", (int)getpid());
    process((int)getpid(), "maryui-desktop", 1, 1000, (unsigned)getuid());
    lp_activity_set_root(a, root);
    lp_activity_select(a, (int)getpid());
    lp_desktop_run_command(&d, LP_CMD_APP, LP_ACTIVITY_QUIT_PROCESS);
    LP_ASSERT(!lp_activity_sheet_open(a));
    LP_ASSERT_EQ(signal_calls, 0);
    lp_files_delete_tree(root);
    mkdir(root, 0755);
    lp_desktop_close_window(&d, id);
}

LP_TEST(says_so_when_there_is_no_proc) {
    char id[12];
    void *a = open_monitor(id);
    char missing[600];
    snprintf(missing, sizeof missing, "%s/nowhere", root);
    lp_activity_set_root(a, missing);
    LP_ASSERT_EQ(lp_activity_visible_count(a), 0);
    pass(a, (lp_input){ .mx = NAN, .my = NAN });              /* and paints without a crash */
    lp_desktop_close_window(&d, id);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_activity_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    char config[600];
    snprintf(config, sizeof config, "%s/config", root);
    setenv("XDG_CONFIG_HOME", config, 1);
    LP_RUN(lists_the_processes_and_searches_them);
    LP_RUN(refreshes_on_its_timer_but_not_while_shaded);
    LP_RUN(quits_through_a_sheet_that_can_be_cancelled_or_forced);
    LP_RUN(will_not_quit_init_or_the_desktop);
    LP_RUN(says_so_when_there_is_no_proc);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
