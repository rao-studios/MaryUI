/* Commands run for apps (lp_job.h). */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_job.h"

struct lp_job {
    lp_desktop *desk;
    pid_t waiter;
    int out_fd, status_fd;
    lp_source *source;
    lp_job_done_fn done;
    void *user;
    char out[LP_JOB_OUTPUT_MAX];
    size_t len;
};

/* Reads what is waiting. 1 once the program's side of the pipe has closed. */
static int drain(lp_job *j) {
    char buf[8192];
    for (;;) {
        ssize_t r = read(j->out_fd, buf, sizeof buf);
        if (r > 0) {
            size_t room = sizeof j->out - 1 - j->len, take = (size_t)r < room ? (size_t)r : room;
            memcpy(j->out + j->len, buf, take);
            j->len += take;
            continue;
        }
        if (r == 0) return 1;
        if (errno == EINTR) continue;
        return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : 1;
    }
}

static void finish(lp_job *j) {
    if (j->source) lp_desktop_remove_source(j->desk, j->source);
    j->source = NULL;
    close(j->out_fd);
    /* the waiter writes the program's status as soon as it has it, and then exits */
    int status = -1;
    ssize_t got = 0;
    while (got < (ssize_t)sizeof status) {
        ssize_t r = read(j->status_fd, (char *)&status + got, sizeof status - (size_t)got);
        if (r > 0) got += r;
        else if (r < 0 && errno == EINTR) continue;
        else break;
    }
    if (got != sizeof status) status = -1;
    close(j->status_fd);
    waitpid(j->waiter, NULL, 0);   /* ECHILD when the desktop's handler got there first: either way it is gone */
    j->out[j->len] = 0;
    if (j->done) j->done(status, j->out, j->user);
    free(j);
}

static int on_output(int fd, uint32_t mask, void *data) {
    lp_job *j = data;
    if (drain(j) || (mask & (LP_SOURCE_HANGUP | LP_SOURCE_ERROR))) finish(j);
    return 0;
}

lp_job *lp_job_run(lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user) {
    if (!argv || !argv[0]) { errno = EINVAL; return NULL; }
    int out[2], st[2];
    if (pipe(out) != 0) return NULL;
    if (pipe(st) != 0) { close(out[0]); close(out[1]); return NULL; }
    lp_job *j = calloc(1, sizeof *j);
    if (!j) { close(out[0]); close(out[1]); close(st[0]); close(st[1]); errno = ENOMEM; return NULL; }
    pid_t waiter = fork();
    if (waiter < 0) {
        int e = errno;
        close(out[0]); close(out[1]); close(st[0]); close(st[1]);
        free(j);
        errno = e;
        return NULL;
    }
    if (waiter == 0) {
        /* The waiter: it runs the program as its own child, so this process — not the desktop — reaps it. */
        close(out[0]);
        close(st[0]);
        signal(SIGCHLD, SIG_DFL);
        pid_t child = fork();
        if (child == 0) {
            close(st[1]);
            dup2(out[1], STDOUT_FILENO);
            dup2(out[1], STDERR_FILENO);
            if (out[1] > STDERR_FILENO) close(out[1]);
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) { dup2(devnull, STDIN_FILENO); if (devnull > STDIN_FILENO) close(devnull); }
            signal(SIGPIPE, SIG_DFL);
            execvp(argv[0], (char *const *)argv);
            dprintf(STDERR_FILENO, "cannot run %s: %s\n", argv[0], strerror(errno));
            _exit(127);
        }
        close(out[1]);   /* only the program writes output now: its exit closes the pipe */
        int status = -1, raw = 0;
        if (child > 0) {
            while (waitpid(child, &raw, 0) < 0 && errno == EINTR) {}
            status = WIFEXITED(raw) ? WEXITSTATUS(raw) : WIFSIGNALED(raw) ? -WTERMSIG(raw) : -1;
        }
        ssize_t ignored = write(st[1], &status, sizeof status);
        (void)ignored;
        _exit(0);
    }
    close(out[1]);
    close(st[1]);
    fcntl(out[0], F_SETFD, FD_CLOEXEC);
    fcntl(st[0], F_SETFD, FD_CLOEXEC);
    j->desk = d;
    j->waiter = waiter;
    j->out_fd = out[0];
    j->status_fd = st[0];
    j->done = done;
    j->user = user;
    if (d) {
        fcntl(out[0], F_SETFL, fcntl(out[0], F_GETFL) | O_NONBLOCK);
        j->source = lp_desktop_add_fd(d, out[0], LP_SOURCE_READABLE, on_output, j);
    }
    if (!j->source) {
        /* no event loop to come back on: see it through here */
        fcntl(out[0], F_SETFL, fcntl(out[0], F_GETFL) & ~O_NONBLOCK);
        while (!drain(j)) {}
        finish(j);
        return NULL;
    }
    return j;
}

void lp_job_cancel(lp_job *job) {
    if (job) job->done = NULL;
}
