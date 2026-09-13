/* A command run for an app without blocking the desktop — Disk Utility's
 * udisksctl, System Settings' timedatectl: argv is spawned with its output
 * (stdout and stderr together) read through a pipe the host watches
 * (lp_desktop_add_fd), and a callback gets the exit status and that output once
 * it has finished. The status comes from a small waiter process rather than
 * waitpid in the desktop, whose SIGCHLD handler reaps its own children before
 * anyone could ask. Without host event sources the job runs to its end inside
 * lp_job_run. Linux only in use (PARITY D15); plain POSIX. */
#ifndef MARYUI_LP_JOB_H
#define MARYUI_LP_JOB_H

#include <stddef.h>

struct lp_desktop;

#define LP_JOB_OUTPUT_MAX 65536   /* output kept; the rest is read and dropped so the program never blocks */

typedef struct lp_job lp_job;

/* status: the exit code (127 when argv[0] could not be run), or minus the signal that ended it.
 * output is NUL-terminated and valid only during the call. */
typedef void (*lp_job_done_fn)(int status, const char *output, void *user);

/* Starts argv. Returns the running job, or NULL when it already finished (no event sources: done has
 * been called) or could not start (done is called with -errno … never, and NULL is returned: see errno). */
lp_job *lp_job_run(struct lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user);
/* Forgets the callback — its owner is going away. The program still runs to its end and is reaped. */
void lp_job_cancel(lp_job *job);

#endif
