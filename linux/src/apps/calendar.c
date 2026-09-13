/* Calendar — Month, Week and Day views of the events in
 * ~/.local/share/maryui/calendar, and an inspector beside them to make or change
 * one: title, place, all day, when it starts and ends, how it repeats, notes.
 * The dates are lp_calendar's arithmetic; the events are .ics files read and
 * written through libical, so other programs can share them, and the folder is
 * watched so their changes show. Today moves at midnight on its own. Linux only
 * (PARITY D15). */
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_calendar.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

enum { VIEW_DAY, VIEW_WEEK, VIEW_MONTH };

#define INSPECTOR_W 292
#define HOUR_H 44
#define GUTTER_W 56
#define ALL_DAY_H 26
#define WEEKDAY_H 22
#define CHIP_H 16
#define FIELD_H LP_SIZE_CONTROL_HEIGHT

struct calendar_app {
    char window_id[12];
    lp_desktop *desk;
    lp_calendar cal;
    int load_err;
    int view;
    lp_date shown, selected, today;
    int week_start;
    lp_source *midnight;
    int watching, scrolled;
    lp_scroll_state hours;
    int editing, edit_index, focus_title;
    lp_text_buffer title, location, start_date, start_time, end_date, end_time, notes;
    int all_day, repeat, original_repeat;
    char message[200];
};

/* MARK: - Model */

static void reload(struct calendar_app *c) {
    char dir[sizeof c->cal.dir];
    snprintf(dir, sizeof dir, "%s", c->cal.dir);
    c->load_err = lp_calendar_load(&c->cal, dir);
    if (c->editing && c->edit_index >= c->cal.count) c->editing = 0;   /* it went while it was open */
}

static void dirty(struct calendar_app *c) {
    if (c->desk && c->desk->on_app_dirty) c->desk->on_app_dirty(c->desk, c->window_id);
}

static void visible_range(const struct calendar_app *c, lp_date *first, int *days) {
    if (c->view == VIEW_MONTH) {
        lp_date cells[42];
        lp_calendar_month_grid(c->shown.year, c->shown.month, c->week_start, cells);
        *first = cells[0];
        *days = 42;
    } else if (c->view == VIEW_WEEK) {
        *first = lp_date_add_days(c->shown, -((lp_date_weekday(c->shown) - c->week_start + 7) % 7));
        *days = 7;
    } else {
        *first = c->shown;
        *days = 1;
    }
}

static void move(struct calendar_app *c, int delta) {
    if (c->view == VIEW_MONTH) c->shown = lp_date_add_months(c->shown, delta);
    else c->shown = lp_date_add_days(c->shown, delta * (c->view == VIEW_WEEK ? 7 : 1));
    c->selected = c->shown;
}

static void write_date(lp_text_buffer *b, lp_date d) {
    char text[16];
    snprintf(text, sizeof text, "%04d-%02d-%02d", d.year, d.month, d.day);
    lp_text_buffer_set(b, text);
}

static void write_time(lp_text_buffer *b, time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    char text[8];
    snprintf(text, sizeof text, "%02d:%02d", tm.tm_hour, tm.tm_min);
    lp_text_buffer_set(b, text);
}

static void open_new(struct calendar_app *c, lp_date day, int minutes) {
    c->editing = 1;
    c->edit_index = -1;
    c->focus_title = 1;
    c->message[0] = 0;
    lp_text_buffer_set(&c->title, "");
    lp_text_buffer_set(&c->location, "");
    lp_text_buffer_set(&c->notes, "");
    time_t start = lp_date_at(day, minutes);
    time_t end = start + 3600;
    write_date(&c->start_date, day);
    write_time(&c->start_time, start);
    write_date(&c->end_date, lp_date_of(end));
    write_time(&c->end_time, end);
    c->all_day = 0;
    c->repeat = c->original_repeat = LP_REPEAT_NONE;
    c->selected = day;
}

static void open_existing(struct calendar_app *c, int index) {
    if (index < 0 || index >= c->cal.count) return;
    const lp_event *e = &c->cal.events[index];
    c->editing = 1;
    c->edit_index = index;
    c->focus_title = 0;
    c->message[0] = 0;
    lp_text_buffer_set(&c->title, e->title);
    lp_text_buffer_set(&c->location, e->location);
    lp_text_buffer_set(&c->notes, e->notes);
    c->all_day = e->all_day;
    write_date(&c->start_date, lp_date_of(e->start));
    write_time(&c->start_time, e->start);
    /* an all-day event's end is the midnight after its last day; people think of the last day */
    write_date(&c->end_date, e->all_day ? lp_date_add_days(lp_date_of(e->end), -1) : lp_date_of(e->end));
    write_time(&c->end_time, e->end);
    c->repeat = c->original_repeat = e->repeat;
}

static void save_editor(struct calendar_app *c) {
    lp_date sd, ed;
    if (!lp_date_parse(c->start_date.text, &sd) || !lp_date_parse(c->end_date.text, &ed)) {
        snprintf(c->message, sizeof c->message, "Write dates as 2026-09-12.");
        return;
    }
    lp_event e;
    memset(&e, 0, sizeof e);
    if (c->edit_index >= 0 && c->edit_index < c->cal.count) e = c->cal.events[c->edit_index];
    if (e.read_only) {
        snprintf(c->message, sizeof c->message, "This event belongs to another program's file and cannot be changed here.");
        return;
    }
    if (c->all_day) {
        if (lp_date_compare(ed, sd) < 0) { snprintf(c->message, sizeof c->message, "The event ends before it starts."); return; }
        e.start = lp_date_at(sd, 0);
        e.end = lp_date_at(lp_date_add_days(ed, 1), 0);
    } else {
        int st = lp_time_parse(c->start_time.text), et = lp_time_parse(c->end_time.text);
        if (st < 0 || et < 0) { snprintf(c->message, sizeof c->message, "Write times as 9:30 or 14:00."); return; }
        e.start = lp_date_at(sd, st);
        e.end = lp_date_at(ed, et);
        if (e.end < e.start) { snprintf(c->message, sizeof c->message, "The event ends before it starts."); return; }
    }
    snprintf(e.title, sizeof e.title, "%s", c->title.len ? c->title.text : "New Event");
    snprintf(e.location, sizeof e.location, "%s", c->location.text);
    snprintf(e.notes, sizeof e.notes, "%s", c->notes.text);
    e.all_day = c->all_day;
    /* a rule another program wrote ("every other Tuesday") survives unless the frequency was changed */
    if (c->edit_index < 0 || c->repeat != c->original_repeat) lp_calendar_repeat_rule((enum lp_repeat)c->repeat, e.rrule, sizeof e.rrule);
    e.repeat = (enum lp_repeat)c->repeat;
    int rc = lp_calendar_save(&c->cal, &e);
    if (rc) {
        snprintf(c->message, sizeof c->message, "%s", rc == -ENOTSUP ? "This build of the desktop has no libical to keep calendars with." : strerror(-rc));
        return;
    }
    c->editing = 0;
    c->message[0] = 0;
    c->selected = sd;
}

static void delete_editing(struct calendar_app *c) {
    if (c->edit_index < 0 || c->edit_index >= c->cal.count) return;
    char uid[96];
    snprintf(uid, sizeof uid, "%s", c->cal.events[c->edit_index].uid);
    int rc = lp_calendar_delete(&c->cal, uid);
    if (rc) { snprintf(c->message, sizeof c->message, "Could not delete it: %s", strerror(-rc)); return; }
    c->editing = 0;
}

static void calendar_command(void *state, lp_desktop *d, int cmd) {
    struct calendar_app *c = state;
    if (!c) return;
    switch ((enum lp_calendar_command)cmd) {
    case LP_CALENDAR_NEW_EVENT: open_new(c, c->selected, 9 * 60); break;
    case LP_CALENDAR_VIEW_DAY: c->view = VIEW_DAY; c->shown = c->selected; break;
    case LP_CALENDAR_VIEW_WEEK: c->view = VIEW_WEEK; c->shown = c->selected; break;
    case LP_CALENDAR_VIEW_MONTH: c->view = VIEW_MONTH; c->shown = c->selected; break;
    case LP_CALENDAR_TODAY: c->today = lp_date_today(); c->shown = c->selected = c->today; c->scrolled = 0; break;
    case LP_CALENDAR_PREVIOUS: move(c, -1); break;
    case LP_CALENDAR_NEXT: move(c, 1); break;
    case LP_CALENDAR_SAVE_EVENT: if (c->editing) save_editor(c); break;
    case LP_CALENDAR_DELETE_EVENT: if (c->editing) delete_editing(c); break;
    case LP_CALENDAR_CANCEL_EDIT: c->editing = 0; c->message[0] = 0; break;
    }
}

static int on_midnight(int fd, uint32_t mask, void *data);

static void arm_midnight(struct calendar_app *c) {
    if (!c->desk) return;
    time_t now = time(NULL);
    time_t next = lp_date_at(lp_date_add_days(lp_date_of(now), 1), 0);
    long ms = (long)(next - now) * 1000 + 500;
    if (ms < 1000) ms = 1000;
    if (!c->midnight) c->midnight = lp_desktop_add_timer(c->desk, (int)ms, on_midnight, c);
    else lp_desktop_update_timer(c->desk, c->midnight, (int)ms);
}

static int on_midnight(int fd, uint32_t mask, void *data) {
    struct calendar_app *c = data;
    c->today = lp_date_today();
    dirty(c);
    arm_midnight(c);
    return 0;
}

/* MARK: - Painting */

static void clock_label(time_t t, char *out, size_t n) {
    struct tm tm;
    localtime_r(&t, &tm);
    int hour = tm.tm_hour % 12;
    snprintf(out, n, "%d:%02d %s", hour ? hour : 12, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
}

static void month_name(int month, int abbreviated, char *out, size_t n) {
    struct tm tm = { 0 };
    tm.tm_mon = month - 1;
    tm.tm_mday = 1;
    tm.tm_year = 100;
    strftime(out, n, abbreviated ? "%b" : "%B", &tm);
}

static void weekday_name(int weekday, int abbreviated, char *out, size_t n) {
    struct tm tm = { 0 };
    tm.tm_wday = weekday;
    strftime(out, n, abbreviated ? "%a" : "%A", &tm);
}

static void period_title(const struct calendar_app *c, char *out, size_t n) {
    char month[32], other[32], weekday[32];
    if (c->view == VIEW_MONTH) {
        month_name(c->shown.month, 0, month, sizeof month);
        snprintf(out, n, "%s %d", month, c->shown.year);
    } else if (c->view == VIEW_WEEK) {
        lp_date first;
        int days;
        visible_range(c, &first, &days);
        lp_date last = lp_date_add_days(first, 6);
        month_name(first.month, 1, month, sizeof month);
        month_name(last.month, 1, other, sizeof other);
        if (first.month == last.month) snprintf(out, n, "%s %d – %d, %d", month, first.day, last.day, last.year);
        else snprintf(out, n, "%s %d – %s %d, %d", month, first.day, other, last.day, last.year);
    } else {
        weekday_name(lp_date_weekday(c->shown), 0, weekday, sizeof weekday);
        month_name(c->shown.month, 0, month, sizeof month);
        snprintf(out, n, "%s, %s %d, %d", weekday, month, c->shown.day, c->shown.year);
    }
}

static lp_accent accent_of(const lp_ctx *ctx) {
    lp_settings fallback = lp_settings_defaults();
    return lp_settings_accent(ctx->settings ? ctx->settings : &fallback);
}

static void paint_toolbar(lp_ctx *ctx, struct calendar_app *c, lp_desktop *d, lp_rect bar, lp_id base) {
    float cy = bar.y + bar.h / 2;
    lp_button_opts chevron = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_CHEVRON_LEFT, 1, 0 };
    lp_size cs = lp_button_measure(ctx, "", chevron);
    float x = bar.x;
    int cmd = -1;
    if (lp_button(ctx, lp_id_index(base, 1), LP_RECT(x, cy - cs.h / 2, cs.w, cs.h), "", chevron)) cmd = LP_CALENDAR_PREVIOUS;
    x += cs.w + LP_SPACE_1;
    lp_button_opts today = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 };
    lp_size ts = lp_button_measure(ctx, "Today", today);
    if (lp_button(ctx, lp_id_index(base, 2), LP_RECT(x, cy - ts.h / 2, ts.w, ts.h), "Today", today)) cmd = LP_CALENDAR_TODAY;
    x += ts.w + LP_SPACE_1;
    chevron.icon = LP_ICON_CHEVRON_RIGHT;
    if (lp_button(ctx, lp_id_index(base, 3), LP_RECT(x, cy - cs.h / 2, cs.w, cs.h), "", chevron)) cmd = LP_CALENDAR_NEXT;
    x += cs.w + LP_SPACE_4;

    static const lp_segment VIEWS[3] = { { "Day", LP_ICON_COUNT }, { "Week", LP_ICON_COUNT }, { "Month", LP_ICON_COUNT } };
    lp_size seg = lp_segmented_measure(ctx, VIEWS, 3, LP_CONTROL_SM);
    lp_button_opts add = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_PLUS, 1, 0 };
    lp_size as = lp_button_measure(ctx, "", add);
    float right = bar.x + bar.w;
    int view = c->view;
    if (lp_segmented(ctx, lp_id_index(base, 4), right - as.w - LP_SPACE_3 - seg.w, cy - seg.h / 2, VIEWS, 3, &view, LP_CONTROL_SM) && view != c->view) {
        cmd = view == VIEW_DAY ? LP_CALENDAR_VIEW_DAY : view == VIEW_WEEK ? LP_CALENDAR_VIEW_WEEK : LP_CALENDAR_VIEW_MONTH;
    }
    if (lp_button(ctx, lp_id_index(base, 5), LP_RECT(right - as.w, cy - as.h / 2, as.w, as.h), "", add)) cmd = LP_CALENDAR_NEW_EVENT;
    if (cmd >= 0 && ctx->pass == LP_PASS_EVENT) { calendar_command(c, d, cmd); ctx->dirty = 1; }

    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        char title[96];
        period_title(c, title, sizeof title);
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_LG;
        st.weight = LP_TEXT_WEIGHT_BOLD;
        st.emboss = 1;
        st.ellipsize = 1;
        float tx = x, tw = right - as.w - LP_SPACE_3 - seg.w - LP_SPACE_4 - tx;
        lp_text_draw(ctx->cr, title, LP_RECT(tx, bar.y, tw, bar.h), &st, LP_ALIGN_START);
    }
}

/* An event's chip: a tinted capsule with its time and title. Returns 1 when clicked. */
static int chip(lp_ctx *ctx, lp_id id, lp_rect r, const lp_event *e, time_t start, int show_time) {
    int clicked = lp_clicked(ctx, id, r);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_accent accent = accent_of(ctx);
        lp_fill_solid(ctx->cr, r, e->all_day ? lp_color_with_alpha(accent.base, lp_is_hot(ctx, id) ? 0.34f : 0.24f) : lp_color_with_alpha(accent.base, lp_is_hot(ctx, id) ? 0.18f : 0.1f), 4);
        if (!e->all_day) lp_fill_solid(ctx->cr, LP_RECT(r.x, r.y + 2, 2, r.h - 4), accent.base, 1);
        char text[200], when[16];
        clock_label(start, when, sizeof when);
        if (show_time && !e->all_day) snprintf(text, sizeof text, "%s %s", when, e->title);
        else snprintf(text, sizeof text, "%s", e->title);
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS;
        st.color = LP_INK_PRIMARY;
        st.ellipsize = 1;
        lp_text_draw(ctx->cr, text, LP_RECT(r.x + 5, r.y, r.w - 8, r.h), &st, LP_ALIGN_START);
    }
    return clicked;
}

static void paint_month(lp_ctx *ctx, struct calendar_app *c, lp_rect area, lp_id base, const lp_occurrence *occ, int nocc) {
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    lp_accent accent = accent_of(ctx);
    lp_rect head = lp_rect_cut_top(&area, WEEKDAY_H);
    float cw = area.w / 7, ch = area.h / 6;
    if (draw) {
        lp_fill_solid(cr, head, LP_PLATINUM_1, 0);
        lp_draw_hairline(cr, head, LP_EDGE_BOTTOM, LP_EDGE_DIVIDER);
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS;
        st.weight = LP_TEXT_WEIGHT_MEDIUM;
        st.color = LP_INK_SECONDARY;
        for (int i = 0; i < 7; i++) {
            char name[16];
            weekday_name((c->week_start + i) % 7, 1, name, sizeof name);
            lp_text_draw(cr, name, LP_RECT(head.x + i * cw + LP_SPACE_2, head.y, cw - LP_SPACE_3, head.h), &st, LP_ALIGN_END);
        }
    }
    lp_date cells[42];
    lp_calendar_month_grid(c->shown.year, c->shown.month, c->week_start, cells);
    for (int i = 0; i < 42; i++) {
        lp_rect r = LP_RECT(roundf(area.x + (i % 7) * cw), roundf(area.y + (i / 7) * ch), ceilf(cw), ceilf(ch));
        time_t day_start = lp_date_at(cells[i], 0), day_end = lp_date_at(lp_date_add_days(cells[i], 1), 0);
        int in_month = cells[i].month == c->shown.month;
        int selected = lp_date_compare(cells[i], c->selected) == 0, today = lp_date_compare(cells[i], c->today) == 0;
        if (draw) {
            lp_fill_solid(cr, r, selected ? lp_color_with_alpha(accent.base, 0.08f) : in_month ? LP_SURFACE_BODY : LP_PLATINUM_1, 0);
            lp_draw_hairline(cr, r, LP_EDGE_RIGHT | LP_EDGE_BOTTOM, LP_EDGE_DIVIDER);
            char number[8];
            snprintf(number, sizeof number, "%d", cells[i].day);
            lp_text_style st = lp_text_style_default();
            st.size_px = LP_TEXT_SM;
            st.tabular_nums = 1;
            st.color = today ? LP_INK_ON_ACCENT : in_month ? LP_INK_PRIMARY : LP_INK_TERTIARY;
            st.weight = today ? LP_TEXT_WEIGHT_BOLD : LP_TEXT_WEIGHT_REGULAR;
            lp_rect badge = LP_RECT(r.x + r.w - 26, r.y + 3, 22, 18);
            if (today) lp_fill_solid(cr, badge, accent.base, 9);
            lp_text_draw(cr, number, badge, &st, LP_ALIGN_CENTER);
        }
        /* the chips that fit, then how many more */
        int fits = (int)((r.h - 24) / (CHIP_H + 2)), shown = 0, more = 0;
        for (int k = 0; k < nocc; k++) {
            if (!(occ[k].start < day_end && (occ[k].end > day_start || (occ[k].end == occ[k].start && occ[k].start >= day_start)))) continue;
            if (shown >= fits || (shown == fits - 1 && more == 0 && k + 1 < nocc && 0)) { more++; continue; }
            lp_rect cr_rect = LP_RECT(r.x + 3, r.y + 22 + shown * (CHIP_H + 2), r.w - 6, CHIP_H);
            if (chip(ctx, lp_id_index(base, 5000 + i * 64 + shown), cr_rect, &c->cal.events[occ[k].event], occ[k].start, 1)) {
                open_existing(c, occ[k].event);
                ctx->dirty = 1;
            }
            shown++;
        }
        if (more && draw) {
            char text[24];
            snprintf(text, sizeof text, "%d more", more);
            lp_text_style st = lp_text_style_default();
            st.size_px = LP_TEXT_XS;
            st.color = LP_INK_TERTIARY;
            lp_text_draw(cr, text, LP_RECT(r.x + 6, r.y + r.h - 16, r.w - 8, 14), &st, LP_ALIGN_START);
        }
        if (ctx->pass == LP_PASS_EVENT && (ctx->in.pressed & LP_BUTTON_LEFT) && lp_hit(ctx, r)) {
            if (ctx->in.double_click) open_new(c, cells[i], 9 * 60);
            else c->selected = cells[i];
            ctx->dirty = 1;
        }
    }
}

static void paint_days(lp_ctx *ctx, struct calendar_app *c, lp_rect area, lp_id base, lp_date first, int days, const lp_occurrence *occ, int nocc) {
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    lp_accent accent = accent_of(ctx);
    lp_rect head = lp_rect_cut_top(&area, WEEKDAY_H);
    lp_rect allday = lp_rect_cut_top(&area, ALL_DAY_H);
    float colw = (area.w - GUTTER_W) / days;
    lp_text_style small = lp_text_style_default();
    small.size_px = LP_TEXT_XS;
    small.color = LP_INK_SECONDARY;
    small.tabular_nums = 1;
    if (draw) {
        lp_fill_solid(cr, head, LP_PLATINUM_1, 0);
        lp_fill_solid(cr, allday, LP_PLATINUM_1, 0);
        lp_draw_hairline(cr, allday, LP_EDGE_BOTTOM, LP_EDGE_DIVIDER);
        lp_text_draw(cr, "all-day", LP_RECT(allday.x, allday.y, GUTTER_W - LP_SPACE_2, allday.h), &small, LP_ALIGN_END);
        for (int i = 0; i < days; i++) {
            lp_date d = lp_date_add_days(first, i);
            char name[16], label[32];
            weekday_name(lp_date_weekday(d), 1, name, sizeof name);
            snprintf(label, sizeof label, "%s %d", name, d.day);
            lp_text_style st = small;
            if (lp_date_compare(d, c->today) == 0) { st.color = accent.base; st.weight = LP_TEXT_WEIGHT_BOLD; }
            lp_text_draw(cr, label, LP_RECT(head.x + GUTTER_W + i * colw, head.y, colw, head.h), &st, LP_ALIGN_CENTER);
        }
    }
    for (int i = 0; i < days; i++) {
        lp_date d = lp_date_add_days(first, i);
        time_t day_start = lp_date_at(d, 0), day_end = lp_date_at(lp_date_add_days(d, 1), 0);
        int shown = 0;
        for (int k = 0; k < nocc && shown < 1; k++) {
            const lp_event *e = &c->cal.events[occ[k].event];
            if (!e->all_day || !(occ[k].start < day_end && occ[k].end > day_start)) continue;
            lp_rect r = LP_RECT(allday.x + GUTTER_W + i * colw + 2, allday.y + 4, colw - 4, CHIP_H + 2);
            if (chip(ctx, lp_id_index(base, 6000 + i * 8 + shown), r, e, occ[k].start, 0)) { open_existing(c, occ[k].event); ctx->dirty = 1; }
            shown++;
        }
    }

    if (!c->scrolled) { c->hours.y = 8 * HOUR_H - LP_SPACE_2; c->scrolled = 1; }   /* the working day, not midnight */
    lp_rect grid = lp_scroll_begin(ctx, lp_id_index(base, 30), area, (lp_size){ area.w, 24 * HOUR_H }, &c->hours);
    if (draw) {
        lp_fill_solid(cr, LP_RECT(grid.x, grid.y, grid.w, 24 * HOUR_H), LP_SURFACE_BODY, 0);
        for (int h = 0; h < 24; h++) {
            float y = grid.y + h * HOUR_H;
            lp_fill_solid(cr, LP_RECT(grid.x + GUTTER_W, y, grid.w - GUTTER_W, 1), LP_EDGE_DIVIDER, 0);
            if (h) {
                char label[12];
                snprintf(label, sizeof label, "%d %s", h % 12 ? h % 12 : 12, h < 12 ? "AM" : "PM");
                lp_text_draw(cr, label, LP_RECT(grid.x, y - 8, GUTTER_W - LP_SPACE_2, 16), &small, LP_ALIGN_END);
            }
        }
        for (int i = 0; i <= days; i++) lp_fill_solid(cr, LP_RECT(grid.x + GUTTER_W + i * colw, grid.y, 1, 24 * HOUR_H), LP_EDGE_DIVIDER, 0);
    }
    for (int i = 0; i < days; i++) {
        lp_date d = lp_date_add_days(first, i);
        time_t day_start = lp_date_at(d, 0), day_end = lp_date_at(lp_date_add_days(d, 1), 0);
        lp_rect column = LP_RECT(grid.x + GUTTER_W + i * colw, grid.y, colw, 24 * HOUR_H);
        int blocks = 0;
        for (int k = 0; k < nocc; k++) {
            const lp_event *e = &c->cal.events[occ[k].event];
            if (e->all_day || !(occ[k].start < day_end && occ[k].end > day_start) || blocks >= 32) continue;
            time_t s = occ[k].start > day_start ? occ[k].start : day_start, t = occ[k].end < day_end ? occ[k].end : day_end;
            float y0 = column.y + (float)(s - day_start) / 3600.0f * HOUR_H, y1 = column.y + (float)(t - day_start) / 3600.0f * HOUR_H;
            lp_rect r = LP_RECT(column.x + 2, y0 + 1, column.w - 4, fmaxf(y1 - y0 - 2, CHIP_H));
            if (chip(ctx, lp_id_index(base, 7000 + i * 40 + blocks), r, e, occ[k].start, 1)) { open_existing(c, occ[k].event); ctx->dirty = 1; }
            blocks++;
        }
        if (ctx->pass == LP_PASS_EVENT && (ctx->in.pressed & LP_BUTTON_LEFT) && ctx->in.double_click && lp_hit(ctx, column)) {
            int hour = (int)((ctx->in.my - column.y) / HOUR_H);
            open_new(c, d, (hour < 0 ? 0 : hour > 23 ? 23 : hour) * 60);
            ctx->dirty = 1;
        }
        if (draw && lp_date_compare(d, c->today) == 0) {
            time_t now = time(NULL);
            float y = column.y + (float)(now - day_start) / 3600.0f * HOUR_H;
            lp_fill_solid(cr, LP_RECT(column.x, y - 1, column.w, 2), accent.base, 0);
        }
    }
    lp_scroll_end(ctx);
}

static float field_row(lp_ctx *ctx, lp_id id, float x, float y, float w, lp_text_buffer *b, const char *placeholder) {
    lp_text_field(ctx, id, LP_RECT(x, y, w, FIELD_H), b, (lp_text_field_opts){ .placeholder = placeholder, .icon = LP_ICON_COUNT });
    return y + FIELD_H + LP_SPACE_2;
}

static void label_at(lp_ctx *ctx, const char *text, float x, float y, float w) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS;
    st.weight = LP_TEXT_WEIGHT_MEDIUM;
    st.color = LP_INK_SECONDARY;
    lp_text_draw(ctx->cr, text, LP_RECT(x, y, w, 16), &st, LP_ALIGN_START);
}

static void paint_inspector(lp_ctx *ctx, struct calendar_app *c, lp_desktop *d, lp_rect r, lp_id base) {
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    int read_only = c->edit_index >= 0 && c->edit_index < c->cal.count && c->cal.events[c->edit_index].read_only;
    if (draw) {
        lp_fill_vgradient(cr, r, LP_PLATINUM_1, LP_PLATINUM_2, 0);
        lp_draw_hairline(cr, r, LP_EDGE_LEFT, LP_EDGE_DIVIDER);
    }
    lp_rect in = lp_rect_inset(r, LP_SPACE_4, LP_SPACE_4);
    float x = in.x, w = in.w, y = in.y;
    if (draw) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_LG;
        st.weight = LP_TEXT_WEIGHT_BOLD;
        st.emboss = 1;
        lp_text_draw(cr, c->edit_index < 0 ? "New Event" : read_only ? "Event (read only)" : "Event", LP_RECT(x, y, w, 22), &st, LP_ALIGN_START);
    }
    y += 22 + LP_SPACE_3;
    lp_id title_id = lp_id_index(base, 100);
    if (c->focus_title) { ctx->focus = title_id; c->focus_title = 0; }
    label_at(ctx, "Title", x, y, w); y += 16;
    y = field_row(ctx, title_id, x, y, w, &c->title, "New Event");
    label_at(ctx, "Location", x, y, w); y += 16;
    y = field_row(ctx, lp_id_index(base, 101), x, y, w, &c->location, "Optional");
    int all_day = c->all_day;
    if (lp_checkbox(ctx, lp_id_index(base, 102), x, y + 2, &all_day, "All day", read_only) && !read_only) { c->all_day = all_day; ctx->dirty = 1; }
    y += FIELD_H + LP_SPACE_2;
    float date_w = c->all_day ? w : w * 0.6f - LP_SPACE_1, time_x = x + w * 0.6f + LP_SPACE_1, time_w = w * 0.4f - LP_SPACE_1;
    label_at(ctx, "Starts", x, y, w); y += 16;
    field_row(ctx, lp_id_index(base, 103), x, y, date_w, &c->start_date, "2026-09-12");
    if (!c->all_day) field_row(ctx, lp_id_index(base, 104), time_x, y, time_w, &c->start_time, "9:00");
    y += FIELD_H + LP_SPACE_2;
    label_at(ctx, "Ends", x, y, w); y += 16;
    field_row(ctx, lp_id_index(base, 105), x, y, date_w, &c->end_date, "2026-09-12");
    if (!c->all_day) field_row(ctx, lp_id_index(base, 106), time_x, y, time_w, &c->end_time, "10:00");
    y += FIELD_H + LP_SPACE_2;
    label_at(ctx, "Repeat", x, y, w); y += 16;
    static const lp_segment REPEATS[5] = { { "Never", LP_ICON_COUNT }, { "Day", LP_ICON_COUNT }, { "Week", LP_ICON_COUNT }, { "Month", LP_ICON_COUNT }, { "Year", LP_ICON_COUNT } };
    int repeat = c->repeat;
    if (lp_segmented(ctx, lp_id_index(base, 107), x, y, REPEATS, 5, &repeat, LP_CONTROL_SM) && !read_only) { c->repeat = repeat; ctx->dirty = 1; }
    y += lp_segmented_measure(ctx, REPEATS, 5, LP_CONTROL_SM).h + LP_SPACE_3;
    label_at(ctx, "Notes", x, y, w); y += 16;
    y = field_row(ctx, lp_id_index(base, 108), x, y, w, &c->notes, "Optional");
    if (draw && c->message[0]) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS;
        st.color = LP_INK_SECONDARY;
        lp_text_layout *l = lp_text_layout_new(cr, c->message, (int)strlen(c->message), &st, w);
        if (l) { lp_text_layout_draw(cr, l, x, y, LP_INK_SECONDARY); lp_text_layout_free(l); }
    }

    /* Delete · Cancel · Save along the bottom */
    lp_button_opts save = { LP_BUTTON_PRIMARY, LP_CONTROL_SM, LP_ICON_COUNT, 0, read_only };
    lp_button_opts cancel = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 };
    lp_button_opts del = { LP_BUTTON_QUIET, LP_CONTROL_SM, LP_ICON_TRASH, 0, read_only };
    lp_size ss = lp_button_measure(ctx, "Save", save), cs = lp_button_measure(ctx, "Cancel", cancel), ds = lp_button_measure(ctx, "Delete", del);
    float by = in.y + in.h - ss.h;
    int cmd = -1;
    if (lp_button(ctx, lp_id_index(base, 110), LP_RECT(in.x + in.w - ss.w, by, ss.w, ss.h), "Save", save) && !read_only) cmd = LP_CALENDAR_SAVE_EVENT;
    if (lp_button(ctx, lp_id_index(base, 111), LP_RECT(in.x + in.w - ss.w - LP_SPACE_2 - cs.w, by, cs.w, cs.h), "Cancel", cancel)) cmd = LP_CALENDAR_CANCEL_EDIT;
    if (c->edit_index >= 0 && lp_button(ctx, lp_id_index(base, 112), LP_RECT(in.x, by, ds.w, ds.h), "Delete", del) && !read_only) cmd = LP_CALENDAR_DELETE_EVENT;
    if (cmd >= 0 && ctx->pass == LP_PASS_EVENT) { calendar_command(c, d, cmd); ctx->dirty = 1; }
}

static void calendar_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct calendar_app *preview;
    struct calendar_app *c = state;
    if (!c) {
        if (!preview) {
            preview = calloc(1, sizeof *preview);
            if (preview) { preview->today = preview->shown = preview->selected = lp_date_today(); preview->view = VIEW_MONTH; preview->edit_index = -1; }
        }
        c = preview;
        if (!c) return;
    }
    lp_id base = LP_ID("calendar");
    if (ctx->pass == LP_PASS_EVENT && ctx->in.key_pressed && state) {
        uint32_t sym = ctx->in.keysym;
        int cmd = -1, command_key = (ctx->in.mods & (LP_MOD_LOGO | LP_MOD_CTRL)) != 0;
        if (command_key && (sym == XKB_KEY_n || sym == XKB_KEY_N)) cmd = LP_CALENDAR_NEW_EVENT;
        else if (command_key && sym == XKB_KEY_1) cmd = LP_CALENDAR_VIEW_DAY;
        else if (command_key && sym == XKB_KEY_2) cmd = LP_CALENDAR_VIEW_WEEK;
        else if (command_key && sym == XKB_KEY_3) cmd = LP_CALENDAR_VIEW_MONTH;
        else if (c->editing && sym == XKB_KEY_Escape) cmd = LP_CALENDAR_CANCEL_EDIT;
        else if (c->editing && (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter)) cmd = LP_CALENDAR_SAVE_EVENT;
        else if (!c->editing && sym == XKB_KEY_Left) cmd = LP_CALENDAR_PREVIOUS;
        else if (!c->editing && sym == XKB_KEY_Right) cmd = LP_CALENDAR_NEXT;
        if (cmd >= 0) {
            calendar_command(c, d, cmd);
            ctx->dirty = 1;
            ctx->in.key_pressed = 0;   /* not typed into a field as well */
            ctx->in.keysym = 0;
        }
    }
    lp_rect area = body;
    lp_rect bar = lp_toolbar(ctx, &area);
    paint_toolbar(ctx, c, state ? d : NULL, bar, base);
    if (c->editing) paint_inspector(ctx, c, state ? d : NULL, lp_rect_cut_right(&area, INSPECTOR_W), base);
    lp_date first;
    int days;
    visible_range(c, &first, &days);
    lp_occurrence *occ = NULL;
    int nocc = lp_calendar_occurrences(&c->cal, lp_date_at(first, 0), lp_date_at(lp_date_add_days(first, days), 0), &occ);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_fill_solid(ctx->cr, area, LP_SURFACE_BODY, 0);
    if (c->view == VIEW_MONTH) paint_month(ctx, c, area, base, occ, nocc);
    else paint_days(ctx, c, area, base, first, days, occ, nocc);
    free(occ);
}

static void entry(lp_menu_model *m, const char *label, const char *shortcut, int arg, int checked) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "%s", label);
    e->shortcut = shortcut;
    e->command = LP_CMD_APP;
    e->arg = arg;
    e->checked = checked;
}

static void calendar_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    const struct calendar_app *c = state;
    if (!c) return;
    if (menu == LP_MENU_FILE) {
        entry(m, "New Event", "⌘N", LP_CALENDAR_NEW_EVENT, 0);
    } else if (menu == LP_MENU_VIEW) {
        entry(m, "Day", "⌘1", LP_CALENDAR_VIEW_DAY, c->view == VIEW_DAY);
        entry(m, "Week", "⌘2", LP_CALENDAR_VIEW_WEEK, c->view == VIEW_WEEK);
        entry(m, "Month", "⌘3", LP_CALENDAR_VIEW_MONTH, c->view == VIEW_MONTH);
    } else if (menu == LP_MENU_GO) {
        entry(m, "Today", NULL, LP_CALENDAR_TODAY, 0);
        entry(m, "Previous", "←", LP_CALENDAR_PREVIOUS, 0);
        entry(m, "Next", "→", LP_CALENDAR_NEXT, 0);
    }
}

static int calendar_notify(void *state, lp_desktop *d, const char *dir) {
    struct calendar_app *c = state;
    if (!c || strcmp(dir, c->cal.dir) != 0) return 0;
    reload(c);
    return 1;
}

static void *calendar_create(lp_desktop *d, const char *window_id) {
    struct calendar_app *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    setlocale(LC_TIME, "");   /* month and weekday names, and the first day of the week, as the user's locale has them */
    snprintf(c->window_id, sizeof c->window_id, "%s", window_id);
    c->desk = d;
    c->view = VIEW_MONTH;
    c->today = c->shown = c->selected = lp_date_today();
    c->week_start = lp_calendar_week_start();
    c->edit_index = -1;
    lp_calendar_default_dir(c->cal.dir, sizeof c->cal.dir);
    reload(c);
    if (d && d->watch) { d->watch(d, c->cal.dir, 1); c->watching = 1; }
    arm_midnight(c);
    return c;
}

static void calendar_destroy(void *state) {
    struct calendar_app *c = state;
    if (!c) return;
    if (c->midnight) lp_desktop_remove_source(c->desk, c->midnight);
    if (c->watching && c->desk && c->desk->watch) c->desk->watch(c->desk, c->cal.dir, 0);
    lp_calendar_free(&c->cal);
    free(c);
}

const lp_app lp_app_calendar = {
    .id = "calendar", .title = "Calendar", .name = "Calendar", .icon = LP_ICON_CLOCK, .object = "appCalendar", .dock = 1,
    .default_rect = { NAN, NAN, 900, 600 }, .min_size = { 640, 440 }, .singleton = 1, .resizable = 1,
    .create = calendar_create, .paint = calendar_paint, .destroy = calendar_destroy,
    .command = calendar_command, .menu_entries = calendar_menu_entries, .notify = calendar_notify,
};

/* MARK: - Tests */

void lp_calendar_app_set_dir(void *state, const char *dir) {
    struct calendar_app *c = state;
    snprintf(c->cal.dir, sizeof c->cal.dir, "%s", dir);
    reload(c);
}
void lp_calendar_app_show(void *state, int year, int month, int day) {
    struct calendar_app *c = state;
    c->shown = c->selected = (lp_date){ year, month, day };
}
void lp_calendar_app_shown(const void *state, int *year, int *month, int *day) {
    const struct calendar_app *c = state;
    *year = c->shown.year;
    *month = c->shown.month;
    *day = c->shown.day;
}
int lp_calendar_app_view(const void *state) { return ((const struct calendar_app *)state)->view; }
int lp_calendar_app_editing(const void *state) { return ((const struct calendar_app *)state)->editing; }
int lp_calendar_app_event_count(const void *state) { return ((const struct calendar_app *)state)->cal.count; }
const char *lp_calendar_app_message(const void *state) { return ((const struct calendar_app *)state)->message; }
void lp_calendar_app_fill(void *state, const char *title, const char *start_date, const char *start_time,
                          const char *end_date, const char *end_time, int all_day, int repeat) {
    struct calendar_app *c = state;
    lp_text_buffer_set(&c->title, title);
    lp_text_buffer_set(&c->start_date, start_date);
    lp_text_buffer_set(&c->start_time, start_time);
    lp_text_buffer_set(&c->end_date, end_date);
    lp_text_buffer_set(&c->end_time, end_time);
    c->all_day = all_day;
    c->repeat = repeat;
}
void lp_calendar_app_edit(void *state, int index) { open_existing(state, index); }
