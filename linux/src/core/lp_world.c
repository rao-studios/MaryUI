/* The world the desktop publishes to Mary (PARITY D28): one place per app with a surface hook,
 * its front window, sent to maryd on change (coalesced), on a poll, and on request. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_world.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

int lp_app_surface_add(lp_app_surface *s, const char *role, const char *kind, const char *label, int focused, int enabled) {
    if (s->element_count >= LP_SURFACE_ELEMENTS) return -1;
    lp_app_surface_element *e = &s->elements[s->element_count];
    memset(e, 0, sizeof *e);
    snprintf(e->role, sizeof e->role, "%s", role ? role : "");
    snprintf(e->kind, sizeof e->kind, "%s", kind ? kind : role ? role : "");
    if (label) {
        size_t i = 0, points = 0;
        while (label[i] && points < LP_SURFACE_LABEL_MAX - 1 && i < sizeof e->label - 4) {
            i++;
            while ((label[i] & 0xC0) == 0x80) i++;
            points++;
        }
        memcpy(e->label, label, i);
    }
    e->focused = focused;
    e->enabled = enabled;
    if (focused) s->focused = s->element_count;
    return s->element_count++;
}

void lp_world_init(lp_world *w) {
    memset(w, 0, sizeof *w);
    w->poll_s = LP_WORLD_POLL_S;
}

void lp_world_free(lp_desktop *d) {
    lp_world *w = &d->world;
    if (w->coalesce) lp_desktop_remove_source(d, w->coalesce);
    if (w->poll) lp_desktop_remove_source(d, w->poll);
    w->coalesce = w->poll = NULL;
    free(w->selection_text);
    w->selection_text = NULL;
}

static int64_t wall_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* The front window of an app: the focused one when the app is focused, else its highest z. */
static const lp_window_record *front_window(const lp_desktop *d, const char *app_id, int *count, int *minimized) {
    const lp_window_record *front = NULL;
    *count = *minimized = 0;
    for (int i = 0; i < d->wm.count; i++) {
        const lp_window_record *w = &d->wm.windows[i];
        if (strcmp(w->app_id, app_id) != 0) continue;
        (*count)++;
        if (w->state == LP_WIN_SHADED) (*minimized)++;
        if (i == d->wm.focused) front = w;
        else if (!front || (front != &d->wm.windows[d->wm.focused] && w->z > front->z)) front = w;
    }
    return front;
}

#ifdef HAVE_JSONC

static struct json_object *str(const char *s) { return json_object_new_string(s ? s : ""); }

static void lower(const char *in, char *out, size_t n) {
    size_t i = 0;
    for (; in[i] && i + 1 < n; i++) out[i] = in[i] >= 'A' && in[i] <= 'Z' ? (char)(in[i] + 32) : in[i];
    out[i] = 0;
}

/* One app's surface, or NULL: the app's own account of its front window. */
static struct json_object *surface_json(lp_desktop *d, const lp_app *app, int64_t now_ms, lp_app_surface **filled) {
    int count, minimized;
    const lp_window_record *win = front_window(d, app->id, &count, &minimized);
    lp_app_instance *inst = win ? lp_desktop_instance(d, win->id) : NULL;
    if (!win || !inst || !app->surface) return NULL;
    lp_app_surface *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->focused = -1;
    if (!app->surface(inst->state, d, s)) { free(s); return NULL; }
    struct json_object *o = json_object_new_object(), *application = json_object_new_object(), *window = json_object_new_object(), *elements = json_object_new_array();
    json_object_object_add(application, "name", str(app->name ? app->name : app->title));
    json_object_object_add(application, "id", str(app->id));
    json_object_object_add(o, "application", application);
    json_object_object_add(window, "title", str(s->window_title[0] ? s->window_title : win->title));
    json_object_object_add(o, "activeWindow", window);
    json_object_object_add(o, "windowCount", json_object_new_int(count));
    json_object_object_add(o, "minimizedCount", json_object_new_int(minimized));
    for (int i = 0; i < s->element_count; i++) {
        const lp_app_surface_element *e = &s->elements[i];
        struct json_object *ej = json_object_new_object();
        char role[24], identity[LP_SURFACE_LABEL_MAX + 32];
        lower(e->role, role, sizeof role);
        snprintf(identity, sizeof identity, "%s|%s", role, e->label);
        json_object_object_add(ej, "identity", str(identity));
        json_object_object_add(ej, "ordinal", json_object_new_int(i));
        json_object_object_add(ej, "role", str(e->role));
        json_object_object_add(ej, "kind", str(e->kind));
        json_object_object_add(ej, "label", str(e->label));
        json_object_object_add(ej, "focused", json_object_new_boolean(e->focused));
        json_object_object_add(ej, "enabled", json_object_new_boolean(e->enabled));
        json_object_array_add(elements, ej);
    }
    json_object_object_add(o, "elements", elements);
    if (s->focused >= 0 && s->focused < s->element_count) json_object_object_add(o, "focused", json_object_get(json_object_array_get_idx(elements, (size_t)s->focused)));
    json_object_object_add(o, "pageNotYetRead", json_object_new_boolean(s->page_not_yet_read));
    if (s->document_name[0] || s->document_path[0] || s->document_text) {
        struct json_object *doc = json_object_new_object();
        json_object_object_add(doc, "name", str(s->document_name[0] ? s->document_name : lp_files_basename(s->document_path)));
        if (s->document_path[0]) json_object_object_add(doc, "path", str(s->document_path));
        if (s->document_text) {
            json_object_object_add(doc, "text", str(s->document_text));
            json_object_object_add(doc, "total", json_object_new_int(s->document_total));
            json_object_object_add(doc, "lower", json_object_new_int(s->document_lower));
            json_object_object_add(doc, "upper", json_object_new_int(s->document_upper));
        }
        json_object_object_add(o, "document", doc);
    }
    json_object_object_add(o, "capturedAt", json_object_new_int64(now_ms));
    if (filled) *filled = s;
    else { free(s->document_text); free(s->selection_text); free(s); }
    return o;
}

static void surface_free(lp_app_surface *s) {
    if (!s) return;
    free(s->document_text);
    free(s->selection_text);
    free(s);
}

/* The world, and the focused app's surface for the selection handoff. */
static struct json_object *build(lp_desktop *d, int64_t now_ms, lp_app_surface **focused_surface, const lp_app **focused_app) {
    struct json_object *world = json_object_new_object(), *places = json_object_new_array(), *windows = json_object_new_array();
    json_object_object_add(world, "type", str("world"));
    json_object_object_add(world, "capturedAt", json_object_new_int64(now_ms));
    const lp_window_record *focused = lp_wm_focused(&d->wm);
    if (focused_surface) *focused_surface = NULL;
    if (focused_app) *focused_app = NULL;
    for (int a = 0; a < d->app_count; a++) {
        const lp_app *app = d->apps[a];
        if (!app->surface) continue;
        int is_focused = focused && strcmp(focused->app_id, app->id) == 0;
        lp_app_surface *filled = NULL;
        struct json_object *surface = surface_json(d, app, now_ms, is_focused && focused_surface ? &filled : NULL);
        if (!surface) continue;
        struct json_object *place = json_object_new_object();
        char token[64];
        snprintf(token, sizeof token, "applications:%s", app->id);
        json_object_object_add(place, "place", str(token));
        json_object_object_add(place, "capturedAt", json_object_new_int64(now_ms));
        json_object_object_add(place, "surface", surface);
        json_object_array_add(places, place);
        if (filled) { *focused_surface = filled; if (focused_app) *focused_app = app; }
    }
    json_object_object_add(world, "places", places);
    if (focused) {
        char token[64];
        snprintf(token, sizeof token, "applications:%s", focused->app_id);
        json_object_object_add(world, "focus", str(token));
    }
    for (int i = 0; i < d->wm.count; i++) {
        const lp_window_record *w = &d->wm.windows[i];
        struct json_object *wj = json_object_new_object();
        json_object_object_add(wj, "id", str(w->id));
        json_object_object_add(wj, "app", str(w->app_id));
        json_object_object_add(wj, "title", str(w->title));
        json_object_object_add(wj, "minimized", json_object_new_boolean(w->state == LP_WIN_SHADED));
        json_object_array_add(windows, wj);
    }
    json_object_object_add(world, "windows", windows);
    return world;
}

#define PLAIN (JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE)

char *lp_desktop_world_json(lp_desktop *d, int64_t now_ms) {
    struct json_object *world = build(d, now_ms, NULL, NULL);
    char *text = strdup(json_object_to_json_string_ext(world, PLAIN));
    json_object_put(world);
    return text;
}

char *lp_desktop_app_surface_json(lp_desktop *d, const char *app_id, int64_t now_ms) {
    const lp_app *app = app_id ? lp_desktop_find_app(d, app_id) : NULL;
    struct json_object *surface = app ? surface_json(d, app, now_ms, NULL) : NULL;
    if (!surface) return NULL;
    char token[64];
    snprintf(token, sizeof token, "applications:%s", app->id);
    json_object_object_add(surface, "place", str(token));
    char *text = strdup(json_object_to_json_string_ext(surface, PLAIN));
    json_object_put(surface);
    return text;
}

static int send_object(lp_desktop *d, struct json_object *o) {
    int rc = lp_mary_send_line(&d->mary, json_object_to_json_string_ext(o, PLAIN));
    json_object_put(o);
    return rc;
}

/* The selection handoff: sent when the focused app's selection differs from the last one sent. */
static void publish_selection(lp_desktop *d, const lp_app *app, lp_app_surface *s, int64_t now_ms) {
    lp_world *w = &d->world;
    const char *text = s && s->selection_text && s->selection_text[0] ? s->selection_text : NULL;
    if (!text) {
        if (w->selection_app[0]) {
            struct json_object *o = json_object_new_object();
            json_object_object_add(o, "type", str("selection.clear"));
            json_object_object_add(o, "applicationID", str(w->selection_app));
            send_object(d, o);
            w->selection_app[0] = 0;
            free(w->selection_text);
            w->selection_text = NULL;
        }
        return;
    }
    if (strcmp(w->selection_app, app->id) == 0 && w->selection_text && strcmp(w->selection_text, text) == 0 &&
        w->selection_lower == s->selection_lower && w->selection_upper == s->selection_upper) return;
    struct json_object *o = json_object_new_object();
    char token[64];
    snprintf(token, sizeof token, "applications:%s", app->id);
    json_object_object_add(o, "type", str("selection"));
    json_object_object_add(o, "applicationID", str(app->id));
    json_object_object_add(o, "place", str(token));
    json_object_object_add(o, "text", str(text));
    if (s->document_name[0] || s->document_path[0]) json_object_object_add(o, "document", str(s->document_name[0] ? s->document_name : lp_files_basename(s->document_path)));
    if (s->selection_upper > s->selection_lower) {
        json_object_object_add(o, "lower", json_object_new_int(s->selection_lower));
        json_object_object_add(o, "upper", json_object_new_int(s->selection_upper));
    }
    if (s->document_total) json_object_object_add(o, "total", json_object_new_int(s->document_total));
    json_object_object_add(o, "editable", json_object_new_boolean(s->selection_editable));
    json_object_object_add(o, "capturedAt", json_object_new_int64(now_ms));
    if (send_object(d, o) != 0) return;
    snprintf(w->selection_app, sizeof w->selection_app, "%s", app->id);
    free(w->selection_text);
    w->selection_text = strdup(text);
    w->selection_lower = s->selection_lower;
    w->selection_upper = s->selection_upper;
}

static int on_poll(int fd, uint32_t mask, void *data);

/* The poll follows the fastest app that has a window open. */
static void arm_poll(lp_desktop *d) {
    lp_world *w = &d->world;
    int fastest = 0;
    for (int i = 0; i < d->wm.count; i++) {
        const lp_app *app = lp_desktop_find_app(d, d->wm.windows[i].app_id);
        if (!app || !app->surface) continue;
        int s = app->surface_poll_s > 0 ? app->surface_poll_s : LP_WORLD_POLL_S;
        if (!fastest || s < fastest) fastest = s;
    }
    if (!fastest) return;
    if (!w->poll) w->poll = lp_desktop_add_timer(d, 0, on_poll, d);
    if (w->poll) lp_desktop_update_timer(d, w->poll, fastest * 1000);
    w->poll_s = fastest;
}

int lp_desktop_publish_world(lp_desktop *d) {
    if (!lp_mary_connected(&d->mary)) return -ENOTCONN;
    int64_t now = wall_ms();
    lp_app_surface *focused_surface = NULL;
    const lp_app *focused_app = NULL;
    struct json_object *world = build(d, now, &focused_surface, &focused_app);
    int rc = send_object(d, world);
    if (rc == 0) {
        d->world.published++;
        publish_selection(d, focused_app, focused_surface, now);
    }
    surface_free(focused_surface);
    arm_poll(d);
    return rc;
}

static int on_poll(int fd, uint32_t mask, void *data) {
    lp_desktop_publish_world(data);
    return 0;
}

static int on_coalesce(int fd, uint32_t mask, void *data) {
    lp_desktop_publish_world(data);
    return 0;
}

void lp_desktop_world_changed(lp_desktop *d) {
    if (!lp_mary_connected(&d->mary)) return;
    lp_world *w = &d->world;
    if (!w->coalesce) w->coalesce = lp_desktop_add_timer(d, 0, on_coalesce, d);
    if (w->coalesce) lp_desktop_update_timer(d, w->coalesce, LP_WORLD_COALESCE_MS);
    else lp_desktop_publish_world(d);
}

void lp_desktop_on_world_request(lp_desktop *d) { lp_desktop_publish_world(d); }

void lp_desktop_on_app_state(lp_desktop *d, const char *call_id, const char *app_id) {
    if (!call_id) return;
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "type", str("app.state.result"));
    json_object_object_add(o, "call_id", str(call_id));
    const lp_app *app = app_id ? lp_desktop_find_app(d, app_id) : NULL;
    struct json_object *surface = app ? surface_json(d, app, wall_ms(), NULL) : NULL;
    if (surface) {
        char token[64];
        snprintf(token, sizeof token, "applications:%s", app->id);
        json_object_object_add(surface, "place", str(token));
        json_object_object_add(o, "ok", json_object_new_boolean(1));
        json_object_object_add(o, "surface", surface);
    } else {
        json_object_object_add(o, "ok", json_object_new_boolean(0));
        json_object_object_add(o, "error", str(app ? "no window" : "unknown"));
    }
    send_object(d, o);
}

#else

char *lp_desktop_world_json(lp_desktop *d, int64_t now_ms) { return NULL; }
char *lp_desktop_app_surface_json(lp_desktop *d, const char *app_id, int64_t now_ms) { return NULL; }
int lp_desktop_publish_world(lp_desktop *d) { return -ENOSYS; }
void lp_desktop_world_changed(lp_desktop *d) {}
void lp_desktop_on_world_request(lp_desktop *d) {}
void lp_desktop_on_app_state(lp_desktop *d, const char *call_id, const char *app_id) {}

#endif
