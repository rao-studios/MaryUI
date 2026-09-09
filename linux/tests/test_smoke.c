#include "lp_test.h"
#include "maryui/maryui.h"

LP_TEST(reports_a_version) {
    LP_ASSERT(lp_version() != NULL);
    LP_ASSERT(strchr(lp_version(), '.') != NULL);
}

LP_TEST(value_types_are_plain) {
    lp_color c = LP_RGBA(0.5f, 0.25f, 1.0f, 1.0f);
    lp_rect r = LP_RECT(1, 2, 3, 4);
    LP_ASSERT_NEAR(c.g, 0.25, 1e-6);
    LP_ASSERT_NEAR(r.w + r.h, 7, 1e-6);
}

int main(void) {
    LP_RUN(reports_a_version);
    LP_RUN(value_types_are_plain);
    LP_TEST_MAIN_END();
}
