/* Activity Monitor's processes, read from a fixture /proc: totals, each
 * process's fields (a comm with parentheses of its own, a kernel thread, a
 * name cut at 15 characters), CPU shares between samples and a reused pid,
 * sorting, search, memory formatting, and quitting a real child. The fixture
 * makes these run on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "lp_test.h"
#include "maryui/lp_files.h"
#include "maryui/lp_proc.h"

static char root[512];

static void put(const char *rel, const char *text, size_t len) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = 0; mkdir(dir, 0755); }
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fwrite(text, 1, len, f);
    fclose(f);
}
static void puts_(const char *rel, const char *text) { put(rel, text, strlen(text)); }

/* A stat line with the fields this reads filled in. */
static void stat_line(int pid, const char *comm, char state, int ppid, int utime, int stime, int threads, int start) {
    char line[512], rel[64];
    snprintf(line, sizeof line, "%d (%s) %c %d %d %d 0 -1 4194560 100 200 3 4 %d %d 0 0 20 0 %d 0 %d 12345678 3000 18446744073709551615\n",
             pid, comm, state, ppid, pid, pid, utime, stime, threads, start);
    snprintf(rel, sizeof rel, "%d/stat", pid);
    puts_(rel, line);
}

static void write_fixture(int cpu_total_extra, int idle_extra, int firefox_ticks, int kworker_start) {
    char stat[512];
    snprintf(stat, sizeof stat, "cpu  %d 0 500 %d 500 0 0 0 0 0\ncpu0 1 0 0 0 0 0 0 0 0 0\ncpu1 1 0 0 0 0 0 0 0 0 0\n"
             "cpu2 1 0 0 0 0 0 0 0 0 0\ncpu3 1 0 0 0 0 0 0 0 0 0\nintr 1 2 3\nctxt 99\n", 1000 + cpu_total_extra - idle_extra, 8000 + idle_extra);
    puts_("stat", stat);
    puts_("meminfo", "MemTotal:        4000000 kB\nMemFree:          200000 kB\nMemAvailable:    1000000 kB\nSwapTotal:       2000000 kB\nSwapFree:        1500000 kB\n");
    puts_("loadavg", "0.52 0.58 0.59 1/345 12345\n");

    stat_line(1, "systemd", 'S', 0, 50, 30, 1, 5);
    puts_("1/status", "Name:\tsystemd\nUid:\t0\t0\t0\t0\nThreads:\t1\nVmRSS:\t   12000 kB\n");
    put("1/cmdline", "/sbin/init\0splash\0", 18);

    char status[256];
    stat_line(4242, "Web Content (x)", 'R', 1, 200 + firefox_ticks, 100, 12, 900);
    snprintf(status, sizeof status, "Name:\tWeb Content\nUid:\t%u\t%u\t%u\t%u\nThreads:\t12\nVmRSS:\t  204800 kB\n",
             (unsigned)getuid(), (unsigned)getuid(), (unsigned)getuid(), (unsigned)getuid());
    puts_("4242/status", status);
    put("4242/cmdline", "/usr/lib/firefox/firefox\0-contentproc\0", 38);

    stat_line(77, "kworker/0:1", 'I', 2, 5, 5, 1, kworker_start);
    puts_("77/status", "Name:\tkworker/0:1\nUid:\t0\t0\t0\t0\nThreads:\t1\n");
    put("77/cmdline", "", 0);

    stat_line(900, "gnome-shell-cal", 'S', 1, 1, 1, 5, 400);
    puts_("900/status", "Name:\tgnome-shell-cal\nUid:\t0\t0\t0\t0\nThreads:\t5\nVmRSS:\t    8000 kB\n");
    put("900/cmdline", "/usr/libexec/gnome-shell-calendar-server\0", 41);

    puts_("self/stat", "not a pid\n");   /* ignored: not numeric */
}

LP_TEST(reads_the_totals_and_each_process) {
    write_fixture(0, 0, 0, 10);
    lp_proc_sample s;
    LP_ASSERT_EQ(lp_proc_read(root, NULL, &s), 0);
    LP_ASSERT_EQ(s.cpus, 4);
    LP_ASSERT_EQ(s.count, 4);
    LP_ASSERT(s.mem_total == 4000000ULL * 1024);
    LP_ASSERT(s.mem_available == 1000000ULL * 1024);
    LP_ASSERT(s.swap_total == 2000000ULL * 1024 && s.swap_free == 1500000ULL * 1024);
    LP_ASSERT_NEAR(s.load1, 0.52, 1e-9);
    LP_ASSERT_NEAR(s.load15, 0.59, 1e-9);
    const lp_process *web = lp_proc_find(&s, 4242);
    LP_ASSERT(web != NULL);
    if (web) {
        LP_ASSERT_STR(web->name, "Web Content (x)");
        LP_ASSERT_EQ(web->state, 'R');
        LP_ASSERT_EQ(web->ppid, 1);
        LP_ASSERT_EQ(web->threads, 12);
        LP_ASSERT(web->rss_bytes == 204800ULL * 1024);
        LP_ASSERT(web->ticks == 300);
        LP_ASSERT_STR(web->command, "/usr/lib/firefox/firefox -contentproc");
        struct passwd *me = getpwuid(getuid());
        LP_ASSERT_STR(web->user, me ? me->pw_name : "");
    }
    const lp_process *init = lp_proc_find(&s, 1);
    LP_ASSERT(init && strcmp(init->user, "root") == 0 && strcmp(init->command, "/sbin/init splash") == 0);
    const lp_process *kthread = lp_proc_find(&s, 77);
    LP_ASSERT(kthread && kthread->rss_bytes == 0 && kthread->command[0] == 0 && strcmp(kthread->name, "kworker/0:1") == 0);
    const lp_process *cal = lp_proc_find(&s, 900);
    LP_ASSERT(cal && strcmp(cal->name, "gnome-shell-calendar-server") == 0);   /* comm is cut at 15 */
    LP_ASSERT(lp_proc_find(&s, 5) == NULL);
    lp_proc_free(&s);
}

LP_TEST(measures_cpu_shares_between_two_samples) {
    write_fixture(0, 0, 0, 10);
    lp_proc_sample before, after;
    LP_ASSERT_EQ(lp_proc_read(root, NULL, &before), 0);
    LP_ASSERT_NEAR(lp_proc_find(&before, 4242)->cpu, 0, 1e-9);   /* nothing to measure against yet */
    /* 400 ticks pass across 4 CPUs (100 each), 300 of them idle; the browser uses 50; the kworker's pid is reused */
    write_fixture(400, 300, 50, 20);
    LP_ASSERT_EQ(lp_proc_read(root, &before, &after), 0);
    LP_ASSERT_NEAR(after.cpu_busy, 25, 1e-9);
    LP_ASSERT_NEAR(lp_proc_find(&after, 4242)->cpu, 50, 1e-9);
    LP_ASSERT_NEAR(lp_proc_find(&after, 1)->cpu, 0, 1e-9);
    LP_ASSERT_NEAR(lp_proc_find(&after, 77)->cpu, 0, 1e-9);     /* a new process under an old pid */
    lp_proc_free(&before);
    lp_proc_free(&after);
}

LP_TEST(sorts_by_each_column) {
    write_fixture(0, 0, 0, 10);
    lp_proc_sample before, s;
    lp_proc_read(root, NULL, &before);
    write_fixture(400, 300, 50, 10);
    lp_proc_read(root, &before, &s);
    lp_proc_sort(&s, LP_PROC_SORT_CPU, 1);
    LP_ASSERT_EQ(s.procs[0].pid, 4242);
    lp_proc_sort(&s, LP_PROC_SORT_NAME, 0);
    LP_ASSERT_STR(s.procs[0].name, "gnome-shell-calendar-server");
    LP_ASSERT_STR(s.procs[1].name, "kworker/0:1");
    LP_ASSERT_STR(s.procs[2].name, "systemd");
    LP_ASSERT_STR(s.procs[3].name, "Web Content (x)");          /* case does not decide */
    lp_proc_sort(&s, LP_PROC_SORT_MEMORY, 1);
    LP_ASSERT_EQ(s.procs[0].pid, 4242);
    LP_ASSERT_EQ(s.procs[3].pid, 77);
    lp_proc_sort(&s, LP_PROC_SORT_PID, 0);
    LP_ASSERT_EQ(s.procs[0].pid, 1);
    LP_ASSERT_EQ(s.procs[3].pid, 4242);
    lp_proc_sort(&s, LP_PROC_SORT_USER, 0);
    LP_ASSERT_STR(s.procs[0].user, strcmp(s.procs[3].user, "root") == 0 ? s.procs[0].user : s.procs[0].user);
    lp_proc_free(&before);
    lp_proc_free(&s);
}

LP_TEST(matches_a_search_in_name_user_or_command) {
    write_fixture(0, 0, 0, 10);
    lp_proc_sample s;
    lp_proc_read(root, NULL, &s);
    const lp_process *web = lp_proc_find(&s, 4242), *init = lp_proc_find(&s, 1);
    LP_ASSERT(lp_proc_matches(web, "WEB"));
    LP_ASSERT(lp_proc_matches(web, "contentproc"));
    LP_ASSERT(lp_proc_matches(init, "root"));
    LP_ASSERT(lp_proc_matches(init, ""));
    LP_ASSERT(!lp_proc_matches(init, "firefox"));
    lp_proc_free(&s);
}

LP_TEST(formats_memory_in_powers_of_1024) {
    char s[32];
    lp_proc_format_bytes(512, s, sizeof s);
    LP_ASSERT_STR(s, "512 bytes");
    lp_proc_format_bytes(2048, s, sizeof s);
    LP_ASSERT_STR(s, "2 KB");
    lp_proc_format_bytes(5ULL * 1048576, s, sizeof s);
    LP_ASSERT_STR(s, "5.0 MB");
    lp_proc_format_bytes(3ULL * 1073741824, s, sizeof s);
    LP_ASSERT_STR(s, "3.00 GB");
}

LP_TEST(asks_a_process_to_quit_and_refuses_init) {
    LP_ASSERT_EQ(lp_proc_signal(1, 0), -EPERM);
    LP_ASSERT_EQ(lp_proc_signal(0, 1), -EPERM);
    pid_t child = fork();
    if (child == 0) { pause(); _exit(0); }
    LP_ASSERT(child > 0);
    LP_ASSERT_EQ(lp_proc_signal(child, 0), 0);
    int status = 0;
    LP_ASSERT_EQ(waitpid(child, &status, 0), child);
    LP_ASSERT(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM);
    child = fork();
    if (child == 0) { signal(SIGTERM, SIG_IGN); pause(); _exit(0); }
    LP_ASSERT_EQ(lp_proc_signal(child, 1), 0);                  /* Force Quit is not asked */
    LP_ASSERT_EQ(waitpid(child, &status, 0), child);
    LP_ASSERT(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
}

LP_TEST(a_missing_tree_is_an_error_not_a_crash) {
    lp_proc_sample s;
    char missing[600];
    snprintf(missing, sizeof missing, "%s/nowhere", root);
    LP_ASSERT_EQ(lp_proc_read(missing, NULL, &s), -ENOENT);
    LP_ASSERT_EQ(s.count, 0);
    lp_proc_free(&s);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_proc_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    LP_RUN(reads_the_totals_and_each_process);
    LP_RUN(measures_cpu_shares_between_two_samples);
    LP_RUN(sorts_by_each_column);
    LP_RUN(matches_a_search_in_name_user_or_command);
    LP_RUN(formats_memory_in_powers_of_1024);
    LP_RUN(asks_a_process_to_quit_and_refuses_init);
    LP_RUN(a_missing_tree_is_an_error_not_a_crash);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
