/* Terminal's pseudo-terminal (lp_pty.h). */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1   /* forkpty, TIOCSWINSZ */
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

#include "maryui/lp_pty.h"

int lp_pty_spawn(lp_pty *p, const char *const *argv, const char *cwd, int rows, int cols) {
    memset(p, 0, sizeof *p);
    p->fd = -1;
    struct winsize ws = { .ws_row = (unsigned short)(rows > 0 ? rows : 24), .ws_col = (unsigned short)(cols > 0 ? cols : 80) };
    const char *shell = getenv("SHELL");
    if (!shell || !*shell || access(shell, X_OK) != 0) shell = "/bin/sh";
    int fd;
    pid_t pid = forkpty(&fd, NULL, NULL, &ws);
    if (pid < 0) return -errno;
    if (pid == 0) {
        /* The child: forkpty gave it a new session with the pty as its controlling terminal.
         * Undo what the compositor set for itself, so the shell's jobs behave. */
        static const int SIGNALS[] = { SIGPIPE, SIGCHLD, SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGTTIN, SIGTTOU };
        for (size_t i = 0; i < sizeof SIGNALS / sizeof SIGNALS[0]; i++) signal(SIGNALS[i], SIG_DFL);
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, NULL);
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERM_PROGRAM", "MaryOS Terminal", 1);
        const char *dir = cwd && *cwd ? cwd : getenv("HOME");
        if (dir && chdir(dir) != 0) { /* stay where the desktop was */ }
        if (argv && argv[0]) {
            execvp(argv[0], (char *const *)argv);
        } else {
            const char *base = strrchr(shell, '/');
            char login[64];
            snprintf(login, sizeof login, "-%s", base ? base + 1 : shell);   /* a login shell, as a terminal window's is */
            execl(shell, login, (char *)NULL);
        }
        _exit(127);
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    p->fd = fd;
    p->pid = pid;
    return 0;
}

int lp_pty_resize(lp_pty *p, int rows, int cols, int px_w, int px_h) {
    if (p->fd < 0) return -EBADF;
    struct winsize ws = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)cols,
                          .ws_xpixel = (unsigned short)px_w, .ws_ypixel = (unsigned short)px_h };
    return ioctl(p->fd, TIOCSWINSZ, &ws) == 0 ? 0 : -errno;
}

ssize_t lp_pty_read(lp_pty *p, char *buf, size_t n) {
    if (p->fd < 0) return -1;
    for (;;) {
        ssize_t r = read(p->fd, buf, n);
        if (r > 0) return r;
        if (r == 0) return -1;                      /* end of file: the other side is gone (macOS) */
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;                                  /* EIO: the other side is gone (Linux) */
    }
}

int lp_pty_write(lp_pty *p, const char *data, size_t n) {
    if (p->fd < 0) return -EBADF;
    while (n > 0) {
        ssize_t w = write(p->fd, data, n);
        if (w > 0) { data += w; n -= (size_t)w; continue; }
        if (w < 0 && errno == EINTR) continue;
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            /* The program is not reading. Wait a moment, then give up on the rest
             * rather than stall the compositor that called us. */
            struct pollfd pfd = { .fd = p->fd, .events = POLLOUT };
            if (poll(&pfd, 1, 200) <= 0) return -EAGAIN;
            continue;
        }
        return -errno;
    }
    return 0;
}

int lp_pty_poll_exit(lp_pty *p) {
    if (p->exited || p->pid <= 0) return 1;
    int status = 0;
    pid_t r = waitpid(p->pid, &status, WNOHANG);
    if (r == p->pid) {
        p->exited = 1;
        p->status = status;
        p->pid = 0;
        return 1;
    }
    if (r < 0 && errno == ECHILD) {   /* the host's SIGCHLD handler reaped it first */
        p->exited = 1;
        p->pid = 0;
        return 1;
    }
    return 0;
}

void lp_pty_foreground(const lp_pty *p, char *out, size_t n) {
    if (n) out[0] = 0;
    if (p->fd < 0 || p->exited || p->pid <= 0) return;
    pid_t group = tcgetpgrp(p->fd);
    if (group <= 0 || group == p->pid) return;
#ifdef __linux__
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/comm", (int)group);
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)n, f)) out[strcspn(out, "\n")] = 0;
    fclose(f);
#else
    snprintf(out, n, "process %d", (int)group);
#endif
}

void lp_pty_close(lp_pty *p) {
    if (p->pid > 0 && !p->exited) {
        kill(-p->pid, SIGHUP);   /* the shell leads its session's process group */
        kill(p->pid, SIGHUP);
        lp_pty_poll_exit(p);
    }
    if (p->fd >= 0) close(p->fd);
    p->fd = -1;
}
