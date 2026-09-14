/* Reopening windows at login (lp_session.h). One line a window: app x y width height state [document]. */
#include "maryui/lp_session.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "maryui/lp_app.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_world.h"

int lp_session_document_path(lp_desktop *d, const char *window_id, char *out, size_t n) {
    if (n) out[0] = 0;
    lp_app_instance *inst = lp_desktop_instance(d, window_id);
    if (!inst || !inst->app || !inst->app->surface) return 0;
    lp_app_surface *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->focused = -1;
    if (inst->app->surface(inst->state, d, s) && n) snprintf(out, n, "%s", s->document_path);
    free(s->document_text);
    free(s->selection_text);
    free(s);
    return n && out[0] != 0;
}

static const char *state_name(enum lp_window_state s) {
    return s == LP_WIN_ZOOMED ? "zoomed" : s == LP_WIN_SHADED ? "shaded" : "normal";
}

int lp_session_save(lp_desktop *d) {
    char path[1100], tmp[1200];
    lp_config_path(LP_SESSION_FILE, path, sizeof path, 1);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -errno;
    fprintf(f, "# The windows open when the desktop last changed; System Settings > General > Reopen at login opens them again.\n"
               "# app x y width height normal|shaded|zoomed [folder or document]\n");
    int order[LP_WM_MAX_WINDOWS], n = 0;
    for (int i = 0; i < d->wm.count; i++) {
        lp_app_instance *inst = lp_desktop_instance(d, d->wm.windows[i].id);
        if (!inst || !inst->app || inst->app->internal) continue;
        int k = n++;
        while (k > 0 && d->wm.windows[order[k - 1]].z > d->wm.windows[i].z) { order[k] = order[k - 1]; k--; }
        order[k] = i;
    }
    char *doc = malloc(LP_FILES_PATH_MAX);
    for (int k = 0; k < n; k++) {
        const lp_window_record *rec = &d->wm.windows[order[k]];
        lp_rect r = rec->state == LP_WIN_ZOOMED && rec->has_prev ? rec->prev_rect : rec->rect;   /* the size it returns to */
        if (!doc || !lp_session_document_path(d, rec->id, doc, LP_FILES_PATH_MAX) || strchr(doc, '\n')) { if (doc) doc[0] = 0; }
        fprintf(f, "%s %.0f %.0f %.0f %.0f %s%s%s\n", rec->app_id, r.x, r.y, r.w, r.h, state_name(rec->state),
                doc && doc[0] ? " " : "", doc ? doc : "");
    }
    free(doc);
    int failed = fflush(f) != 0 || fsync(fileno(f)) != 0;
    if (fclose(f) != 0) failed = 1;
    if (failed || rename(tmp, path) != 0) {
        int e = errno ? errno : EIO;
        unlink(tmp);
        return -e;
    }
    return n;
}

int lp_session_restore(lp_desktop *d) {
    char path[1100];
    lp_config_path(LP_SESSION_FILE, path, sizeof path, 0);
    FILE *f = fopen(path, "r");
    if (!f) return errno == ENOENT ? 0 : -errno;
    size_t cap = LP_FILES_PATH_MAX + 256;
    char *line = malloc(cap);
    int restored = 0;
    while (line && fgets(line, (int)cap, f)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = 0;
        char app_id[32], state[16];
        float x, y, w, h;
        int used = 0;
        if (sscanf(line, "%31s %f %f %f %f %15s%n", app_id, &x, &y, &w, &h, state, &used) != 6) continue;
        if (!isfinite(x) || !isfinite(y) || !isfinite(w) || !isfinite(h) || w <= 0 || h <= 0) continue;
        const char *doc = line + used;
        while (*doc == ' ') doc++;
        const lp_app *app = lp_desktop_find_app(d, app_id);
        if (!app || app->internal) continue;
        int is_dir = 0;
        const char *open_path = *doc && lp_files_exists(doc, &is_dir) ? doc : NULL;   /* gone: the app opens empty */
        char id[12];
        if (!lp_desktop_open_app_with(d, app_id, open_path, NULL, id)) continue;
        /* the size first (a resize takes the rect as given), then the place (a move keeps the title bar reachable) */
        lp_wm_action resize = { .type = LP_WM_RESIZE, .id = id, .rect = LP_RECT(x, y, w, h) };
        lp_desktop_dispatch(d, &resize);
        lp_wm_action move = { .type = LP_WM_MOVE, .id = id, .x = x, .y = y };
        lp_desktop_dispatch(d, &move);
        if (strcmp(state, "zoomed") == 0) {
            lp_wm_action a = { .type = LP_WM_TOGGLE_ZOOM, .id = id };
            lp_desktop_dispatch(d, &a);
        } else if (strcmp(state, "shaded") == 0) {
            lp_wm_action a = { .type = LP_WM_TOGGLE_SHADE, .id = id };
            lp_desktop_dispatch(d, &a);
        }
        restored++;
    }
    free(line);
    fclose(f);
    return restored;
}
