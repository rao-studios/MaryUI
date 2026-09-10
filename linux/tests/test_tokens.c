/* The generated lp_tokens.h: a few values pinned against tokens.json. */
#include "lp_test.h"
#include "maryui/lp_tokens.h"

LP_TEST(has_every_token_in_the_table) {
    LP_ASSERT(LP_TOKEN_COUNT > 100);
    LP_ASSERT_EQ((int)(sizeof(LP_TOKENS) / sizeof(LP_TOKENS[0])), LP_TOKEN_COUNT);
    LP_ASSERT_STR(LP_TOKENS[0].name, "platinum.0");
    LP_ASSERT_STR(LP_TOKENS[0].css_name, "--lp-platinum-0");
    LP_ASSERT_STR(LP_TOKENS[0].value, "#f7f7f9");
}

LP_TEST(colors_dimensions_and_springs_carry_their_values) {
    lp_color p0 = LP_PLATINUM_0;
    LP_ASSERT_NEAR(p0.r, 0xf7 / 255.0, 0.001);
    LP_ASSERT_NEAR(p0.b, 0xf9 / 255.0, 0.001);
    LP_ASSERT_NEAR(LP_RADIUS_WINDOW, 12, 0);
    LP_ASSERT_NEAR(LP_SIZE_MENUBAR_HEIGHT, 24, 0);
    LP_ASSERT_NEAR(LP_SIZE_TITLEBAR_HEIGHT, 42, 0);
    LP_ASSERT_NEAR(LP_MOTION_FAST_MS, 120, 0);
    lp_spring_params jelly = LP_MOTION_SPRING_JELLY;
    LP_ASSERT_NEAR(jelly.frequency, 2.2, 1e-6);
    LP_ASSERT_NEAR(jelly.damping, 0.55, 1e-6);
    LP_ASSERT_EQ(LP_SHADOW_WINDOW_COUNT, 2);
    LP_ASSERT_EQ(LP_SHADOW_EMBOSS_RAISED[0].inset, 1);
    LP_ASSERT_NEAR(LP_SHADOW_EMBOSS_RAISED[1].y, -1, 0);
    LP_ASSERT_EQ(LP_Z_MENUS, 700);
}

LP_TEST(font_stacks_end_in_the_linux_families) {
    LP_ASSERT(strstr(LP_FONT_UI_PANGO, "Inter") != NULL);
    LP_ASSERT(strstr(LP_FONT_DISPLAY_PANGO, "P052") != NULL);
    LP_ASSERT(strstr(LP_FONT_MONO_PANGO, "JetBrains Mono") != NULL);
    int n = 0;
    while (LP_FONT_UI_FAMILIES[n]) n++;
    LP_ASSERT_EQ(n, 8);
}

int main(void) {
    LP_RUN(has_every_token_in_the_table);
    LP_RUN(colors_dimensions_and_springs_carry_their_values);
    LP_RUN(font_stacks_end_in_the_linux_families);
    LP_TEST_MAIN_END();
}
