/* What System Settings reads about the machine and its services, as parsers
 * over the text the system hands out — /proc/cpuinfo and /proc/meminfo, the
 * device tree's model, `ip -brief address`, `wpctl get-volume`, `timedatectl
 * show`, `iwctl station … get-networks` — and a reader for the About pane. The
 * commands themselves run as lp_jobs in the app, so everything here is plain
 * text in, fields out, and testable anywhere. Linux only (PARITY D15). */
#ifndef MARYUI_LP_SYSINFO_H
#define MARYUI_LP_SYSINFO_H

#include <stddef.h>
#include <stdint.h>

typedef struct lp_about {
    char os[128];           /* PRETTY_NAME */
    char kernel[128];       /* "Linux 6.8.0-139-generic" */
    char cpu[128];          /* "Cortex-A76", "Apple silicon (virtual)" */
    int cores;
    uint64_t memory;        /* bytes */
    char hostname[64];
    char model[128];        /* the board, from the device tree ("Raspberry Pi 5 Model B Rev 1.0"); "" in a VM */
} lp_about;

/* The processor's name and the count of cores in a /proc/cpuinfo. */
void lp_sysinfo_parse_cpuinfo(const char *text, char *cpu, size_t n, int *cores);
/* MemTotal from a /proc/meminfo, in bytes. */
uint64_t lp_sysinfo_parse_memtotal(const char *text);
/* Everything the About pane shows, read from this machine. */
void lp_sysinfo_about(lp_about *out);

/* A value from KEY=VALUE lines (timedatectl show, os-release; quotes dropped). 1 when the key is there. */
int lp_sysinfo_value(const char *text, const char *key, char *out, size_t n);

typedef struct lp_net_link {
    char name[32];          /* "enp0s1", "wlan0" */
    char state[16];         /* "UP", "DOWN", "DORMANT" */
    char addresses[192];    /* "192.168.64.2/24 fe80::…/64" */
    int wireless;
} lp_net_link;

/* `ip -brief address` → the links other than loopback. Returns the count. */
int lp_sysinfo_parse_links(const char *text, lp_net_link *out, int max);

typedef struct lp_wifi_network {
    char ssid[64];
    char security[16];      /* "psk", "open", "8021x" */
    int signal;             /* 0 … 4 bars */
    int connected;
} lp_wifi_network;

/* `iwctl station <device> get-networks`, colours and all → the networks. Returns the count. */
int lp_sysinfo_parse_wifi(const char *text, lp_wifi_network *out, int max);

/* `wpctl get-volume @DEFAULT_AUDIO_SINK@` ("Volume: 0.40 [MUTED]") → volume and muted. 1 when it parsed. */
int lp_sysinfo_parse_volume(const char *text, double *volume, int *muted);

#endif
