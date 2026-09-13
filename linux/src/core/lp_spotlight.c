#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_spotlight.h"

void lp_spotlight_init(lp_spotlight *s) { memset(s, 0, sizeof *s); }
void lp_spotlight_open(lp_spotlight *s) { s->open = 1; lp_text_buffer_set(&s->query, ""); s->selection = 0; }
void lp_spotlight_close(lp_spotlight *s) { s->open = 0; }
void lp_spotlight_toggle(lp_spotlight *s) { if (s->open) lp_spotlight_close(s); else lp_spotlight_open(s); }
void lp_spotlight_set_query(lp_spotlight *s, const char *query) { lp_text_buffer_set(&s->query, query); s->selection = 0; }

void lp_spotlight_move(lp_spotlight *s, int delta, int count) {
    if (count <= 0) { s->selection = 0; return; }
    s->selection = ((s->selection + delta) % count + count) % count;
}

static void lower(const char *in, char *out, size_t n) {
    size_t i;
    for (i = 0; in[i] && i + 1 < n; i++) out[i] = (char)tolower((unsigned char)in[i]);
    out[i] = 0;
}

static void trim(const char *in, char *out, size_t n) {
    while (*in && isspace((unsigned char)*in)) in++;
    size_t len = strlen(in);
    while (len > 0 && isspace((unsigned char)in[len - 1])) len--;
    if (len >= n) len = n - 1;
    memcpy(out, in, len);
    out[len] = 0;
}

int lp_spotlight_items(const lp_desktop *d, lp_spotlight_item *out, int max) {
    int n = 0;
    for (int i = 0; i < d->app_count && n < max; i++) {
        const lp_app *app = d->apps[i];
        if (app->internal) continue; /* opened by other apps only; `index` stays the real app index */
        lp_spotlight_item *it = &out[n++];
        memset(it, 0, sizeof *it);
        it->kind = LP_SPOT_APP;
        snprintf(it->id, sizeof it->id, "%s", app->id);
        snprintf(it->title, sizeof it->title, "%s", app->name ? app->name : app->title);
        snprintf(it->subtitle, sizeof it->subtitle, "Application");
        it->icon = app->icon < LP_ICON_COUNT ? app->icon : LP_ICON_DOCUMENT;
        it->object = app->object;
        it->dock = lp_desktop_in_dock(d, app);
        it->index = i;
        for (int w = 0; w < d->wm.count; w++) if (strcmp(d->wm.windows[w].app_id, app->id) == 0) it->running = 1;
    }
    /* foot, until a native terminal app is registered (PARITY D9) */
    if (n < max && !lp_desktop_find_app(d, "terminal")) {
        lp_spotlight_item *it = &out[n++];
        memset(it, 0, sizeof *it);
        it->kind = LP_SPOT_COMMAND;
        snprintf(it->id, sizeof it->id, "terminal");
        snprintf(it->title, sizeof it->title, "Terminal");
        snprintf(it->subtitle, sizeof it->subtitle, "Command");
        it->icon = LP_ICON_TERMINAL;
        it->dock = 1;
        for (int w = 0; w < d->wm.count; w++) if (!lp_desktop_find_app(d, d->wm.windows[w].app_id)) it->running = 1;
    }
    for (int w = 0; w < d->wm.count && n < max; w++) {
        const lp_window_record *rec = &d->wm.windows[w];
        const lp_app *app = lp_desktop_find_app(d, rec->app_id);
        lp_spotlight_item *it = &out[n++];
        memset(it, 0, sizeof *it);
        it->kind = LP_SPOT_WINDOW;
        snprintf(it->id, sizeof it->id, "%s", rec->id);
        snprintf(it->title, sizeof it->title, "%s", rec->title);
        snprintf(it->subtitle, sizeof it->subtitle, "Window · %s", app ? (app->name ? app->name : app->title) : rec->app_id);
        it->icon = app ? (app->icon < LP_ICON_COUNT ? app->icon : LP_ICON_DOCUMENT) : LP_ICON_TERMINAL;
        it->index = w;
    }
    return n;
}

/* 0: the title starts with q · 1: a word of the title starts with q · 2: q appears inside · -1: no match */
static int rank(const char *title, const char *q) {
    char t[128];
    lower(title, t, sizeof t);
    size_t qn = strlen(q);
    if (strncmp(t, q, qn) == 0) return 0;
    for (const char *p = t; *p; p++) {
        if (p > t && isspace((unsigned char)p[-1]) && strncmp(p, q, qn) == 0) return 1;
    }
    return strstr(t, q) ? 2 : -1;
}

int lp_spotlight_results(const lp_spotlight_item *items, int n, const char *query, lp_spotlight_item *out, int max) {
    char trimmed[256], q[256];
    trim(query ? query : "", trimmed, sizeof trimmed);
    lower(trimmed, q, sizeof q);
    int count = 0;
    if (!q[0]) {
        for (int i = 0; i < n && count < max; i++) if (items[i].kind != LP_SPOT_WINDOW && items[i].dock) out[count++] = items[i];
        return count;
    }
    for (int r = 0; r <= 2; r++) {
        for (int i = 0; i < n && count < max; i++) if (rank(items[i].title, q) == r) out[count++] = items[i];
    }
    return count;
}
