/* A program on a pseudo-terminal, for Terminal: forkpty with TERM set and the
 * window's size, non-blocking reads, writes that finish, the foreground job's
 * name, and hanging up. Host-agnostic — the caller watches `fd` (for the
 * desktop, lp_desktop_add_fd). Linux only in use (PARITY D15), but plain POSIX,
 * so it builds and tests on a Mac too. */
#ifndef MARYUI_LP_PTY_H
#define MARYUI_LP_PTY_H

#include <stddef.h>
#include <sys/types.h>

typedef struct lp_pty {
    int fd;          /* the master side, non-blocking; -1 when there is none */
    pid_t pid;       /* the child; 0 once it has been reaped */
    int exited;      /* the child is gone */
    int status;      /* its waitpid status, when this side reaped it */
} lp_pty;

/* Starts argv (NULL: $SHELL, else /bin/sh, as a login shell) in cwd (NULL: $HOME)
 * on a rows×cols terminal. 0 or -errno; p->fd is -1 on failure. */
int lp_pty_spawn(lp_pty *p, const char *const *argv, const char *cwd, int rows, int cols);
/* Tells the program its terminal changed size (it gets SIGWINCH). 0 or -errno. */
int lp_pty_resize(lp_pty *p, int rows, int cols, int px_w, int px_h);
/* What is waiting: > 0 bytes, 0 when nothing is, -1 once the program's side has closed. */
ssize_t lp_pty_read(lp_pty *p, char *buf, size_t n);
/* All of it, waiting briefly on a full pty rather than hanging the caller. 0 or -errno. */
int lp_pty_write(lp_pty *p, const char *data, size_t n);
/* Reaps the child if it has gone (or notes that the host reaped it). 1 when it has exited. */
int lp_pty_poll_exit(lp_pty *p);
/* The name of the job in front ("vim"), or "" while the shell itself is. */
void lp_pty_foreground(const lp_pty *p, char *out, size_t n);
/* Hangs up — SIGHUP to the program's process group — closes the fd and reaps without waiting. */
void lp_pty_close(lp_pty *p);

#endif
