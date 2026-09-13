/* Calendar's model. Dates as a calendar counts them — days in a month, the
 * weekday, stepping by days and months across leap years and the days the
 * clocks change, the six-week grid a month is drawn on, what a person types for
 * a date or a time — and events kept as .ics files in
 * $XDG_DATA_HOME/maryui/calendar, read and written through libical, repeating
 * events expanded for the range on screen. Times are written as floating local
 * wall time, so a 9:00 meeting stays at 9:00 after the clocks change. Files it
 * did not write (other programs', several events in one) are shown but never
 * rewritten. The dates are plain C; events need libical (HAVE_ICAL). Linux only
 * (PARITY D15). */
#ifndef MARYUI_LP_CALENDAR_H
#define MARYUI_LP_CALENDAR_H

#include <stddef.h>
#include <time.h>

typedef struct lp_date { int year, month, day; } lp_date;   /* month 1 … 12 */

int lp_date_is_leap(int year);
int lp_date_days_in_month(int year, int month);
int lp_date_weekday(lp_date d);                 /* 0 Sunday … 6 Saturday */
long lp_date_to_days(lp_date d);                /* days since 1970-01-01, proleptic Gregorian */
lp_date lp_date_from_days(long days);
lp_date lp_date_add_days(lp_date d, int days);
lp_date lp_date_add_months(lp_date d, int months);   /* the day clamps: Jan 31 + 1 month is the end of February */
int lp_date_compare(lp_date a, lp_date b);
lp_date lp_date_of(time_t t);                   /* the local date t falls on */
lp_date lp_date_today(void);
/* Local time on date d at minutes after midnight, the clock as it reads that day (DST-safe). */
time_t lp_date_at(lp_date d, int minutes);
/* "2026-09-12" or "2026-9-12": 1 and *out, else 0. */
int lp_date_parse(const char *text, lp_date *out);
/* "9:05", "09:05", "21:30": minutes after midnight, else -1. */
int lp_time_parse(const char *text);
/* The locale's first day of the week: 0 Sunday, 1 Monday. */
int lp_calendar_week_start(void);
/* The 42 days — six weeks from week_start — a month is drawn on. */
void lp_calendar_month_grid(int year, int month, int week_start, lp_date cells[42]);

enum lp_repeat { LP_REPEAT_NONE, LP_REPEAT_DAILY, LP_REPEAT_WEEKLY, LP_REPEAT_MONTHLY, LP_REPEAT_YEARLY };

typedef struct lp_event {
    char uid[96];
    char title[128];
    char location[128];
    char notes[512];
    time_t start, end;          /* timed: the moments; all-day: local midnights, the end exclusive */
    int all_day;
    enum lp_repeat repeat;      /* the rule's frequency, as the editor offers it */
    char rrule[256];            /* the whole rule ("FREQ=WEEKLY;COUNT=10"); "" when it does not repeat */
    char file[512];             /* the .ics it came from */
    int read_only;              /* a file this did not write: shown, never rewritten */
} lp_event;

typedef struct lp_occurrence { int event; time_t start, end; } lp_occurrence;   /* event indexes lp_calendar.events */

typedef struct lp_calendar {
    lp_event *events;
    int count, cap;
    char dir[512];
} lp_calendar;

int lp_calendar_available(void);
/* $XDG_DATA_HOME/maryui/calendar, else ~/.local/share/maryui/calendar. */
void lp_calendar_default_dir(char *out, size_t n);
/* Reads every .ics in dir, making dir when missing, replacing what cal held. 0 or -errno; -ENOTSUP without libical. */
int lp_calendar_load(lp_calendar *cal, const char *dir);
void lp_calendar_free(lp_calendar *cal);
/* The occurrences overlapping [from, to), repeats expanded, by start (all-day first on a tie). The count; free *out. */
int lp_calendar_occurrences(const lp_calendar *cal, time_t from, time_t to, lp_occurrence **out);
/* The rule the editor writes for a frequency; "" for none. */
void lp_calendar_repeat_rule(enum lp_repeat repeat, char *out, size_t n);
/* Writes event to dir/<uid>.ics (a new uid when it has none), then reloads. 0 or -errno: -EROFS for a read-only one. */
int lp_calendar_save(lp_calendar *cal, lp_event *event);
int lp_calendar_delete(lp_calendar *cal, const char *uid);

#endif
