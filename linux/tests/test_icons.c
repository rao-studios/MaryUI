/* lp_icons.h and the path parser that draws it. */
#include <cairo.h>
#include "lp_test.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_svgpath.h"

LP_TEST(has_every_icon_from_icons_json) {
    LP_ASSERT_EQ(LP_ICON_COUNT, 27);
    LP_ASSERT_EQ(lp_icon_by_name("trash"), LP_ICON_TRASH);
    LP_ASSERT_STR(LP_ICON_NAMES[LP_ICON_FOLDER], "folder");
    LP_ASSERT_STR(LP_ICON_NAMES[LP_ICON_CHEVRON_LEFT], "chevronLeft");
    LP_ASSERT_EQ(lp_icon_by_name("drop"), LP_ICON_DROP);
    LP_ASSERT_EQ(lp_icon_by_name("nope"), LP_ICON_COUNT);
}

LP_TEST(parses_every_icon_path) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 24, 24);
    cairo_t *cr = cairo_create(s);
    for (int i = 0; i < LP_ICON_COUNT; i++) {
        for (int p = 0; LP_ICON_PATHS[i][p]; p++) {
            cairo_new_path(cr);
            int n = lp_svgpath_apply(cr, LP_ICON_PATHS[i][p]);
            if (n <= 0) LP_FAIL("%s path %d did not parse", LP_ICON_NAMES[i], p);
        }
    }
    LP_ASSERT_EQ(lp_svgpath_apply(cr, "M1 1 L2"), -1);
    LP_ASSERT_EQ(lp_svgpath_apply(cr, "12 12"), -1);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

LP_TEST(draws_ink_inside_the_box) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 32, 32);
    cairo_t *cr = cairo_create(s);
    lp_icon_draw(cr, LP_ICON_SEARCH, 4, 4, 24, 0, LP_RGBA(0, 0, 0, 1));
    cairo_destroy(cr);
    cairo_surface_flush(s);
    const unsigned char *d = cairo_image_surface_get_data(s);
    int stride = cairo_image_surface_get_stride(s), inked = 0, outside = 0;
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        int a = d[y * stride + x * 4 + 3];
        if (a > 64) { inked++; if (x < 3 || y < 3 || x > 28 || y > 28) outside++; }
    }
    LP_ASSERT(inked > 40);
    LP_ASSERT_EQ(outside, 0);
    cairo_surface_destroy(s);
}

int main(void) {
    LP_RUN(has_every_icon_from_icons_json);
    LP_RUN(parses_every_icon_path);
    LP_RUN(draws_ink_inside_the_box);
    LP_TEST_MAIN_END();
}
