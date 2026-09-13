/* Calendar's dates and events (lp_calendar.h). */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#ifdef __GLIBC__
#include <langinfo.h>
#endif

#include "maryui/lp_calendar.h"
#include "maryui/lp_files.h"

/* MARK: - Dates */

int lp_date_is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int lp_date_days_in_month(int y, int m) {
    static const int DAYS[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12) return 0;
    return m == 2 && lp_date_is_leap(y) ? 29 : DAYS[m - 1];
}

/* Howard Hinnant's days_from_civil and civil_from_days: exact for every Gregorian date. */
long lp_date_to_days(lp_date d) {
    long y = d.year - (d.month <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (d.month + (d.month > 2 ? -3 : 9)) + 2) / 5 + d.day - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

lp_date lp_date_from_days(long z) {
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    long day = doy - (153 * mp + 2) / 5 + 1;
    long month = mp + (mp < 10 ? 3 : -9);
    return (lp_date){ (int)(yoe + era * 400 + (month <= 2)), (int)month, (int)day };
}

int lp_date_weekday(lp_date d) { return (int)(((lp_date_to_days(d) % 7) + 7 + 4) % 7); }   /* 1970-01-01 was a Thursday */
lp_date lp_date_add_days(lp_date d, int days) { return lp_date_from_days(lp_date_to_days(d) + days); }

lp_date lp_date_add_months(lp_date d, int months) {
    long total = (long)d.year * 12 + (d.month - 1) + months;
    long year = total >= 0 ? total / 12 : (total - 11) / 12;
    int month = (int)(total - year * 12) + 1;
    int last = lp_date_days_in_month((int)year, month);
    return (lp_date){ (int)year, month, d.day > last ? last : d.day };
}

int lp_date_compare(lp_date a, lp_date b) {
    long x = lp_date_to_days(a), y = lp_date_to_days(b);
    return x < y ? -1 : x > y;
}

lp_date lp_date_of(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return (lp_date){ tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday };
}

lp_date lp_date_today(void) { return lp_date_of(time(NULL)); }

time_t lp_date_at(lp_date d, int minutes) {
    struct tm tm = { 0 };
    tm.tm_year = d.year - 1900;
    tm.tm_mon = d.month - 1;
    tm.tm_mday = d.day;
    tm.tm_hour = minutes / 60;
    tm.tm_min = minutes % 60;
    tm.tm_isdst = -1;   /* the clock as it reads that day, whichever side of a change */
    return mktime(&tm);
}

int lp_date_parse(const char *text, lp_date *out) {
    int y, m, d;
    char tail;
    if (!text || sscanf(text, " %d-%d-%d %c", &y, &m, &d, &tail) != 3) return 0;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > lp_date_days_in_month(y, m)) return 0;
    *out = (lp_date){ y, m, d };
    return 1;
}

int lp_time_parse(const char *text) {
    int h, m;
    char tail;
    if (!text || sscanf(text, " %d:%d %c", &h, &m, &tail) != 2) return -1;
    if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

int lp_calendar_week_start(void) {
#if defined(__GLIBC__) && defined(_NL_TIME_FIRST_WEEKDAY) && defined(_NL_TIME_WEEK_1STDAY)
    /* glibc: the week is counted from WEEK_1STDAY (a yyyymmdd), and FIRST_WEEKDAY (1-based) steps from there */
    long ref = (long)(intptr_t)nl_langinfo(_NL_TIME_WEEK_1STDAY);
    int first = (unsigned char)nl_langinfo(_NL_TIME_FIRST_WEEKDAY)[0];
    lp_date base = { (int)(ref / 10000), (int)(ref / 100 % 100), (int)(ref % 100) };
    if (base.month < 1 || base.month > 12 || first < 1 || first > 7) return 0;
    return (lp_date_weekday(base) + first - 1) % 7;
#else
    return 0;
#endif
}

void lp_calendar_month_grid(int year, int month, int week_start, lp_date cells[42]) {
    lp_date first = { year, month, 1 };
    int back = (lp_date_weekday(first) - week_start + 7) % 7;
    lp_date start = lp_date_add_days(first, -back);
    for (int i = 0; i < 42; i++) cells[i] = lp_date_add_days(start, i);
}

void lp_calendar_default_dir(char *out, size_t n) {
    const char *data = getenv("XDG_DATA_HOME");
    if (data && *data) snprintf(out, n, "%s/maryui/calendar", data);
    else snprintf(out, n, "%s/.local/share/maryui/calendar", lp_files_home());
}

void lp_calendar_repeat_rule(enum lp_repeat repeat, char *out, size_t n) {
    static const char *const RULES[] = { "", "FREQ=DAILY", "FREQ=WEEKLY", "FREQ=MONTHLY", "FREQ=YEARLY" };
    snprintf(out, n, "%s", repeat >= LP_REPEAT_NONE && repeat <= LP_REPEAT_YEARLY ? RULES[repeat] : "");
}

void lp_calendar_free(lp_calendar *cal) {
    free(cal->events);
    cal->events = NULL;
    cal->count = cal->cap = 0;
}

#ifndef HAVE_ICAL

int lp_calendar_available(void) { return 0; }
int lp_calendar_load(lp_calendar *cal, const char *dir) {
    lp_calendar_free(cal);
    snprintf(cal->dir, sizeof cal->dir, "%s", dir);
    return -ENOTSUP;
}
int lp_calendar_occurrences(const lp_calendar *cal, time_t from, time_t to, lp_occurrence **out) { *out = NULL; return 0; }
int lp_calendar_save(lp_calendar *cal, lp_event *event) { return -ENOTSUP; }
int lp_calendar_delete(lp_calendar *cal, const char *uid) { return -ENOTSUP; }

#else

#include <libical/ical.h>

int lp_calendar_available(void) { return 1; }

static int by_start(const void *a, const void *b) {
    const lp_occurrence *x = a, *y = b;
    if (x->start != y->start) return x->start < y->start ? -1 : 1;
    time_t lx = x->end - x->start, ly = y->end - y->start;
    return lx == ly ? x->event - y->event : lx > ly ? -1 : 1;   /* longer (all-day) first on a tie */
}

static time_t time_of(struct icaltimetype t) {
    if (icaltime_is_null_time(t)) return 0;
    if (t.is_date) return lp_date_at((lp_date){ t.year, t.month, t.day }, 0);
    if (icaltime_is_utc(t)) return icaltime_as_timet_with_zone(t, icaltimezone_get_utc_timezone());
    if (t.zone) return icaltime_as_timet_with_zone(t, t.zone);
    return lp_date_at((lp_date){ t.year, t.month, t.day }, t.hour * 60 + t.minute) + t.second;   /* floating: the wall clock here */
}

static struct icaltimetype floating(time_t t, int is_date) {
    struct tm tm;
    localtime_r(&t, &tm);
    struct icaltimetype it = icaltime_null_time();
    it.year = tm.tm_year + 1900;
    it.month = tm.tm_mon + 1;
    it.day = tm.tm_mday;
    if (is_date) it.is_date = 1;
    else { it.hour = tm.tm_hour; it.minute = tm.tm_min; it.second = tm.tm_sec; }
    return it;
}

static enum lp_repeat repeat_of(icalrecurrencetype_frequency freq) {
    switch (freq) {
    case ICAL_DAILY_RECURRENCE: return LP_REPEAT_DAILY;
    case ICAL_WEEKLY_RECURRENCE: return LP_REPEAT_WEEKLY;
    case ICAL_MONTHLY_RECURRENCE: return LP_REPEAT_MONTHLY;
    case ICAL_YEARLY_RECURRENCE: return LP_REPEAT_YEARLY;
    default: return LP_REPEAT_NONE;
    }
}

static void copy_text(char *out, size_t n, const char *text) { snprintf(out, n, "%s", text ? text : ""); }

static void read_event(lp_calendar *cal, icalcomponent *c, const char *path) {
    if (cal->count == cal->cap) {
        int cap = cal->cap ? cal->cap * 2 : 32;
        lp_event *grown = realloc(cal->events, sizeof *grown * (size_t)cap);
        if (!grown) return;
        cal->events = grown;
        cal->cap = cap;
    }
    lp_event *e = &cal->events[cal->count];
    memset(e, 0, sizeof *e);
    copy_text(e->uid, sizeof e->uid, icalcomponent_get_uid(c));
    copy_text(e->title, sizeof e->title, icalcomponent_get_summary(c));
    copy_text(e->notes, sizeof e->notes, icalcomponent_get_description(c));
    icalproperty *location = icalcomponent_get_first_property(c, ICAL_LOCATION_PROPERTY);
    if (location) copy_text(e->location, sizeof e->location, icalproperty_get_location(location));
    struct icaltimetype start = icalcomponent_get_dtstart(c), end = icalcomponent_get_dtend(c);
    e->all_day = start.is_date;
    e->start = time_of(start);
    if (!icaltime_is_null_time(end)) e->end = time_of(end);
    else if (e->all_day) e->end = lp_date_at(lp_date_add_days((lp_date){ start.year, start.month, start.day }, 1), 0);
    else e->end = e->start;
    icalproperty *rule = icalcomponent_get_first_property(c, ICAL_RRULE_PROPERTY);
    if (rule) {
        struct icalrecurrencetype r = icalproperty_get_rrule(rule);
        copy_text(e->rrule, sizeof e->rrule, icalrecurrencetype_as_string(&r));
        e->repeat = repeat_of(r.freq);
    }
    snprintf(e->file, sizeof e->file, "%s", path);
    char own[160];
    snprintf(own, sizeof own, "%s.ics", e->uid);
    for (char *p = own; *p; p++) if (*p == '/') *p = '-';
    e->read_only = strcmp(lp_files_basename(path), own) != 0;
    if (e->uid[0] && e->start) cal->count++;
}

int lp_calendar_load(lp_calendar *cal, const char *dir) {
    lp_calendar_free(cal);
    snprintf(cal->dir, sizeof cal->dir, "%s", dir);
    lp_files_mkdir_p(dir);
    DIR *d = opendir(dir);
    if (!d) return -errno;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        size_t len = strlen(entry->d_name);
        if (len < 5 || strcasecmp(entry->d_name + len - 4, ".ics") != 0) continue;
        char path[1024];
        lp_files_join(dir, entry->d_name, path, sizeof path);
        char *text = NULL;
        size_t size = 0;
        if (lp_files_read(path, &text, &size) != 0) continue;
        icalcomponent *root = icalcomponent_new_from_string(text);
        free(text);
        if (!root) continue;
        if (icalcomponent_isa(root) == ICAL_VEVENT_COMPONENT) {
            read_event(cal, root, path);
        } else {
            for (icalcomponent *c = icalcomponent_get_first_component(root, ICAL_VEVENT_COMPONENT); c;
                 c = icalcomponent_get_next_component(root, ICAL_VEVENT_COMPONENT)) read_event(cal, c, path);
        }
        icalcomponent_free(root);
    }
    closedir(d);
    return 0;
}

static int push(lp_occurrence **list, int *count, int *cap, int event, time_t start, time_t end) {
    if (*count == *cap) {
        int grown_cap = *cap ? *cap * 2 : 64;
        lp_occurrence *grown = realloc(*list, sizeof *grown * (size_t)grown_cap);
        if (!grown) return 0;
        *list = grown;
        *cap = grown_cap;
    }
    (*list)[(*count)++] = (lp_occurrence){ event, start, end };
    return 1;
}

static int overlaps(time_t start, time_t end, time_t from, time_t to) {
    return start < to && (end > from || (end == start && start >= from));
}

int lp_calendar_occurrences(const lp_calendar *cal, time_t from, time_t to, lp_occurrence **out) {
    lp_occurrence *list = NULL;
    int count = 0, cap = 0;
    for (int i = 0; i < cal->count; i++) {
        const lp_event *e = &cal->events[i];
        if (!e->rrule[0]) {
            if (overlaps(e->start, e->end, from, to)) push(&list, &count, &cap, i, e->start, e->end);
            continue;
        }
        long span_days = lround((double)(e->end - e->start) / 86400.0);
        time_t length = e->end - e->start;
        struct icalrecurrencetype rule = icalrecurrencetype_from_string(e->rrule);
        icalrecur_iterator *it = icalrecur_iterator_new(rule, floating(e->start, e->all_day));
        if (!it) continue;
        for (int n = 0; n < 20000; n++) {
            struct icaltimetype next = icalrecur_iterator_next(it);
            if (icaltime_is_null_time(next)) break;
            time_t start = time_of(next);
            if (start >= to) break;
            /* an all-day occurrence ends at a midnight, which a clock change moves: count days, not seconds */
            time_t end = e->all_day ? lp_date_at(lp_date_add_days(lp_date_of(start), (int)span_days), 0) : start + length;
            if (overlaps(start, end, from, to) && !push(&list, &count, &cap, i, start, end)) break;
        }
        icalrecur_iterator_free(it);
    }
    if (count > 1) qsort(list, (size_t)count, sizeof *list, by_start);
    *out = list;
    return count;
}

static void new_uid(char *out, size_t n) {
    unsigned char bytes[8] = { 0 };
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) { ssize_t got = read(fd, bytes, sizeof bytes); (void)got; close(fd); }
    snprintf(out, n, "%lx-%02x%02x%02x%02x%02x%02x%02x%02x@maryos", (unsigned long)time(NULL),
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
}

static void own_path(const lp_calendar *cal, const char *uid, char *out, size_t n) {
    char name[160];
    snprintf(name, sizeof name, "%s.ics", uid);
    for (char *p = name; *p; p++) if (*p == '/') *p = '-';
    lp_files_join(cal->dir, name, out, n);
}

int lp_calendar_save(lp_calendar *cal, lp_event *event) {
    if (event->read_only) return -EROFS;
    if (!event->uid[0]) new_uid(event->uid, sizeof event->uid);
    icalcomponent *vevent = icalcomponent_new(ICAL_VEVENT_COMPONENT);
    icalcomponent_set_uid(vevent, event->uid);
    icalcomponent_set_summary(vevent, event->title);
    if (event->location[0]) icalcomponent_add_property(vevent, icalproperty_new_location(event->location));
    if (event->notes[0]) icalcomponent_set_description(vevent, event->notes);
    icalcomponent_set_dtstamp(vevent, icaltime_current_time_with_zone(icaltimezone_get_utc_timezone()));
    icalcomponent_set_dtstart(vevent, floating(event->start, event->all_day));
    icalcomponent_set_dtend(vevent, floating(event->end, event->all_day));
    if (event->rrule[0]) icalcomponent_add_property(vevent, icalproperty_new_rrule(icalrecurrencetype_from_string(event->rrule)));
    icalcomponent *root = icalcomponent_vanew(ICAL_VCALENDAR_COMPONENT,
        icalproperty_new_version("2.0"), icalproperty_new_prodid("-//MaryOS//Calendar//EN"), (void *)0);
    icalcomponent_add_component(root, vevent);
    char path[1024];
    own_path(cal, event->uid, path, sizeof path);
    lp_files_mkdir_p(cal->dir);
    char *text = icalcomponent_as_ical_string(root);
    int rc = lp_files_write(path, text, strlen(text));
    icalcomponent_free(root);
    if (rc) return rc;
    char dir[sizeof cal->dir];
    snprintf(dir, sizeof dir, "%s", cal->dir);
    lp_calendar_load(cal, dir);
    return 0;
}

int lp_calendar_delete(lp_calendar *cal, const char *uid) {
    for (int i = 0; i < cal->count; i++) {
        if (strcmp(cal->events[i].uid, uid) != 0) continue;
        if (cal->events[i].read_only) return -EROFS;
        if (unlink(cal->events[i].file) != 0) return -errno;
        char dir[sizeof cal->dir];
        snprintf(dir, sizeof dir, "%s", cal->dir);
        lp_calendar_load(cal, dir);
        return 0;
    }
    return -ENOENT;
}

#endif
