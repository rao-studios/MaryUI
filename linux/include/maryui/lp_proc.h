/* Activity Monitor's model: the processes under a /proc tree — the real one,
 * or a test's fixture — with each one's name, user, state, threads, memory
 * and share of a CPU between two samples; the machine's CPU and memory
 * totals; sorting, matching a search, and asking a process to quit. Linux only
 * in use (PARITY D15); the parsing is plain C and its tests run on a Mac
 * against fixtures. */
#ifndef MARYUI_LP_PROC_H
#define MARYUI_LP_PROC_H

#include <stddef.h>
#include <stdint.h>

typedef struct lp_process {
    int pid, ppid;
    char name[64];          /* comm, or the command's own name when comm was cut at 15 characters */
    char user[32];
    char state;             /* R running, S sleeping, D disk, Z zombie, T stopped, I idle */
    int threads;
    uint64_t rss_bytes;     /* resident memory (VmRSS); 0 for kernel threads */
    uint64_t ticks;         /* utime + stime, in clock ticks */
    uint64_t start_ticks;   /* when it started: tells a reused pid from the process that had it */
    double cpu;             /* percent of one CPU since the previous sample; above 100 for many busy threads */
    char command[256];      /* the command line, arguments separated by spaces; "" for kernel threads */
} lp_process;

typedef struct lp_proc_sample {
    lp_process *procs;
    int count, cap;
    uint64_t total_ticks;           /* the "cpu" line of stat, summed */
    uint64_t idle_ticks;            /* idle + iowait */
    int cpus;
    double cpu_busy;                /* percent of all CPUs busy since the previous sample */
    uint64_t mem_total, mem_available, swap_total, swap_free;   /* bytes */
    double load1, load5, load15;
} lp_proc_sample;

enum lp_proc_sort { LP_PROC_SORT_CPU, LP_PROC_SORT_MEMORY, LP_PROC_SORT_NAME, LP_PROC_SORT_PID, LP_PROC_SORT_USER };

/* Reads root (NULL: "/proc") into out, measuring CPU shares against prev when given. 0 or -errno. */
int lp_proc_read(const char *root, const lp_proc_sample *prev, lp_proc_sample *out);
void lp_proc_free(lp_proc_sample *s);
void lp_proc_sort(lp_proc_sample *s, enum lp_proc_sort by, int descending);
const lp_process *lp_proc_find(const lp_proc_sample *s, int pid);
/* 1 when query (case-insensitive) is in the name, the user or the command line; an empty query matches all. */
int lp_proc_matches(const lp_process *p, const char *query);
/* SIGTERM, or SIGKILL when force. 0 or -errno: -EPERM for pid 1 and below, and for another user's process. */
int lp_proc_signal(int pid, int force);
/* Memory the way it is counted, in powers of 1024: "512 bytes", "2 KB", "5.0 MB", "3.00 GB". */
void lp_proc_format_bytes(uint64_t bytes, char *out, size_t n);

#endif
