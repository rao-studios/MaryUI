/* Calendar. Everywhere: the date arithmetic (leap years, weekdays, stepping by
 * days and months, the six-week grid), what a person types for a date or a
 * time, days that are 23 or 25 hours long, and the app's navigation, its editor
 * refusing what it cannot keep, and a DRAW pass over each view. With libical:
 * events saved and read back, a weekly repeat that stays at 9:00 across the
 * clocks going back, another program's file shown but not rewritten, and the
 * app saving and deleting an event. The time zone is a POSIX rule, so no
 * tzdata is needed. Linux only (PARITY D15). */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "lp_test.h"
#include "maryui/lp_calendar.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"

static char root[512], dir[600];
static lp_desktop d;

static int same(lp_date a, int y, int m, int day) { return a.year == y && a.month == m && a.day == day; }

LP_TEST(counts_days_leap_years_and_weekdays) {
    LP_ASSERT_EQ(lp_date_days_in_month(2028, 2), 29);
    LP_ASSERT_EQ(lp_date_days_in_month(2100, 2), 28);
    LP_ASSERT_EQ(lp_date_days_in_month(2000, 2), 29);
    LP_ASSERT_EQ(lp_date_days_in_month(2026, 9), 30);
    LP_ASSERT_EQ(lp_date_weekday((lp_date){ 2026, 9, 12 }), 6);   /* a Saturday */
    LP_ASSERT_EQ(lp_date_weekday((lp_date){ 1970, 1, 1 }), 4);
    LP_ASSERT_EQ(lp_date_weekday((lp_date){ 1969, 12, 31 }), 3);
    LP_ASSERT_EQ(lp_date_to_days((lp_date){ 1970, 1, 1 }), 0);
    LP_ASSERT(same(lp_date_from_days(lp_date_to_days((lp_date){ 2400, 2, 29 })), 2400, 2, 29));
}

LP_TEST(steps_by_days_and_months) {
    LP_ASSERT(same(lp_date_add_days((lp_date){ 2028, 2, 28 }, 1), 2028, 2, 29));
    LP_ASSERT(same(lp_date_add_days((lp_date){ 2028, 2, 28 }, 2), 2028, 3, 1));
    LP_ASSERT(same(lp_date_add_days((lp_date){ 2026, 12, 31 }, 1), 2027, 1, 1));
    LP_ASSERT(same(lp_date_add_days((lp_date){ 2026, 1, 1 }, -1), 2025, 12, 31));
    LP_ASSERT(same(lp_date_add_months((lp_date){ 2026, 1, 31 }, 1), 2026, 2, 28));
    LP_ASSERT(same(lp_date_add_months((lp_date){ 2028, 1, 31 }, 1), 2028, 2, 29));
    LP_ASSERT(same(lp_date_add_months((lp_date){ 2026, 1, 15 }, -1), 2025, 12, 15));
    LP_ASSERT(same(lp_date_add_months((lp_date){ 2026, 11, 30 }, 14), 2028, 1, 30));
    LP_ASSERT(lp_date_compare((lp_date){ 2026, 9, 12 }, (lp_date){ 2026, 9, 13 }) < 0);
}

LP_TEST(lays_a_month_on_six_weeks) {
    lp_date cells[42];
    lp_calendar_month_grid(2026, 9, 0, cells);   /* September 2026 starts on a Tuesday */
    LP_ASSERT(same(cells[0], 2026, 8, 30));
    LP_ASSERT(same(cells[2], 2026, 9, 1));
    LP_ASSERT(same(cells[41], 2026, 10, 10));
    lp_calendar_month_grid(2026, 9, 1, cells);
    LP_ASSERT(same(cells[0], 2026, 8, 31));
    lp_calendar_month_grid(2026, 2, 0, cells);   /* February 2026 starts on a Sunday: no days from January */
    LP_ASSERT(same(cells[0], 2026, 2, 1));
    int ws = lp_calendar_week_start();
    LP_ASSERT(ws >= 0 && ws <= 6);
}

LP_TEST(reads_dates_and_times_as_typed) {
    lp_date out;
    LP_ASSERT(lp_date_parse("2026-09-12", &out) && same(out, 2026, 9, 12));
    LP_ASSERT(lp_date_parse(" 2026-9-3 ", &out) && same(out, 2026, 9, 3));
    LP_ASSERT(!lp_date_parse("2026-02-30", &out));
    LP_ASSERT(!lp_date_parse("12/09/2026", &out));
    LP_ASSERT(!lp_date_parse("2026-09-12x", &out));
    LP_ASSERT_EQ(lp_time_parse("9:05"), 545);
    LP_ASSERT_EQ(lp_time_parse("21:30"), 1290);
    LP_ASSERT_EQ(lp_time_parse("24:00"), -1);
    LP_ASSERT_EQ(lp_time_parse("9"), -1);
}

LP_TEST(knows_a_day_can_be_23_or_25_hours) {
    /* EST5EDT: the clocks go back on November 1, 2026 and forward on March 8 */
    LP_ASSERT_EQ(lp_date_at((lp_date){ 2026, 11, 2 }, 0) - lp_date_at((lp_date){ 2026, 11, 1 }, 0), 25 * 3600);
    LP_ASSERT_EQ(lp_date_at((lp_date){ 2026, 3, 9 }, 0) - lp_date_at((lp_date){ 2026, 3, 8 }, 0), 23 * 3600);
    time_t nine = lp_date_at((lp_date){ 2026, 11, 2 }, 9 * 60);
    struct tm tm;
    localtime_r(&nine, &tm);
    LP_ASSERT_EQ(tm.tm_hour, 9);
    LP_ASSERT(same(lp_date_of(nine), 2026, 11, 2));
}

static void *open_calendar(char *id) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    if (lp_desktop_open_app_with(&d, "calendar", NULL, NULL, id) != 1) return NULL;
    void *c = lp_desktop_instance(&d, id)->state;
    lp_files_delete_tree(dir);
    lp_calendar_app_set_dir(c, dir);
    lp_calendar_app_show(c, 2026, 9, 12);
    return c;
}

LP_TEST(the_app_steps_through_months_weeks_and_days) {
    char id[12];
    void *c = open_calendar(id);
    LP_ASSERT(c != NULL);
    int y, m, day;
    LP_ASSERT_EQ(lp_calendar_app_view(c), 2);   /* Month */
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_NEXT);
    lp_calendar_app_shown(c, &y, &m, &day);
    LP_ASSERT(y == 2026 && m == 10);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_VIEW_WEEK);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_PREVIOUS);
    lp_calendar_app_shown(c, &y, &m, &day);
    LP_ASSERT(y == 2026 && m == 10 && day == 5);   /* October 12, a week back */
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_VIEW_DAY);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_NEXT);
    lp_calendar_app_shown(c, &y, &m, &day);
    LP_ASSERT(m == 10 && day == 6);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_TODAY);
    lp_calendar_app_shown(c, &y, &m, &day);
    lp_date today = lp_date_today();
    LP_ASSERT(same((lp_date){ y, m, day }, today.year, today.month, today.day));
    lp_desktop_close_window(&d, id);
}

LP_TEST(the_editor_refuses_what_it_cannot_keep) {
    char id[12];
    void *c = open_calendar(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_NEW_EVENT);
    LP_ASSERT(lp_calendar_app_editing(c));
    lp_calendar_app_fill(c, "Standup", "2026-09-14", "10:00", "2026-09-14", "9:00", 0, LP_REPEAT_NONE);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_SAVE_EVENT);
    LP_ASSERT(lp_calendar_app_editing(c));
    LP_ASSERT_STR(lp_calendar_app_message(c), "The event ends before it starts.");
    lp_calendar_app_fill(c, "Standup", "2026-09-31", "9:00", "2026-09-31", "9:15", 0, LP_REPEAT_NONE);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_SAVE_EVENT);
    LP_ASSERT_STR(lp_calendar_app_message(c), "Write dates as 2026-09-12.");
    lp_calendar_app_fill(c, "Standup", "2026-09-14", "9", "2026-09-14", "9:15", 0, LP_REPEAT_NONE);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_SAVE_EVENT);
    LP_ASSERT_STR(lp_calendar_app_message(c), "Write times as 9:30 or 14:00.");
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_CANCEL_EDIT);
    LP_ASSERT(!lp_calendar_app_editing(c));
    lp_desktop_close_window(&d, id);
}

LP_TEST(the_app_paints_each_view) {
    char id[12];
    void *c = open_calendar(id);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 900, 558);
    cairo_t *cr = cairo_create(s);
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    lp_input in = { .mx = -1, .my = -1 };
    static const int VIEWS[3] = { LP_CALENDAR_VIEW_MONTH, LP_CALENDAR_VIEW_WEEK, LP_CALENDAR_VIEW_DAY };
    for (int v = 0; v < 3; v++) {
        lp_desktop_run_command(&d, LP_CMD_APP, VIEWS[v]);
        if (v == 1) lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_NEW_EVENT);   /* the inspector too */
        for (int pass = 0; pass < 2; pass++) {
            lp_ctx_begin(&ctx, pass ? LP_PASS_DRAW : LP_PASS_EVENT, pass ? cr : NULL, &in, LP_RECT(0, 0, 900, 558), 1000 + v * 10 + pass);
            lp_app_calendar.paint(c, &ctx, LP_RECT(0, 0, 900, 558), &d);
            lp_ctx_end(&ctx);
        }
    }
    cairo_surface_flush(s);
    uint32_t px;
    memcpy(&px, cairo_image_surface_get_data(s) + 300 * cairo_image_surface_get_stride(s) + 4 * 200, 4);
    LP_ASSERT((px >> 24) == 255);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    lp_desktop_close_window(&d, id);
}

#ifdef HAVE_ICAL

static int hour_of(time_t t) { struct tm tm; localtime_r(&t, &tm); return tm.tm_hour; }

LP_TEST(saves_an_event_and_reads_it_back) {
    lp_files_delete_tree(dir);
    lp_calendar cal = { 0 };
    LP_ASSERT_EQ(lp_calendar_load(&cal, dir), 0);
    LP_ASSERT_EQ(cal.count, 0);
    lp_event e = { 0 };
    snprintf(e.title, sizeof e.title, "Dentist");
    snprintf(e.location, sizeof e.location, "Main St");
    snprintf(e.notes, sizeof e.notes, "Bring the forms");
    e.start = lp_date_at((lp_date){ 2026, 9, 15 }, 14 * 60 + 30);
    e.end = e.start + 45 * 60;
    LP_ASSERT_EQ(lp_calendar_save(&cal, &e), 0);
    LP_ASSERT(e.uid[0] != 0);
    LP_ASSERT_EQ(cal.count, 1);
    const lp_event *back = &cal.events[0];
    LP_ASSERT_STR(back->title, "Dentist");
    LP_ASSERT_STR(back->location, "Main St");
    LP_ASSERT_STR(back->notes, "Bring the forms");
    LP_ASSERT(back->start == e.start && back->end == e.end);
    LP_ASSERT(!back->read_only);
    lp_event trip = { 0 };
    snprintf(trip.title, sizeof trip.title, "Trip");
    trip.all_day = 1;
    trip.start = lp_date_at((lp_date){ 2026, 9, 20 }, 0);
    trip.end = lp_date_at((lp_date){ 2026, 9, 22 }, 0);
    LP_ASSERT_EQ(lp_calendar_save(&cal, &trip), 0);
    LP_ASSERT_EQ(cal.count, 2);
    lp_occurrence *occ = NULL;
    int n = lp_calendar_occurrences(&cal, lp_date_at((lp_date){ 2026, 9, 21 }, 0), lp_date_at((lp_date){ 2026, 9, 22 }, 0), &occ);
    LP_ASSERT_EQ(n, 1);                         /* the trip's second day */
    LP_ASSERT(n == 1 && cal.events[occ[0].event].all_day);
    free(occ);
    LP_ASSERT_EQ(lp_calendar_delete(&cal, e.uid), 0);
    LP_ASSERT_EQ(cal.count, 1);
    LP_ASSERT_EQ(lp_calendar_delete(&cal, "nobody"), -ENOENT);
    lp_calendar_free(&cal);
}

LP_TEST(a_weekly_repeat_stays_at_nine_across_the_clocks_going_back) {
    lp_files_delete_tree(dir);
    lp_calendar cal = { 0 };
    lp_calendar_load(&cal, dir);
    lp_event e = { 0 };
    snprintf(e.title, sizeof e.title, "Standup");
    e.start = lp_date_at((lp_date){ 2026, 10, 19 }, 9 * 60);   /* a Monday */
    e.end = e.start + 15 * 60;
    lp_calendar_repeat_rule(LP_REPEAT_WEEKLY, e.rrule, sizeof e.rrule);
    LP_ASSERT_EQ(lp_calendar_save(&cal, &e), 0);
    LP_ASSERT_EQ(cal.events[0].repeat, LP_REPEAT_WEEKLY);
    lp_occurrence *occ = NULL;
    int n = lp_calendar_occurrences(&cal, lp_date_at((lp_date){ 2026, 10, 1 }, 0), lp_date_at((lp_date){ 2026, 11, 16 }, 0), &occ);
    LP_ASSERT_EQ(n, 4);                         /* Oct 19, 26, Nov 2, 9 — the range ends as November 16 begins */
    for (int i = 0; i < n; i++) LP_ASSERT_EQ(hour_of(occ[i].start), 9);
    LP_ASSERT(n == 4 && same(lp_date_of(occ[2].start), 2026, 11, 2));
    free(occ);
    lp_calendar_free(&cal);
}

LP_TEST(shows_another_programs_file_but_never_rewrites_it) {
    lp_files_delete_tree(dir);
    lp_files_mkdir_p(dir);
    char path[800];
    snprintf(path, sizeof path, "%s/other.ics", dir);
    const char *ics =
        "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Other//EN\r\n"
        "BEGIN:VEVENT\r\nUID:external-1\r\nDTSTART:20260915T130000Z\r\nDTEND:20260915T140000Z\r\nSUMMARY:Imported\r\nEND:VEVENT\r\n"
        "BEGIN:VEVENT\r\nUID:external-2\r\nDTSTART;VALUE=DATE:20260920\r\nDTEND;VALUE=DATE:20260922\r\nSUMMARY:Holiday\r\n"
        "RRULE:FREQ=YEARLY;COUNT=3\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    lp_files_write(path, ics, strlen(ics));
    lp_calendar cal = { 0 };
    LP_ASSERT_EQ(lp_calendar_load(&cal, dir), 0);
    LP_ASSERT_EQ(cal.count, 2);
    const lp_event *imported = strcmp(cal.events[0].uid, "external-1") == 0 ? &cal.events[0] : &cal.events[1];
    struct tm utc = { .tm_year = 126, .tm_mon = 8, .tm_mday = 15, .tm_hour = 13 };
    LP_ASSERT(imported->start == timegm(&utc));
    LP_ASSERT(imported->read_only);
    lp_event copy = *imported;
    LP_ASSERT_EQ(lp_calendar_save(&cal, &copy), -EROFS);
    lp_occurrence *occ = NULL;
    int n = lp_calendar_occurrences(&cal, lp_date_at((lp_date){ 2028, 9, 1 }, 0), lp_date_at((lp_date){ 2028, 10, 1 }, 0), &occ);
    LP_ASSERT_EQ(n, 1);                         /* the holiday's third year, and no fourth */
    free(occ);
    n = lp_calendar_occurrences(&cal, lp_date_at((lp_date){ 2029, 9, 1 }, 0), lp_date_at((lp_date){ 2029, 10, 1 }, 0), &occ);
    LP_ASSERT_EQ(n, 0);
    free(occ);
    lp_calendar_free(&cal);
}

LP_TEST(the_app_saves_and_deletes_an_event) {
    char id[12];
    void *c = open_calendar(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_NEW_EVENT);
    lp_calendar_app_fill(c, "Standup", "2026-09-14", "9:00", "2026-09-14", "9:15", 0, LP_REPEAT_WEEKLY);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_SAVE_EVENT);
    LP_ASSERT(!lp_calendar_app_editing(c));
    LP_ASSERT_EQ(lp_calendar_app_event_count(c), 1);
    lp_calendar_app_edit(c, 0);
    LP_ASSERT(lp_calendar_app_editing(c));
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_DELETE_EVENT);
    LP_ASSERT(!lp_calendar_app_editing(c));
    LP_ASSERT_EQ(lp_calendar_app_event_count(c), 0);
    lp_desktop_close_window(&d, id);
}

#else

LP_TEST(says_the_build_has_no_libical) {
    char id[12];
    void *c = open_calendar(id);
    LP_ASSERT_EQ(lp_calendar_available(), 0);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_NEW_EVENT);
    lp_calendar_app_fill(c, "Standup", "2026-09-14", "9:00", "2026-09-14", "9:15", 0, LP_REPEAT_NONE);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_CALENDAR_SAVE_EVENT);
    LP_ASSERT(lp_calendar_app_editing(c));
    LP_ASSERT(strstr(lp_calendar_app_message(c), "libical") != NULL);
    lp_desktop_close_window(&d, id);
}

#endif

int main(void) {
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_calendar_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    snprintf(dir, sizeof dir, "%s/calendar", root);
    char config[600];
    snprintf(config, sizeof config, "%s/config", root);
    setenv("XDG_CONFIG_HOME", config, 1);
    setenv("XDG_DATA_HOME", root, 1);
    LP_RUN(counts_days_leap_years_and_weekdays);
    LP_RUN(steps_by_days_and_months);
    LP_RUN(lays_a_month_on_six_weeks);
    LP_RUN(reads_dates_and_times_as_typed);
    LP_RUN(knows_a_day_can_be_23_or_25_hours);
    LP_RUN(the_app_steps_through_months_weeks_and_days);
    LP_RUN(the_editor_refuses_what_it_cannot_keep);
    LP_RUN(the_app_paints_each_view);
#ifdef HAVE_ICAL
    LP_RUN(saves_an_event_and_reads_it_back);
    LP_RUN(a_weekly_repeat_stays_at_nine_across_the_clocks_going_back);
    LP_RUN(shows_another_programs_file_but_never_rewrites_it);
    LP_RUN(the_app_saves_and_deletes_an_event);
#else
    LP_RUN(says_the_build_has_no_libical);
#endif
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
