/* Activity Monitor — the processes on the machine, refreshed every two
 * seconds: a CPU view and a Memory view of one sortable, searchable list, the
 * machine's totals beneath it, and Quit Process… behind a sheet that also
 * offers Force Quit. The numbers are lp_proc (src/core/lp_proc.c) reading
 * /proc; a shaded window stops reading them. Linux only (PARITY D15). */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_proc.h"
#include "maryui/lp_sheet.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define REFRESH_MS 2000
#define SUMMARY_H 52

struct activity {
    char window_id[12];
    lp_desktop *desk;
    char root[512];
    lp_proc_sample sample;
    int err;                        /* lp_proc_read's, when the last read failed */
    lp_source *timer;
    int view;                       /* 0 CPU, 1 Memory */
    enum lp_proc_sort sort;
    int descending;
    int *visible;                   /* indices into sample.procs matching the search, in list order */
    int nvisible, visible_cap;
    int selected_pid;
    lp_text_buffer query;
    lp_scroll_state scroll;
    int sheet_open, sheet_pid;
    char sheet_name[64];
    char status[200];
    int (*signal)(int pid, int force);
};

static void rebuild_visible(struct activity *a) {
    lp_proc_sort(&a->sample, a->sort, a->descending);
    if (a->visible_cap < a->sample.count) {
        int *grown = realloc(a->visible, sizeof *grown * (size_t)a->sample.count);
        if (!grown) { a->nvisible = 0; return; }
        a->visible = grown;
        a->visible_cap = a->sample.count;
    }
    a->nvisible = 0;
    for (int i = 0; i < a->sample.count; i++) {
        if (lp_proc_matches(&a->sample.procs[i], a->query.text)) a->visible[a->nvisible++] = i;
    }
}

static void refresh(struct activity *a) {
    lp_proc_sample next;
    int rc = lp_proc_read(a->root, a->sample.total_ticks ? &a->sample : NULL, &next);
    if (rc) {
        a->err = rc;
        lp_proc_free(&next);
        return;
    }
    lp_proc_free(&a->sample);
    a->sample = next;
    a->err = 0;
    if (a->selected_pid && !lp_proc_find(&a->sample, a->selected_pid)) a->selected_pid = 0;
    rebuild_visible(a);
}

static int on_tick(int fd, uint32_t mask, void *data) {
    struct activity *a = data;
    int w = lp_wm_find(&a->desk->wm, a->window_id);
    /* a shaded window shows no numbers, so it reads none */
    if (w >= 0 && a->desk->wm.windows[w].state != LP_WIN_SHADED) {
        refresh(a);
        if (a->desk->on_app_dirty) a->desk->on_app_dirty(a->desk, a->window_id);
    }
    lp_desktop_update_timer(a->desk, a->timer, REFRESH_MS);
    return 0;
}

static const lp_process *selected(const struct activity *a) {
    return a->selected_pid ? lp_proc_find(&a->sample, a->selected_pid) : NULL;
}

/* Not init; not a kernel thread (it has no command line, and ignores the signal anyway); and not the
 * desktop showing this window, nor the launcher that would restart it. */
static int can_quit(const struct activity *a) {
    const lp_process *p = selected(a);
    return p && p->pid > 1 && p->command[0] && p->pid != (int)getpid() && p->pid != (int)getppid();
}

static void ask_quit(struct activity *a) {
    const lp_process *p = selected(a);
    if (!p) return;
    if (!can_quit(a)) {
        snprintf(a->status, sizeof a->status, "“%s” keeps the system running and cannot be quit here.", p->name);
        return;
    }
    a->sheet_open = 1;
    a->sheet_pid = p->pid;
    snprintf(a->sheet_name, sizeof a->sheet_name, "%s", p->name);
}

static void quit_process(struct activity *a, int force) {
    int rc = a->signal(a->sheet_pid, force);
    if (rc) snprintf(a->status, sizeof a->status, "Could not quit “%s”: %s", a->sheet_name, strerror(-rc));
    else snprintf(a->status, sizeof a->status, force ? "Force quit “%s”." : "Asked “%s” to quit.", a->sheet_name);
    refresh(a);
}

static void sheet_spec(const struct activity *a, lp_sheet_spec *spec, char *title, size_t n) {
    snprintf(title, n, "Do you want to quit “%s”?", a->sheet_name);
    *spec = (lp_sheet_spec){
        .title = title,
        .message = "Quit asks it to finish and close. Force Quit ends it at once, and anything it has not saved is lost.",
        .icon = LP_ICON_WARNING,
        .buttons = { "Quit", "Cancel", "Force Quit" },
        .count = 3,
    };
}

static void set_view(struct activity *a, int view) {
    a->view = view;
    a->sort = view ? LP_PROC_SORT_MEMORY : LP_PROC_SORT_CPU;
    a->descending = 1;
    rebuild_visible(a);
}

/* A header click: the column's order, or the other way round when it already is. */
static void set_sort(struct activity *a, int column) {
    enum lp_proc_sort by;
    switch (column) {
    case 0: by = LP_PROC_SORT_NAME; break;
    case 1: by = a->view ? LP_PROC_SORT_MEMORY : LP_PROC_SORT_CPU; break;
    case 3: by = LP_PROC_SORT_PID; break;
    case 4: by = LP_PROC_SORT_USER; break;
    default: return;   /* threads: not an order worth having */
    }
    if (by == a->sort) a->descending = !a->descending;
    else { a->sort = by; a->descending = by == LP_PROC_SORT_CPU || by == LP_PROC_SORT_MEMORY; }
    rebuild_visible(a);
}

static void activity_command(void *state, lp_desktop *d, int cmd) {
    struct activity *a = state;
    if (!a) return;
    switch ((enum lp_activity_command)cmd) {
    case LP_ACTIVITY_QUIT_PROCESS: ask_quit(a); break;
    case LP_ACTIVITY_VIEW_CPU: set_view(a, 0); break;
    case LP_ACTIVITY_VIEW_MEMORY: set_view(a, 1); break;
    }
}

/* MARK: - Painting */

static void paint_toolbar(lp_ctx *ctx, struct activity *a, lp_rect bar, lp_id base) {
    float cy = bar.y + bar.h / 2;
    static const lp_segment VIEWS[2] = { { "CPU", LP_ICON_COUNT }, { "Memory", LP_ICON_COUNT } };
    lp_size seg = lp_segmented_measure(ctx, VIEWS, 2, LP_CONTROL_SM);
    int view = a->view;
    if (lp_segmented(ctx, lp_id_index(base, 1), bar.x, cy - seg.h / 2, VIEWS, 2, &view, LP_CONTROL_SM) && view != a->view) {
        set_view(a, view);
        ctx->dirty = 1;
    }
    lp_button_opts quit = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_STOP, 0, !can_quit(a) };
    lp_size qs = lp_button_measure(ctx, "Quit Process", quit);
    if (lp_button(ctx, lp_id_index(base, 2), LP_RECT(bar.x + seg.w + LP_SPACE_3, cy - qs.h / 2, qs.w, qs.h), "Quit Process", quit) && can_quit(a)) {
        ask_quit(a);
        ctx->dirty = 1;
    }
    lp_rect search = LP_RECT(bar.x + bar.w - 180, cy - LP_SIZE_CONTROL_HEIGHT / 2, 180, LP_SIZE_CONTROL_HEIGHT);
    if (lp_text_field(ctx, lp_id_index(base, 3), search, &a->query, (lp_text_field_opts){ .placeholder = "Search", .icon = LP_ICON_SEARCH, .round = 1 })) {
        rebuild_visible(a);
        a->scroll.y = 0;
        ctx->dirty = 1;
    }
}

static void paint_list(lp_ctx *ctx, struct activity *a, lp_rect area, lp_id base) {
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    if (draw) lp_fill_solid(cr, area, LP_SURFACE_BODY, 0);
    if (a->err) {
        if (draw) {
            char text[160];
            snprintf(text, sizeof text, "Activity Monitor cannot read the processes: %s", strerror(-a->err));
            lp_text_style st = lp_text_style_default();
            st.color = LP_INK_SECONDARY;
            lp_text_draw(cr, text, area, &st, LP_ALIGN_CENTER);
        }
        return;
    }
    static const char *const CPU_COLUMNS[5] = { "Process Name", "% CPU", "Threads", "PID", "User" };
    static const char *const MEMORY_COLUMNS[5] = { "Process Name", "Memory", "Threads", "PID", "User" };
    static const enum lp_align ALIGN[4] = { LP_ALIGN_END, LP_ALIGN_END, LP_ALIGN_END, LP_ALIGN_START };
    int sort_col = a->sort == LP_PROC_SORT_NAME ? 0 : a->sort == LP_PROC_SORT_PID ? 3 : a->sort == LP_PROC_SORT_USER ? 4 : 1;
    lp_rect header = lp_rect_cut_top(&area, LP_LIST_HEADER_H);
    int clicked = lp_list_header_aligned(ctx, lp_id_index(base, 10), header, a->view ? MEMORY_COLUMNS : CPU_COLUMNS, ALIGN, 5, sort_col, a->descending);
    if (clicked >= 0) { set_sort(a, clicked); ctx->dirty = 1; }
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 11), area, (lp_size){ area.w, (float)a->nvisible * LP_LIST_ROW_H }, &a->scroll);
    lp_id rows = lp_id_index(base, 12);
    for (int v = 0; v < a->nvisible; v++) {
        lp_rect r = LP_RECT(c.x, c.y + (float)v * LP_LIST_ROW_H, c.w, LP_LIST_ROW_H);
        if (r.y + r.h < area.y || r.y > area.y + area.h) continue;
        const lp_process *p = &a->sample.procs[a->visible[v]];
        char first[32], threads[16], pid[16];
        if (a->view) lp_proc_format_bytes(p->rss_bytes, first, sizeof first);
        else snprintf(first, sizeof first, "%.1f", p->cpu);
        snprintf(threads, sizeof threads, "%d", p->threads);
        snprintf(pid, sizeof pid, "%d", p->pid);
        const char *columns[4] = { first, threads, pid, p->user };
        if (lp_list_row_aligned(ctx, lp_id_index(rows, p->pid), r, LP_ICON_COUNT, p->name, columns, ALIGN, 4, p->pid == a->selected_pid, v % 2)) {
            a->selected_pid = p->pid;
            ctx->dirty = 1;
        }
    }
    lp_scroll_end(ctx);
}

static void paint_summary(lp_ctx *ctx, const struct activity *a, lp_rect r) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    lp_fill_vgradient(cr, r, LP_PLATINUM_2, LP_PLATINUM_3, 0);
    lp_draw_hairline(cr, r, LP_EDGE_TOP, LP_EDGE_DIVIDER);
    const lp_proc_sample *s = &a->sample;
    char line1[160], line2[160], a1[32], a2[32];
    float fraction;
    if (a->view == 0) {
        snprintf(line1, sizeof line1, "CPU %.0f%% busy across %d core%s", s->cpu_busy, s->cpus, s->cpus == 1 ? "" : "s");
        snprintf(line2, sizeof line2, "Load %.2f, %.2f, %.2f · %d processes", s->load1, s->load5, s->load15, s->count);
        fraction = (float)(s->cpu_busy / 100);
    } else {
        uint64_t in_use = s->mem_total > s->mem_available ? s->mem_total - s->mem_available : 0;
        lp_proc_format_bytes(in_use, a1, sizeof a1);
        lp_proc_format_bytes(s->mem_total, a2, sizeof a2);
        snprintf(line1, sizeof line1, "Memory %s used of %s", a1, a2);
        lp_proc_format_bytes(s->swap_total > s->swap_free ? s->swap_total - s->swap_free : 0, a1, sizeof a1);
        lp_proc_format_bytes(s->swap_total, a2, sizeof a2);
        snprintf(line2, sizeof line2, "Swap %s used of %s · %d processes", a1, a2, s->count);
        fraction = s->mem_total ? (float)((double)in_use / (double)s->mem_total) : 0;
    }
    lp_rect inner = lp_rect_inset(r, LP_SPACE_4, LP_SPACE_2);
    float left_w = inner.w * 0.55f, right_x = inner.x + inner.w * 0.6f, right_w = inner.w * 0.4f;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.color = LP_INK_SECONDARY;
    st.tabular_nums = 1;
    st.ellipsize = 1;
    lp_text_draw(cr, line1, LP_RECT(inner.x, inner.y, left_w, 18), &st, LP_ALIGN_START);
    lp_text_style sub = st;
    sub.size_px = LP_TEXT_XS;
    sub.color = LP_INK_TERTIARY;
    lp_text_draw(cr, line2, LP_RECT(inner.x, inner.y + 18, left_w, 16), &sub, LP_ALIGN_START);
    lp_progress(ctx, LP_RECT(right_x, inner.y + 5, right_w, LP_PROGRESS_H), fraction < 0 ? 0 : fraction > 1 ? 1 : fraction);
    if (a->status[0]) lp_text_draw(cr, a->status, LP_RECT(right_x, inner.y + 18, right_w, 16), &sub, LP_ALIGN_END);
}

static void activity_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct activity empty = { .err = -ENOENT };
    struct activity *a = state ? state : &empty;
    lp_id base = LP_ID("activity");
    char title[128];
    lp_sheet_spec spec;
    sheet_spec(a, &spec, title, sizeof title);
    int chosen = lp_sheet_input(ctx, lp_id_index(base, 900), body, &a->sheet_open, &spec);
    if (state && (chosen == 0 || chosen == 2)) quit_process(a, chosen == 2);

    if (ctx->pass == LP_PASS_EVENT && ctx->in.key_pressed && state && !a->err && ctx->focus != lp_id_index(base, 3)) {
        uint32_t sym = ctx->in.keysym;
        if ((sym == XKB_KEY_Down || sym == XKB_KEY_Up) && a->nvisible > 0) {
            int at = -1;
            for (int v = 0; v < a->nvisible; v++) if (a->sample.procs[a->visible[v]].pid == a->selected_pid) at = v;
            at = at < 0 ? 0 : at + (sym == XKB_KEY_Down ? 1 : -1);
            if (at < 0) at = 0;
            if (at >= a->nvisible) at = a->nvisible - 1;
            a->selected_pid = a->sample.procs[a->visible[at]].pid;
            ctx->dirty = 1;
        }
        if ((ctx->in.mods & (LP_MOD_LOGO | LP_MOD_ALT)) == (LP_MOD_LOGO | LP_MOD_ALT) && (sym == XKB_KEY_q || sym == XKB_KEY_Q)) {
            ask_quit(a);
            ctx->dirty = 1;
        }
    }

    lp_rect area = body;
    lp_rect bar = lp_toolbar(ctx, &area);
    lp_rect summary = lp_rect_cut_bottom(&area, SUMMARY_H);
    paint_toolbar(ctx, a, bar, base);
    paint_list(ctx, a, area, base);
    paint_summary(ctx, a, summary);
    lp_sheet_draw(ctx, lp_id_index(base, 900), body, a->sheet_open, &spec);
}

static void entry(lp_menu_model *m, const char *label, const char *shortcut, int arg, int checked, int disabled) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "%s", label);
    e->shortcut = shortcut;
    e->command = LP_CMD_APP;
    e->arg = arg;
    e->checked = checked;
    e->disabled = disabled;
}

static void activity_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    const struct activity *a = state;
    if (!a) return;
    if (menu == LP_MENU_FILE) {
        entry(m, "Quit Process…", "⌥⌘Q", LP_ACTIVITY_QUIT_PROCESS, 0, !can_quit(a));
    } else if (menu == LP_MENU_VIEW && m->count + 3 <= LP_MENU_MAX_ENTRIES) {
        lp_menu_entry *sep = &m->entries[m->count++];
        memset(sep, 0, sizeof *sep);
        sep->separator = 1;
        entry(m, "CPU", NULL, LP_ACTIVITY_VIEW_CPU, a->view == 0, 0);
        entry(m, "Memory", NULL, LP_ACTIVITY_VIEW_MEMORY, a->view == 1, 0);
    }
}

static void *activity_create(lp_desktop *d, const char *window_id) {
    struct activity *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    snprintf(a->window_id, sizeof a->window_id, "%s", window_id);
    a->desk = d;
    snprintf(a->root, sizeof a->root, "/proc");
    a->sort = LP_PROC_SORT_CPU;
    a->descending = 1;
    a->signal = lp_proc_signal;
    lp_text_buffer_set(&a->query, "");
    refresh(a);
    if (d) a->timer = lp_desktop_add_timer(d, REFRESH_MS, on_tick, a);
    return a;
}

static void activity_destroy(void *state) {
    struct activity *a = state;
    if (!a) return;
    if (a->timer) lp_desktop_remove_source(a->desk, a->timer);
    lp_proc_free(&a->sample);
    free(a->visible);
    free(a);
}

const lp_app lp_app_activity = {
    .id = "activity", .title = "Activity Monitor", .name = "Activity Monitor", .icon = LP_ICON_GRID, .object = "deviceDisplay",
    .default_rect = { NAN, NAN, 720, 480 }, .min_size = { 520, 320 }, .singleton = 1, .resizable = 1,
    .create = activity_create, .paint = activity_paint, .destroy = activity_destroy,
    .command = activity_command, .menu_entries = activity_menu_entries,
};

/* MARK: - Tests */

void lp_activity_set_root(void *state, const char *root) {
    struct activity *a = state;
    snprintf(a->root, sizeof a->root, "%s", root);
    lp_proc_free(&a->sample);
    a->nvisible = 0;   /* the list pointed into the sample just freed */
    a->selected_pid = 0;
    refresh(a);
}
void lp_activity_set_signal(void *state, int (*signal_fn)(int pid, int force)) { ((struct activity *)state)->signal = signal_fn; }
void lp_activity_search(void *state, const char *query) {
    struct activity *a = state;
    lp_text_buffer_set(&a->query, query);
    rebuild_visible(a);
}
void lp_activity_select(void *state, int pid) { ((struct activity *)state)->selected_pid = pid; }
int lp_activity_selected(const void *state) { return ((const struct activity *)state)->selected_pid; }
int lp_activity_visible_count(const void *state) { return ((const struct activity *)state)->nvisible; }
int lp_activity_visible_pid(const void *state, int v) {
    const struct activity *a = state;
    return v >= 0 && v < a->nvisible ? a->sample.procs[a->visible[v]].pid : 0;
}
double lp_activity_cpu(const void *state, int pid) {
    const lp_process *p = lp_proc_find(&((const struct activity *)state)->sample, pid);
    return p ? p->cpu : -1;
}
int lp_activity_sheet_open(const void *state) { return ((const struct activity *)state)->sheet_open; }
const char *lp_activity_status(const void *state) { return ((const struct activity *)state)->status; }
lp_rect lp_activity_sheet_button(const void *state, lp_ctx *ctx, lp_rect body, int button) {
    char title[128];
    lp_sheet_spec spec;
    sheet_spec(state, &spec, title, sizeof title);
    return lp_sheet_button_rect(ctx, body, &spec, button);
}
