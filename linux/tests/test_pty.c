/* Terminal's pseudo-terminal: a program runs on a terminal of the asked size
 * with TERM set, input reaches it, a resize reaches it, its exit is noticed,
 * closing hangs it up, and the job in front has a name. Plain POSIX, so these
 * run on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "lp_test.h"
#include "maryui/lp_pty.h"

/* Reads into acc until `needle` appears or `ms` pass. 1 when it appeared. The waits are long
 * because a loaded build host can take seconds to schedule a shell; a pass returns at once. */
static int read_until(lp_pty *p, const char *needle, char *acc, size_t cap, int ms) {
    size_t len = strlen(acc);
    for (int waited = 0; waited < ms; waited += 10) {
        struct pollfd pfd = { .fd = p->fd, .events = POLLIN };
        poll(&pfd, 1, 10);
        char buf[512];
        ssize_t r;
        while ((r = lp_pty_read(p, buf, sizeof buf)) > 0) {
            size_t take = (size_t)r < cap - 1 - len ? (size_t)r : cap - 1 - len;
            memcpy(acc + len, buf, take);
            len += take;
            acc[len] = 0;
        }
        if (strstr(acc, needle)) return 1;
        if (r < 0) return 0;
    }
    return 0;
}

static int wait_gone(pid_t pid) {
    for (int i = 0; i < 300; i++) {
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || (r < 0 && errno == ECHILD)) return 1;
        poll(NULL, 0, 10);
    }
    return 0;
}

LP_TEST(runs_a_program_on_a_terminal_of_the_asked_size) {
    lp_pty p;
    const char *const argv[] = { "/bin/sh", "-c", "stty size; printf 'term=%s\\n' \"$TERM\"", NULL };
    LP_ASSERT_EQ(lp_pty_spawn(&p, argv, NULL, 30, 100), 0);
    char acc[4096] = "";
    LP_ASSERT(read_until(&p, "term=xterm-256color", acc, sizeof acc, 10000));
    LP_ASSERT(strstr(acc, "30 100") != NULL);
    lp_pty_close(&p);
    LP_ASSERT_EQ(p.fd, -1);
}

LP_TEST(starts_in_the_asked_directory) {
    lp_pty p;
    const char *const argv[] = { "/bin/sh", "-c", "printf 'at=%s;\\n' \"$(pwd)\"", NULL };
    LP_ASSERT_EQ(lp_pty_spawn(&p, argv, "/", 24, 80), 0);
    char acc[4096] = "";
    LP_ASSERT(read_until(&p, "at=/;", acc, sizeof acc, 10000));
    lp_pty_close(&p);
}

LP_TEST(passes_input_through_and_resizes) {
    lp_pty p;
    /* A read-eval loop rather than an interactive shell: macOS's /bin/sh is bash with readline,
     * which flushes input typed ahead of its prompt, and a test cannot wait for a prompt it
     * does not know. The loop reads each line the way the terminal delivers it. */
    const char *const argv[] = { "/bin/sh", "-c", "while read -r line; do eval \"$line\"; done", NULL };
    LP_ASSERT_EQ(lp_pty_spawn(&p, argv, "/", 24, 80), 0);
    const char *echo = "echo marker-$((6*7))\n";   /* the echoed command says $((6*7)); only the output says 42 */
    LP_ASSERT_EQ(lp_pty_write(&p, echo, strlen(echo)), 0);
    char acc[8192] = "";
    LP_ASSERT(read_until(&p, "marker-42", acc, sizeof acc, 10000));
    LP_ASSERT_EQ(lp_pty_resize(&p, 40, 120, 960, 680), 0);
    acc[0] = 0;
    const char *size = "stty size\n";
    LP_ASSERT_EQ(lp_pty_write(&p, size, strlen(size)), 0);
    LP_ASSERT(read_until(&p, "40 120", acc, sizeof acc, 10000));
    lp_pty_close(&p);
}

LP_TEST(notices_when_the_program_exits) {
    lp_pty p;
    const char *const argv[] = { "/bin/sh", "-c", "exit 3", NULL };
    LP_ASSERT_EQ(lp_pty_spawn(&p, argv, NULL, 24, 80), 0);
    int exited = 0;
    for (int i = 0; i < 300 && !exited; i++) { exited = lp_pty_poll_exit(&p); if (!exited) poll(NULL, 0, 10); }
    LP_ASSERT(exited);
    LP_ASSERT(WIFEXITED(p.status) && WEXITSTATUS(p.status) == 3);
    char buf[64];
    ssize_t r = 0;
    for (int i = 0; i < 100 && (r = lp_pty_read(&p, buf, sizeof buf)) >= 0; i++) poll(NULL, 0, 10);
    LP_ASSERT_EQ(r, -1);   /* and the terminal says so */
    lp_pty_close(&p);
}

LP_TEST(hangs_up_on_close) {
    lp_pty p;
    const char *const argv[] = { "/bin/sh", "-c", "sleep 30", NULL };
    LP_ASSERT_EQ(lp_pty_spawn(&p, argv, NULL, 24, 80), 0);
    pid_t pid = p.pid;
    poll(NULL, 0, 50);
    lp_pty_close(&p);
    LP_ASSERT(wait_gone(pid));
    LP_ASSERT(kill(pid, 0) != 0);
}

LP_TEST(names_the_job_in_front) {
    lp_pty p;
    const char *const argv[] = { "/bin/sh", NULL };
    LP_ASSERT_EQ(lp_pty_spawn(&p, argv, "/", 24, 80), 0);
    char job[64];
    poll(NULL, 0, 100);
    lp_pty_foreground(&p, job, sizeof job);
    LP_ASSERT_STR(job, "");            /* the shell itself */
    const char *cmd = "sleep 5\n";
    LP_ASSERT_EQ(lp_pty_write(&p, cmd, strlen(cmd)), 0);
    for (int i = 0; i < 300 && !job[0]; i++) {
        char drain[256];
        while (lp_pty_read(&p, drain, sizeof drain) > 0) {}
        lp_pty_foreground(&p, job, sizeof job);
        if (!job[0]) poll(NULL, 0, 10);
    }
#ifdef __linux__
    LP_ASSERT_STR(job, "sleep");
#else
    LP_ASSERT(job[0] != 0);
#endif
    lp_pty_close(&p);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    LP_RUN(runs_a_program_on_a_terminal_of_the_asked_size);
    LP_RUN(starts_in_the_asked_directory);
    LP_RUN(passes_input_through_and_resizes);
    LP_RUN(notices_when_the_program_exits);
    LP_RUN(hangs_up_on_close);
    LP_RUN(names_the_job_in_front);
    LP_TEST_MAIN_END();
}
