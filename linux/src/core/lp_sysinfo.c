/* System Settings' readers (lp_sysinfo.h). */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "maryui/lp_files.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_sysinfo.h"

static const char *next_line(const char **cursor, size_t *len) {
    const char *line = *cursor;
    if (!line || !*line) return NULL;
    const char *nl = strchr(line, '\n');
    *len = nl ? (size_t)(nl - line) : strlen(line);
    *cursor = nl ? nl + 1 : line + *len;
    return line;
}

static void copy_trimmed(const char *from, size_t len, char *out, size_t n) {
    while (len && isspace((unsigned char)*from)) { from++; len--; }
    while (len && isspace((unsigned char)from[len - 1])) len--;
    if (len >= n) len = n - 1;
    memcpy(out, from, len);
    out[len] = 0;
}

/* ARM's cores by their part number, for the arm64 cpuinfo that names none. */
static const char *arm_core(unsigned implementer, unsigned part) {
    if (implementer == 0x61) return "Apple silicon (virtual)";
    if (implementer != 0x41) return NULL;
    switch (part) {
    case 0xd03: return "Cortex-A53";
    case 0xd04: return "Cortex-A35";
    case 0xd05: return "Cortex-A55";
    case 0xd07: return "Cortex-A57";
    case 0xd08: return "Cortex-A72";
    case 0xd09: return "Cortex-A73";
    case 0xd0a: return "Cortex-A75";
    case 0xd0b: return "Cortex-A76";
    case 0xd0d: return "Cortex-A77";
    case 0xd41: return "Cortex-A78";
    default: return NULL;
    }
}

void lp_sysinfo_parse_cpuinfo(const char *text, char *cpu, size_t n, int *cores) {
    if (n) cpu[0] = 0;
    *cores = 0;
    unsigned implementer = 0, part = 0;
    const char *cursor = text, *line;
    size_t len;
    while ((line = next_line(&cursor, &len))) {
        const char *colon = memchr(line, ':', len);
        if (!colon) continue;
        char key[64];
        copy_trimmed(line, (size_t)(colon - line), key, sizeof key);
        char value[160];
        copy_trimmed(colon + 1, len - (size_t)(colon + 1 - line), value, sizeof value);
        if (strcmp(key, "processor") == 0) (*cores)++;
        else if ((strcmp(key, "model name") == 0 || strcmp(key, "Model") == 0) && !cpu[0]) snprintf(cpu, n, "%s", value);
        else if (strcmp(key, "CPU implementer") == 0) implementer = (unsigned)strtoul(value, NULL, 0);
        else if (strcmp(key, "CPU part") == 0) part = (unsigned)strtoul(value, NULL, 0);
    }
    if (!cpu[0]) {
        const char *core = arm_core(implementer, part);
        snprintf(cpu, n, "%s", core ? core : "Unknown processor");
    }
}

uint64_t lp_sysinfo_parse_memtotal(const char *text) {
    const char *at = text ? strstr(text, "MemTotal:") : NULL;
    return at ? strtoull(at + 9, NULL, 10) * 1024 : 0;
}

int lp_sysinfo_value(const char *text, const char *key, char *out, size_t n) {
    size_t klen = strlen(key);
    const char *cursor = text, *line;
    size_t len;
    while ((line = next_line(&cursor, &len))) {
        if (len <= klen || strncmp(line, key, klen) != 0 || line[klen] != '=') continue;
        const char *value = line + klen + 1;
        size_t vlen = len - klen - 1;
        if (vlen >= 2 && (value[0] == '"' || value[0] == '\'') && value[vlen - 1] == value[0]) { value++; vlen -= 2; }
        copy_trimmed(value, vlen, out, n);
        return 1;
    }
    if (n) out[0] = 0;
    return 0;
}

void lp_sysinfo_about(lp_about *out) {
    memset(out, 0, sizeof *out);
    lp_branding b = lp_branding_load();
    snprintf(out->os, sizeof out->os, "%s", b.pretty_name);
    struct utsname u;
    if (uname(&u) == 0) snprintf(out->kernel, sizeof out->kernel, "%s %s", u.sysname, u.release);
    char *text = NULL;
    size_t len = 0;
    if (lp_files_read("/proc/cpuinfo", &text, &len) == 0) { lp_sysinfo_parse_cpuinfo(text, out->cpu, sizeof out->cpu, &out->cores); free(text); }
    if (lp_files_read("/proc/meminfo", &text, &len) == 0) { out->memory = lp_sysinfo_parse_memtotal(text); free(text); }
    if (lp_files_read("/proc/device-tree/model", &text, &len) == 0) { snprintf(out->model, sizeof out->model, "%s", text); free(text); }
    if (gethostname(out->hostname, sizeof out->hostname) != 0) out->hostname[0] = 0;
    out->hostname[sizeof out->hostname - 1] = 0;
}

int lp_sysinfo_parse_links(const char *text, lp_net_link *out, int max) {
    int count = 0;
    const char *cursor = text, *line;
    size_t len;
    while ((line = next_line(&cursor, &len)) && count < max) {
        char buf[512];
        copy_trimmed(line, len, buf, sizeof buf);
        char name[64], state[32];
        int used = 0;
        if (sscanf(buf, "%63s %31s %n", name, state, &used) < 2) continue;
        if (strcmp(name, "lo") == 0) continue;
        char *at = strchr(name, '@');   /* "veth0@if3" */
        if (at) *at = 0;
        lp_net_link *l = &out[count++];
        memset(l, 0, sizeof *l);
        snprintf(l->name, sizeof l->name, "%s", name);
        snprintf(l->state, sizeof l->state, "%s", state);
        snprintf(l->addresses, sizeof l->addresses, "%s", used > 0 && used <= (int)strlen(buf) ? buf + used : "");
        l->wireless = strncmp(name, "wl", 2) == 0;
    }
    return count;
}

int lp_sysinfo_parse_wifi(const char *text, lp_wifi_network *out, int max) {
    int count = 0;
    const char *cursor = text, *line;
    size_t len;
    while ((line = next_line(&cursor, &len)) && count < max) {
        /* take the colours out, counting only the bright stars: iwd dims the bars a network lacks */
        char plain[512];
        size_t o = 0;
        int dim = 0, stars = 0;
        for (size_t i = 0; i < len && o + 1 < sizeof plain; i++) {
            if (line[i] == '\x1b' && i + 1 < len && line[i + 1] == '[') {
                size_t j = i + 2;
                while (j < len && !isalpha((unsigned char)line[j])) j++;
                dim = j - i >= 4 && memcmp(line + i + 2, "1;90", 4) == 0 ? 1 : strncmp(line + i + 2, "0m", 2) == 0 ? 0 : dim;
                i = j;
                continue;
            }
            if (line[i] == '*' && !dim) stars++;
            plain[o++] = line[i];
        }
        plain[o] = 0;
        if (strstr(plain, "Available networks") || strstr(plain, "Network name") || strstr(plain, "----") || !strchr(plain, '*')) continue;
        char *p = plain;
        while (*p == ' ') p++;
        int connected = 0;
        if (*p == '>') { connected = 1; p++; }
        /* from the right: the bars, then the security; what is left is the name, spaces and all */
        char *end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) end--;
        while (end > p && end[-1] == '*') end--;
        while (end > p && isspace((unsigned char)end[-1])) end--;
        char *security = end;
        while (security > p && !isspace((unsigned char)security[-1])) security--;
        if (security == p) continue;
        lp_wifi_network *w = &out[count];
        memset(w, 0, sizeof *w);
        copy_trimmed(security, (size_t)(end - security), w->security, sizeof w->security);
        copy_trimmed(p, (size_t)(security - p), w->ssid, sizeof w->ssid);
        if (!w->ssid[0]) continue;
        w->signal = stars > 4 ? 4 : stars;
        w->connected = connected;
        count++;
    }
    return count;
}

int lp_sysinfo_parse_volume(const char *text, double *volume, int *muted) {
    const char *at = text ? strstr(text, "Volume:") : NULL;
    if (!at) return 0;
    char *end;
    double v = strtod(at + 7, &end);
    if (end == at + 7) return 0;
    *volume = v;
    *muted = strstr(end, "[MUTED]") != NULL;
    return 1;
}
