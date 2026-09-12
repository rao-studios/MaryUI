/* The event sources apps ask the host for (lp_desktop.add_fd / add_timer),
 * through the NULL-safe wrappers and the poll loop in lp_test_loop.h. */
#include <unistd.h>
#include "lp_test.h"
#include "lp_test_loop.h"
#include "maryui/lp_desktop.h"

static lp_desktop d;
static int reads, ticks;
static char got[32];

static int on_readable(int fd, uint32_t mask, void *data) {
    LP_ASSERT(mask & LP_SOURCE_READABLE);
    ssize_t n = read(fd, got, sizeof got - 1);
    got[n > 0 ? n : 0] = 0;
    reads++;
    return 0;
}
static int on_tick(int fd, uint32_t mask, void *data) {
    LP_ASSERT_EQ(fd, -1);
    ticks++;
    return 0;
}
static int have_read(void) { return reads > 0; }
static int have_ticked(void) { return ticks > 0; }

LP_TEST(a_host_without_sources_hands_back_null) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    LP_ASSERT(lp_desktop_add_fd(&d, 0, LP_SOURCE_READABLE, on_readable, NULL) == NULL);
    LP_ASSERT(lp_desktop_add_timer(&d, 10, on_tick, NULL) == NULL);
    lp_desktop_update_timer(&d, NULL, 10);
    lp_desktop_remove_source(&d, NULL);
}

LP_TEST(an_fd_source_fires_when_readable_and_stops_once_removed) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&d);
    reads = 0;
    int p[2];
    LP_ASSERT_EQ(pipe(p), 0);
    lp_source *s = lp_desktop_add_fd(&d, p[0], LP_SOURCE_READABLE, on_readable, NULL);
    LP_ASSERT(s != NULL);
    lp_test_loop_run(20, have_read);
    LP_ASSERT_EQ(reads, 0);
    LP_ASSERT_EQ(write(p[1], "hello", 5), 5);
    lp_test_loop_run(1000, have_read);
    LP_ASSERT_EQ(reads, 1);
    LP_ASSERT_STR(got, "hello");
    lp_desktop_remove_source(&d, s);
    LP_ASSERT_EQ(write(p[1], "again", 5), 5);
    lp_test_loop_run(20, NULL);
    LP_ASSERT_EQ(reads, 1);
    close(p[0]);
    close(p[1]);
}

LP_TEST(a_timer_fires_once_per_arming_and_zero_disarms) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&d);
    ticks = 0;
    lp_source *t = lp_desktop_add_timer(&d, 5, on_tick, NULL);
    LP_ASSERT(t != NULL);
    lp_test_loop_run(1000, have_ticked);
    LP_ASSERT_EQ(ticks, 1);
    lp_test_loop_run(30, NULL);
    LP_ASSERT_EQ(ticks, 1);                 /* one-shot */
    lp_desktop_update_timer(&d, t, 5);
    lp_desktop_update_timer(&d, t, 0);      /* disarmed before it was due */
    lp_test_loop_run(30, NULL);
    LP_ASSERT_EQ(ticks, 1);
    ticks = 0;
    lp_desktop_update_timer(&d, t, 5);
    lp_test_loop_run(1000, have_ticked);
    LP_ASSERT_EQ(ticks, 1);
    lp_desktop_remove_source(&d, t);
}

int main(void) {
    LP_RUN(a_host_without_sources_hands_back_null);
    LP_RUN(an_fd_source_fires_when_readable_and_stops_once_removed);
    LP_RUN(a_timer_fires_once_per_arming_and_zero_disarms);
    LP_TEST_MAIN_END();
}
