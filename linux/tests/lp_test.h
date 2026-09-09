/* A twenty-line test harness. Each tests/test_<x>.c defines cases with
 * LP_TEST(name) and runs them from main() with LP_RUN(name); the exit status is
 * the number of failed cases. Case names mirror the vitest `it()` names in
 * ../web so `make test` output can be diffed against `npm test`. */
#ifndef LP_TEST_H
#define LP_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lp_test_failures = 0;
static int lp_test_case_failed = 0;

#define LP_TEST(name) static void name(void)
#define LP_RUN(name) do { lp_test_case_failed = 0; name(); \
    printf("%s %s\n", lp_test_case_failed ? "FAIL" : "ok  ", #name); \
    if (lp_test_case_failed) lp_test_failures++; } while (0)
#define LP_FAIL(fmt, ...) do { lp_test_case_failed = 1; \
    fprintf(stderr, "  %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while (0)
#define LP_ASSERT(cond) do { if (!(cond)) LP_FAIL("assertion failed: %s", #cond); } while (0)
#define LP_ASSERT_EQ(a, b) do { if ((a) != (b)) LP_FAIL("%s != %s", #a, #b); } while (0)
#define LP_ASSERT_STR(a, b) do { if (strcmp((a), (b)) != 0) LP_FAIL("%s = \"%s\", expected \"%s\"", #a, (a), (b)); } while (0)
#define LP_ASSERT_NEAR(a, b, eps) do { double _a = (a), _b = (b); \
    if (fabs(_a - _b) > (eps)) LP_FAIL("%s = %g, expected %g (±%g)", #a, _a, _b, (double)(eps)); } while (0)
#define LP_TEST_MAIN_END() return lp_test_failures

#endif
