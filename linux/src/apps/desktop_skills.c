/* The desktop's own skills (PARITY D30): the window verbs, as the Mac's window-management
 * discipline. An app without a window — nothing to paint, nothing to open — that maryd
 * publishes as `desktop`: list the windows, close or shade the front one, bring an app forward. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_skill.h"

static const char *const WIN_LIST_TOKENS[] = { "windows", "open" };
static const char *const WIN_LIST_PHRASES[] = { "what is open", "which windows are open", "list the windows" };
static const char *const WIN_CLOSE_TOKENS[] = { "close" };
static const char *const WIN_CLOSE_PHRASES[] = { "close this window", "close the window", "close it" };
static const char *const WIN_SHADE_TOKENS[] = { "shade", "roll", "collapse" };
static const char *const WIN_SHADE_PHRASES[] = { "shade this window", "roll up the window", "collapse the window" };
static const char *const WIN_FRONT_TOKENS[] = { "switch", "bring", "front", "focus" };
static const char *const WIN_FRONT_PHRASES[] = { "switch to", "bring forward", "go to", "show me" };
static const char *const WIN_CLASSES[] = { "window", "app" };

static const lp_skill desktop_skills[] = {
    { .id = "list_windows", .title = "List the windows", .summary = "Names every open window, its app and which is in front.",
      .effect = LP_SKILL_READ, .kind = "cognitive", .access = "seamless", .triggers = WIN_LIST_TOKENS, .trigger_count = 2,
      .phrases = WIN_LIST_PHRASES, .phrase_count = 3, .target_classes = WIN_CLASSES, .target_class_count = 2 },
    { .id = "close_front_window", .title = "Close the front window", .summary = "Closes the window in front, as its close button does; what it held is gone.",
      .effect = LP_SKILL_DESTRUCTIVE, .kind = "effectful", .access = "confirm", .triggers = WIN_CLOSE_TOKENS, .trigger_count = 1,
      .phrases = WIN_CLOSE_PHRASES, .phrase_count = 3, .target_classes = WIN_CLASSES, .target_class_count = 2 },
    { .id = "shade_front_window", .title = "Shade the front window", .summary = "Rolls the front window up to its title bar, or down again.",
      .effect = LP_SKILL_ACT, .kind = "effectful", .access = "reversible", .triggers = WIN_SHADE_TOKENS, .trigger_count = 3,
      .phrases = WIN_SHADE_PHRASES, .phrase_count = 3, .target_classes = WIN_CLASSES, .target_class_count = 2 },
    { .id = "bring_forward", .title = "Bring an app forward", .summary = "Opens an app, or brings its window to the front.",
      .params = "{\"type\":\"object\",\"properties\":{\"app\":{\"type\":\"string\"}},\"required\":[\"app\"]}",
      .effect = LP_SKILL_ACT, .kind = "effectful", .access = "reversible", .triggers = WIN_FRONT_TOKENS, .trigger_count = 4,
      .phrases = WIN_FRONT_PHRASES, .phrase_count = 4, .target_classes = WIN_CLASSES, .target_class_count = 2 },
};

static const lp_app *app_named(const lp_desktop *d, const char *name) {
    for (int i = 0; i < d->app_count; i++) {
        const lp_app *a = d->apps[i];
        if (a->internal) continue;
        if (strcasecmp(a->id, name) == 0 || (a->name && strcasecmp(a->name, name) == 0) || (a->title && strcasecmp(a->title, name) == 0) ||
            (a->aka && strcasecmp(a->aka, name) == 0)) return a;
        for (int k = 0; k < a->alias_count; k++) if (strcasecmp(a->aliases[k], name) == 0) return a;
    }
    return NULL;
}

static int desktop_perform(void *state, lp_desktop *d, const char *skill, const char *args, char *result, size_t n) {
    (void)state;
    if (!d) return -EIO;
    const lp_window_record *front = lp_wm_focused(&d->wm);
    if (strcmp(skill, "list_windows") == 0) {
        size_t used = (size_t)snprintf(result, n, "{\"count\":%d,\"windows\":[", d->wm.count);
        for (int i = 0; i < d->wm.count && used < n; i++) {
            const lp_window_record *w = &d->wm.windows[i];
            char title[300];
            lp_skill_json_escape(w->title, title, sizeof title);
            used += (size_t)snprintf(result + used, n - used, "%s{\"id\":\"%s\",\"app\":\"%s\",\"title\":\"%s\",\"front\":%s,\"shaded\":%s}",
                                     i ? "," : "", w->id, w->app_id, title, front == w ? "true" : "false", w->state == LP_WIN_SHADED ? "true" : "false");
        }
        if (used < n) snprintf(result + used, n - used, "]}");
        return used + 2 < n ? 0 : -ENOBUFS;
    }
    if (strcmp(skill, "close_front_window") == 0 || strcmp(skill, "shade_front_window") == 0) {
        if (!front) { snprintf(result, n, "No window is open."); return -ENOENT; }
        char id[12], title[300];
        snprintf(id, sizeof id, "%s", front->id);
        lp_skill_json_escape(front->title, title, sizeof title);
        if (strcmp(skill, "close_front_window") == 0) {
            lp_desktop_close_window(d, id);
            snprintf(result, n, "{\"landed\":true,\"window\":\"%s\",\"summary\":\"Closed %s.\"}", id, title);
        } else {
            lp_wm_action a = { .type = LP_WM_TOGGLE_SHADE, .id = id };
            lp_desktop_dispatch(d, &a);
            int w = lp_wm_find(&d->wm, id);
            int shaded = w >= 0 && d->wm.windows[w].state == LP_WIN_SHADED;
            snprintf(result, n, "{\"landed\":true,\"window\":\"%s\",\"shaded\":%s,\"summary\":\"%s %s.\"}", id, shaded ? "true" : "false", shaded ? "Shaded" : "Unshaded", title);
        }
        return 0;
    }
    if (strcmp(skill, "bring_forward") == 0) {
        char name[80];
        if (!lp_skill_arg_string(args, "app", name, sizeof name) || !name[0]) { snprintf(result, n, "Which app?"); return -EINVAL; }
        const lp_app *app = app_named(d, name);
        if (!app) { snprintf(result, n, "There is no app called %.60s.", name); return -ENOENT; }
        lp_desktop_open_app(d, app->id);
        snprintf(result, n, "{\"landed\":true,\"app\":\"%s\",\"summary\":\"%s is in front.\"}", app->id, app->name ? app->name : app->title);
        return 0;
    }
    return -ENOENT;
}

const lp_app lp_app_desktop = {
    .id = "desktop", .title = "Desktop", .name = "Desktop", .icon = LP_ICON_DESKTOP, .hidden = 1, .internal = 1,
    .skills = desktop_skills, .skill_count = 4, .perform = desktop_perform,
    .summary = "The windows on screen: what is open, what is in front.", .discipline = "window-management", .paradigm = "systemControl",
};
