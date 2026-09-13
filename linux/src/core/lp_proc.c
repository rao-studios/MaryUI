/* Activity Monitor's processes (lp_proc.h). */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#include "maryui/lp_proc.h"

/* Reads root/rel whole (up to n - 1 bytes, NUL-terminated). The length, or -errno. */
static int slurp(const char *root, const char *rel, char *buf, size_t n) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", root, rel);
    FILE *f = fopen(path, "r");
    if (!f) return -errno;
    size_t len = fread(buf, 1, n - 1, f);
    fclose(f);
    buf[len] = 0;
    return (int)len;
}

/* Next line of a NUL-terminated buffer, cutting it in place; NULL at the end. */
static char *next_line(char **cursor) {
    char *line = *cursor;
    if (!line || !*line) return NULL;
    char *nl = strchr(line, '\n');
    if (nl) { *nl = 0; *cursor = nl + 1; } else { *cursor = line + strlen(line); }
    return line;
}

static void read_cpu(const char *root, lp_proc_sample *s) {
    size_t cap = 1 << 16;   /* stat grows with the number of CPUs and interrupts */
    char *buf = malloc(cap);
    if (!buf) return;
    if (slurp(root, "stat", buf, cap) > 0) {
        char *cursor = buf, *line;
        int cpus = 0;
        while ((line = next_line(&cursor))) {
            if (strncmp(line, "cpu ", 4) == 0) {
                unsigned long long v[8] = { 0 };
                int got = sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
                for (int i = 0; i < got; i++) s->total_ticks += v[i];   /* guest time is already inside user */
                s->idle_ticks = v[3] + v[4];
            } else if (strncmp(line, "cpu", 3) == 0 && isdigit((unsigned char)line[3])) {
                cpus++;
            }
        }
        s->cpus = cpus;
    }
    free(buf);
    if (s->cpus < 1) s->cpus = 1;
}

static void read_memory(const char *root, lp_proc_sample *s) {
    char buf[8192];
    if (slurp(root, "meminfo", buf, sizeof buf) <= 0) return;
    char *cursor = buf, *line;
    while ((line = next_line(&cursor))) {
        unsigned long long kb;
        if (sscanf(line, "MemTotal: %llu kB", &kb) == 1) s->mem_total = kb * 1024;
        else if (sscanf(line, "MemAvailable: %llu kB", &kb) == 1) s->mem_available = kb * 1024;
        else if (sscanf(line, "SwapTotal: %llu kB", &kb) == 1) s->swap_total = kb * 1024;
        else if (sscanf(line, "SwapFree: %llu kB", &kb) == 1) s->swap_free = kb * 1024;
    }
}

/* uid → name, remembered: a refresh asks for the same few users hundreds of times. */
static void user_name(unsigned uid, char *out, size_t n) {
    static struct { unsigned uid; char name[32]; int used; } cache[32];
    static int next;
    for (int i = 0; i < 32; i++) if (cache[i].used && cache[i].uid == uid) { snprintf(out, n, "%s", cache[i].name); return; }
    struct passwd *pw = getpwuid((uid_t)uid);
    int slot = next++ % 32;
    cache[slot].used = 1;
    cache[slot].uid = uid;
    if (pw && pw->pw_name) snprintf(cache[slot].name, sizeof cache[slot].name, "%s", pw->pw_name);
    else snprintf(cache[slot].name, sizeof cache[slot].name, "%u", uid);
    snprintf(out, n, "%s", cache[slot].name);
}

static int read_process(const char *root, int pid, lp_process *p) {
    char rel[64], buf[4096];
    memset(p, 0, sizeof *p);
    p->pid = pid;
    snprintf(rel, sizeof rel, "%d/stat", pid);
    if (slurp(root, rel, buf, sizeof buf) <= 0) return -1;   /* it went between the listing and now */
    /* comm may hold spaces and parentheses of its own: it ends at the last ')' */
    char *open = strchr(buf, '('), *close = strrchr(buf, ')');
    if (!open || !close || close < open || !close[1]) return -1;
    size_t name_len = (size_t)(close - open - 1);
    if (name_len >= sizeof p->name) name_len = sizeof p->name - 1;
    memcpy(p->name, open + 1, name_len);
    p->name[name_len] = 0;
    /* state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime priority nice threads itreal starttime */
    char state = '?';
    int ppid = 0;
    unsigned long long utime = 0, stime = 0, start = 0;
    long threads = 0;
    if (sscanf(close + 2, "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %*d %*d %*d %*d %ld %*d %llu",
               &state, &ppid, &utime, &stime, &threads, &start) < 6) return -1;
    p->state = state;
    p->ppid = ppid;
    p->ticks = utime + stime;
    p->start_ticks = start;
    p->threads = (int)threads;

    unsigned uid = 0;
    snprintf(rel, sizeof rel, "%d/status", pid);
    if (slurp(root, rel, buf, sizeof buf) > 0) {
        char *cursor = buf, *line;
        while ((line = next_line(&cursor))) {
            unsigned long long kb;
            int t;
            if (sscanf(line, "Uid: %u", &uid) == 1) continue;
            if (sscanf(line, "VmRSS: %llu kB", &kb) == 1) p->rss_bytes = kb * 1024;
            else if (sscanf(line, "Threads: %d", &t) == 1) p->threads = t;
        }
    }
    user_name(uid, p->user, sizeof p->user);

    snprintf(rel, sizeof rel, "%d/cmdline", pid);
    int len = slurp(root, rel, buf, sizeof buf);
    if (len > 0) {
        while (len > 0 && buf[len - 1] == 0) len--;   /* the arguments end in NUL; the last one is not a separator */
        if (len >= (int)sizeof p->command) len = (int)sizeof p->command - 1;
        for (int i = 0; i < len; i++) p->command[i] = buf[i] ? buf[i] : ' ';
        p->command[len] = 0;
        /* comm is cut at 15 characters; the command's own name is not */
        if (strlen(p->name) == 15) {
            char first[256];
            snprintf(first, sizeof first, "%s", p->command);
            char *space = strchr(first, ' ');
            if (space) *space = 0;
            const char *base = strrchr(first, '/');
            base = base ? base + 1 : first;
            if (strncmp(base, p->name, 15) == 0) snprintf(p->name, sizeof p->name, "%s", base);
        }
    }
    return 0;
}

const lp_process *lp_proc_find(const lp_proc_sample *s, int pid) {
    for (int i = 0; i < s->count; i++) if (s->procs[i].pid == pid) return &s->procs[i];
    return NULL;
}

int lp_proc_read(const char *root, const lp_proc_sample *prev, lp_proc_sample *out) {
    memset(out, 0, sizeof *out);
    if (!root) root = "/proc";
    DIR *dir = opendir(root);
    if (!dir) return -errno;
    read_cpu(root, out);
    read_memory(root, out);
    char buf[256];
    if (slurp(root, "loadavg", buf, sizeof buf) > 0) sscanf(buf, "%lf %lf %lf", &out->load1, &out->load5, &out->load15);
    struct dirent *e;
    while ((e = readdir(dir))) {
        char *end;
        long pid = strtol(e->d_name, &end, 10);
        if (*end || pid <= 0) continue;
        if (out->count == out->cap) {
            int cap = out->cap ? out->cap * 2 : 256;
            lp_process *grown = realloc(out->procs, sizeof *grown * (size_t)cap);
            if (!grown) break;
            out->procs = grown;
            out->cap = cap;
        }
        if (read_process(root, (int)pid, &out->procs[out->count]) == 0) out->count++;
    }
    closedir(dir);

    if (prev && prev->total_ticks && out->total_ticks > prev->total_ticks) {
        double elapsed = (double)(out->total_ticks - prev->total_ticks);
        double idle = out->idle_ticks >= prev->idle_ticks ? (double)(out->idle_ticks - prev->idle_ticks) : 0;
        out->cpu_busy = elapsed > idle ? (elapsed - idle) / elapsed * 100 : 0;
        double per_cpu = elapsed / out->cpus;   /* the ticks one CPU lived through */
        for (int i = 0; i < out->count; i++) {
            lp_process *p = &out->procs[i];
            const lp_process *was = lp_proc_find(prev, p->pid);
            if (was && was->start_ticks == p->start_ticks && p->ticks >= was->ticks) p->cpu = (double)(p->ticks - was->ticks) / per_cpu * 100;
        }
    }
    return 0;
}

void lp_proc_free(lp_proc_sample *s) {
    free(s->procs);
    memset(s, 0, sizeof *s);
}

static enum lp_proc_sort sort_by;
static int sort_descending;

static int compare(const void *a, const void *b) {
    const lp_process *x = a, *y = b;
    int c = 0;
    switch (sort_by) {
    case LP_PROC_SORT_CPU: c = x->cpu < y->cpu ? -1 : x->cpu > y->cpu; break;
    case LP_PROC_SORT_MEMORY: c = x->rss_bytes < y->rss_bytes ? -1 : x->rss_bytes > y->rss_bytes; break;
    case LP_PROC_SORT_NAME: c = strcasecmp(x->name, y->name); break;
    case LP_PROC_SORT_PID: c = x->pid - y->pid; break;
    case LP_PROC_SORT_USER: c = strcasecmp(x->user, y->user); if (!c) c = strcasecmp(x->name, y->name); break;
    }
    if (sort_descending) c = -c;
    return c ? c : x->pid - y->pid;   /* a stable order for equals, whichever way */
}

void lp_proc_sort(lp_proc_sample *s, enum lp_proc_sort by, int descending) {
    sort_by = by;
    sort_descending = descending;
    if (s->count > 1) qsort(s->procs, (size_t)s->count, sizeof *s->procs, compare);
}

static int contains(const char *haystack, const char *needle) {
    size_t n = strlen(needle);
    for (; *haystack; haystack++) if (strncasecmp(haystack, needle, n) == 0) return 1;
    return 0;
}

int lp_proc_matches(const lp_process *p, const char *query) {
    if (!query || !*query) return 1;
    return contains(p->name, query) || contains(p->user, query) || contains(p->command, query);
}

int lp_proc_signal(int pid, int force) {
    if (pid <= 1) return -EPERM;
    return kill((pid_t)pid, force ? SIGKILL : SIGTERM) == 0 ? 0 : -errno;
}

void lp_proc_format_bytes(uint64_t bytes, char *out, size_t n) {
    if (bytes < 1024) snprintf(out, n, "%llu bytes", (unsigned long long)bytes);
    else if (bytes < 1024 * 1024) snprintf(out, n, "%.0f KB", bytes / 1024.0);
    else if (bytes < 1024ULL * 1024 * 1024) snprintf(out, n, "%.1f MB", bytes / 1048576.0);
    else snprintf(out, n, "%.2f GB", bytes / 1073741824.0);
}
