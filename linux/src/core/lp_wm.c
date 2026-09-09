#include <math.h>
#include <stdio.h>
#include <string.h>

#include "maryui/lp_geometry.h"
#include "maryui/lp_wm.h"

lp_open_spec lp_open_spec_default(const char *app_id, const char *title) {
    return (lp_open_spec){ .app_id = app_id, .title = title, .rect = { NAN, NAN, NAN, NAN }, .min_size = { 0, 0 }, .resizable = 1, .singleton = 0 };
}

void lp_wm_init(lp_wm_state *s, lp_rect bounds) {
    memset(s, 0, sizeof *s);
    s->focused = -1;
    s->next_z = 1;
    s->next_id = 1;
    s->bounds = bounds;
}

int lp_wm_find(const lp_wm_state *s, const char *id) {
    if (!id) return -1;
    for (int i = 0; i < s->count; i++) if (strcmp(s->windows[i].id, id) == 0) return i;
    return -1;
}

int lp_wm_top_most(const lp_wm_state *s, int except) {
    int best = -1;
    for (int i = 0; i < s->count; i++) {
        if (i == except) continue;
        if (best < 0 || s->windows[i].z > s->windows[best].z) best = i;
    }
    return best;
}

const lp_window_record *lp_wm_focused(const lp_wm_state *s) {
    return s->focused >= 0 && s->focused < s->count ? &s->windows[s->focused] : NULL;
}

static uint64_t bit(int i) { return 1ull << i; }

/* raise(): a strictly higher z for one window; everything else untouched. */
static uint64_t raise_window(lp_wm_state *s, int i) {
    if (i < 0 || i >= s->count) return 0;
    if (s->focused == i) return 0;
    s->windows[i].z = s->next_z++;
    s->focused = i;
    return bit(i) | LP_WM_CHANGED_FOCUS | LP_WM_CHANGED_ORDER;
}

/* Staggers new windows so they never open exactly on top of each other. */
static lp_rect default_rect(const lp_wm_state *s, int index) {
    float w = fminf(640, fmaxf(LP_DEFAULT_MIN_SIZE.w, s->bounds.w - 80));
    float h = fminf(440, fmaxf(LP_DEFAULT_MIN_SIZE.h, s->bounds.h - 80));
    float offset = (float)((index % 8) * 28);
    return LP_RECT(s->bounds.x + 60 + offset, s->bounds.y + 40 + offset, w, h);
}

static uint64_t reduce_open(lp_wm_state *s, const lp_open_spec *spec) {
    if (spec->singleton) {
        for (int i = 0; i < s->count; i++) {
            if (strcmp(s->windows[i].app_id, spec->app_id) == 0) {
                uint64_t changed = 0;
                if (s->windows[i].state == LP_WIN_SHADED) { s->windows[i].state = LP_WIN_NORMAL; changed |= bit(i); }
                return changed | raise_window(s, i);
            }
        }
    }
    if (s->count >= LP_WM_MAX_WINDOWS) return 0;
    int i = s->count;
    lp_window_record *w = &s->windows[i];
    memset(w, 0, sizeof *w);
    snprintf(w->id, sizeof w->id, "w%d", s->next_id);
    snprintf(w->app_id, sizeof w->app_id, "%s", spec->app_id ? spec->app_id : "");
    snprintf(w->title, sizeof w->title, "%s", spec->title ? spec->title : "");
    lp_rect base = default_rect(s, i);
    if (!isnan(spec->rect.x)) base.x = spec->rect.x;
    if (!isnan(spec->rect.y)) base.y = spec->rect.y;
    if (!isnan(spec->rect.w)) base.w = spec->rect.w;
    if (!isnan(spec->rect.h)) base.h = spec->rect.h;
    w->min_size = spec->min_size.w > 0 && spec->min_size.h > 0 ? spec->min_size : LP_DEFAULT_MIN_SIZE;
    w->rect = lp_constrain_rect(base, s->bounds, w->min_size);
    w->has_prev = 0;
    w->state = LP_WIN_NORMAL;
    w->z = s->next_z++;
    w->resizable = spec->resizable;
    s->count++;
    s->focused = i;
    s->next_id++;
    return bit(i) | LP_WM_CHANGED_OPENED | LP_WM_CHANGED_FOCUS | LP_WM_CHANGED_ORDER;
}

static uint64_t reduce_close(lp_wm_state *s, int i) {
    if (i < 0) return 0;
    int was_focused = s->focused == i;
    memmove(&s->windows[i], &s->windows[i + 1], sizeof(lp_window_record) * (size_t)(s->count - i - 1));
    s->count--;
    if (s->focused > i) s->focused--;
    if (was_focused) s->focused = lp_wm_top_most(s, -1);
    return LP_WM_CHANGED_CLOSED | LP_WM_CHANGED_ORDER | (was_focused ? LP_WM_CHANGED_FOCUS : 0);
}

uint64_t lp_wm_reduce(lp_wm_state *s, const lp_wm_action *a) {
    switch (a->type) {
    case LP_WM_OPEN:
        return reduce_open(s, &a->spec);
    case LP_WM_CLOSE:
        return reduce_close(s, lp_wm_find(s, a->id));
    case LP_WM_FOCUS:
        return raise_window(s, lp_wm_find(s, a->id));
    case LP_WM_FOCUS_NEXT: {
        if (s->count < 2) return 0;
        int lowest = 0;
        for (int i = 1; i < s->count; i++) if (s->windows[i].z < s->windows[lowest].z) lowest = i;
        return raise_window(s, lowest);
    }
    case LP_WM_MOVE: {
        int i = lp_wm_find(s, a->id);
        if (i < 0 || s->windows[i].state == LP_WIN_ZOOMED) return 0;
        lp_rect r = s->windows[i].rect;
        r.x = a->x;
        r.y = a->y;
        r = lp_clamp_to_bounds(r, s->bounds, 0, LP_TITLE_HEIGHT);
        if (lp_rects_equal(r, s->windows[i].rect)) return 0;
        s->windows[i].rect = r;
        return bit(i);
    }
    case LP_WM_RESIZE: {
        int i = lp_wm_find(s, a->id);
        if (i < 0 || !s->windows[i].resizable || s->windows[i].state != LP_WIN_NORMAL) return 0;
        lp_rect r = a->rect;
        r.w = fmaxf(r.w, s->windows[i].min_size.w);
        r.h = fmaxf(r.h, s->windows[i].min_size.h);
        if (lp_rects_equal(r, s->windows[i].rect)) return 0;
        s->windows[i].rect = r;
        return bit(i);
    }
    case LP_WM_TOGGLE_SHADE: {
        int i = lp_wm_find(s, a->id);
        if (i < 0) return 0;
        lp_window_record *w = &s->windows[i];
        if (w->state == LP_WIN_SHADED) {
            w->state = LP_WIN_NORMAL;
            return bit(i) | raise_window(s, i);
        }
        if (w->state == LP_WIN_ZOOMED) {
            w->state = LP_WIN_SHADED;
            if (w->has_prev) w->rect = w->prev_rect;
            w->has_prev = 0;
            return bit(i);
        }
        w->state = LP_WIN_SHADED;
        return bit(i);
    }
    case LP_WM_TOGGLE_ZOOM: {
        int i = lp_wm_find(s, a->id);
        if (i < 0) return 0;
        lp_window_record *w = &s->windows[i];
        if (w->state == LP_WIN_ZOOMED) {
            w->state = LP_WIN_NORMAL;
            if (w->has_prev) w->rect = lp_constrain_rect(w->prev_rect, s->bounds, w->min_size);
            w->has_prev = 0;
            return bit(i) | raise_window(s, i);
        }
        w->prev_rect = w->rect;
        w->has_prev = 1;
        w->state = LP_WIN_ZOOMED;
        w->rect = lp_fit_rect(s->bounds);
        return bit(i) | raise_window(s, i);
    }
    case LP_WM_SET_TITLE: {
        int i = lp_wm_find(s, a->id);
        if (i < 0 || !a->title) return 0;
        if (strcmp(s->windows[i].title, a->title) == 0) return 0;
        snprintf(s->windows[i].title, sizeof s->windows[i].title, "%s", a->title);
        return bit(i);
    }
    case LP_WM_SET_BOUNDS: {
        if (lp_rects_equal(a->bounds, s->bounds)) return 0;
        uint64_t changed = LP_WM_CHANGED_BOUNDS;
        s->bounds = a->bounds;
        for (int i = 0; i < s->count; i++) {
            lp_window_record *w = &s->windows[i];
            lp_rect r = w->state == LP_WIN_ZOOMED ? lp_fit_rect(a->bounds)
                : lp_clamp_to_bounds(lp_constrain_rect(w->rect, a->bounds, w->min_size), a->bounds, 0, LP_TITLE_HEIGHT);
            if (!lp_rects_equal(r, w->rect)) { w->rect = r; changed |= bit(i); }
        }
        return changed;
    }
    }
    return 0;
}
