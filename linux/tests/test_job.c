/* Commands run for apps: the exit status and the output come back through the
 * poll loop, a signal death reads as minus the signal, a missing program as
 * 127, a host with no event loop runs the job through, a cancelled job still
 * finishes quietly, and a flood of output never blocks the program. Runs on a
 * Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "lp_test.h"
#include "lp_test_loop.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_job.h"

static lp_desktop d;
static int calls, last_status;
static char last_output[256];
static size_t last_length;

static void done(int status, const char *output, void *user) {
    calls++;
    last_status = status;
    last_length = strlen(output);
    snprintf(last_output, sizeof last_output, "%s", output);
}
static int finished(void) { return calls > 0; }

static void fresh(int with_loop) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    if (with_loop) lp_test_loop_install(&d);
    calls = 0;
    last_status = -99;
    last_output[0] = 0;
}

LP_TEST(reports_the_exit_status_and_the_output) {
    fresh(1);
    const char *const argv[] = { "/bin/sh", "-c", "echo out; echo err >&2; exit 3", NULL };
    LP_ASSERT(lp_job_run(&d, argv, done, NULL) != NULL);
    LP_ASSERT_EQ(calls, 0);                        /* nothing yet: it runs beside the desktop */
    lp_test_loop_run(5000, finished);
    LP_ASSERT_EQ(calls, 1);
    LP_ASSERT_EQ(last_status, 3);
    LP_ASSERT(strstr(last_output, "out\n") && strstr(last_output, "err\n"));
}

LP_TEST(a_signal_reads_as_minus_the_signal) {
    fresh(1);
    const char *const argv[] = { "/bin/sh", "-c", "kill -TERM $$", NULL };
    lp_job_run(&d, argv, done, NULL);
    lp_test_loop_run(5000, finished);
    LP_ASSERT_EQ(last_status, -SIGTERM);
}

LP_TEST(a_missing_program_is_127_and_says_why) {
    fresh(1);
    const char *const argv[] = { "/nonexistent/udisksctl", "mount", NULL };
    lp_job_run(&d, argv, done, NULL);
    lp_test_loop_run(5000, finished);
    LP_ASSERT_EQ(last_status, 127);
    LP_ASSERT(strstr(last_output, "cannot run /nonexistent/udisksctl") != NULL);
}

LP_TEST(without_an_event_loop_it_runs_through) {
    fresh(0);
    const char *const argv[] = { "/bin/sh", "-c", "printf done", NULL };
    LP_ASSERT(lp_job_run(&d, argv, done, NULL) == NULL);
    LP_ASSERT_EQ(calls, 1);
    LP_ASSERT_EQ(last_status, 0);
    LP_ASSERT_STR(last_output, "done");
    LP_ASSERT(lp_job_run(NULL, argv, done, NULL) == NULL);   /* nor any desktop at all */
    LP_ASSERT_EQ(calls, 2);
}

LP_TEST(a_cancelled_job_finishes_without_calling_back) {
    fresh(1);
    const char *const argv[] = { "/bin/sh", "-c", "sleep 0.2; echo late", NULL };
    lp_job *job = lp_job_run(&d, argv, done, NULL);
    LP_ASSERT(job != NULL);
    lp_job_cancel(job);
    lp_test_loop_run(1500, NULL);
    LP_ASSERT_EQ(calls, 0);
}

LP_TEST(a_flood_of_output_never_blocks_the_program) {
    fresh(1);
    const char *const argv[] = { "/bin/sh", "-c", "i=0; while [ $i -lt 3000 ]; do echo 0123456789012345678901234567890123456789; i=$((i+1)); done; exit 0", NULL };
    lp_job_run(&d, argv, done, NULL);
    lp_test_loop_run(20000, finished);
    LP_ASSERT_EQ(calls, 1);
    LP_ASSERT_EQ(last_status, 0);
    LP_ASSERT_EQ(last_length, LP_JOB_OUTPUT_MAX - 1);          /* 123 KB written, the first 64 kept */
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    char config[256];
    const char *tmp = getenv("TMPDIR");
    snprintf(config, sizeof config, "%s/lp_job_config", tmp && *tmp ? tmp : "/tmp");
    setenv("XDG_CONFIG_HOME", config, 1);
    LP_RUN(reports_the_exit_status_and_the_output);
    LP_RUN(a_signal_reads_as_minus_the_signal);
    LP_RUN(a_missing_program_is_127_and_says_why);
    LP_RUN(without_an_event_loop_it_runs_through);
    LP_RUN(a_cancelled_job_finishes_without_calling_back);
    LP_RUN(a_flood_of_output_never_blocks_the_program);
    LP_TEST_MAIN_END();
}
