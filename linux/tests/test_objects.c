/* lp_objects.h, the tier thresholds, and the renderer that draws it. The
 * geometry checks mirror web/src/components/ObjectIcon/objects.test.ts. */
#include <cairo.h>
#include <string.h>

#include "lp_test.h"
#include "maryui/lp_object_icon.h"
#include "maryui/lp_objects.h"
#include "maryui/lp_svgpath.h"
#include "maryui/lp_tokens.h"

LP_TEST(has_every_object_from_objects_json) {
    LP_ASSERT_EQ(LP_OBJ_COUNT, 69);
    LP_ASSERT_EQ(lp_object_by_name("folder"), LP_OBJ_FOLDER);
    LP_ASSERT_STR(LP_OBJ_NAMES[LP_OBJ_FOLDER], "folder");
    /* The camel-case naming rule, and a mark that exists only as an object. */
    LP_ASSERT_STR(LP_OBJ_NAMES[LP_OBJ_APP_FINDER], "appFinder");
    LP_ASSERT_EQ(lp_object_by_name("appFinder"), LP_OBJ_APP_FINDER);
    LP_ASSERT_EQ(lp_object_by_name("nope"), LP_OBJ_COUNT);
    LP_ASSERT_EQ(LP_OBJ_GRID, 32);
}

LP_TEST(every_object_names_a_silhouette_among_its_parts) {
    for (int i = 0; i < LP_OBJ_COUNT; i++) {
        const lp_obj_def *def = &LP_OBJECTS[i];
        LP_ASSERT(def->part_count > 0);
        int found = 0;
        for (int p = 0; p < def->part_count; p++)
            if (strcmp(def->parts[p].id, def->silhouette) == 0) found = 1;
        if (!found) LP_FAIL("%s: no part named %s", LP_OBJ_NAMES[i], def->silhouette);
        if (!def->glyph || !def->glyph[0]) LP_FAIL("%s: no glyph to fall back to", LP_OBJ_NAMES[i]);
    }
}

LP_TEST(parses_every_object_path) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8);
    cairo_t *cr = cairo_create(s);
    for (int i = 0; i < LP_OBJ_COUNT; i++) {
        for (int p = 0; p < LP_OBJECTS[i].part_count; p++) {
            cairo_new_path(cr);
            if (lp_svgpath_apply(cr, LP_OBJECTS[i].parts[p].d) <= 0)
                LP_FAIL("%s part %s did not parse", LP_OBJ_NAMES[i], LP_OBJECTS[i].parts[p].id);
        }
    }
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

LP_TEST(tiers_follow_the_logical_size_alone) {
    LP_ASSERT_EQ(lp_icon_tier(14), LP_TIER_GLYPH);
    LP_ASSERT_EQ(lp_icon_tier(LP_OBJECT_TIER_GLYPH_MAX), LP_TIER_GLYPH);
    LP_ASSERT_EQ(lp_icon_tier(LP_OBJECT_TIER_GLYPH_MAX + 1), LP_TIER_PLAIN);
    LP_ASSERT_EQ(lp_icon_tier(LP_OBJECT_TIER_PLAIN_MAX), LP_TIER_PLAIN);
    LP_ASSERT_EQ(lp_icon_tier(LP_OBJECT_TIER_PLAIN_MAX + 1), LP_TIER_LIT);
    LP_ASSERT_EQ(lp_icon_tier(52), LP_TIER_LIT);
}

/* The object's ink stays in its box, like test_icons.c's check for the glyphs. */
LP_TEST(draws_ink_inside_the_box) {
    const int side = 64, margin = 6, size = 52;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, side, side);
    cairo_t *cr = cairo_create(s);
    lp_object_icon_draw(cr, LP_OBJ_FOLDER, LP_RECT(margin, margin, size, size), NULL);
    cairo_destroy(cr);
    cairo_surface_flush(s);
    const unsigned char *d = cairo_image_surface_get_data(s);
    int stride = cairo_image_surface_get_stride(s), inked = 0, outside = 0;
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            int a = d[y * stride + x * 4 + 3];
            /* The contact shadow is soft and reaches past the body, so the ink
             * test is the opaque part; the box gets one pixel of slack. */
            if (a > 200) {
                inked++;
                if (x < margin - 1 || y < margin - 1 || x > margin + size + 1 || y > margin + size + 1) outside++;
            }
        }
    }
    LP_ASSERT(inked > 400);
    LP_ASSERT_EQ(outside, 0);
    cairo_surface_destroy(s);
}

/* Below the glyph threshold the object is not drawn at all: the fallback is. */
LP_TEST(falls_back_to_the_glyph_when_small) {
    const int side = 24;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, side, side);
    cairo_t *cr = cairo_create(s);
    lp_object_icon_draw(cr, LP_OBJ_FOLDER, LP_RECT(2, 2, 16, 16), NULL);
    cairo_destroy(cr);
    cairo_surface_flush(s);
    const unsigned char *d = cairo_image_surface_get_data(s);
    int stride = cairo_image_surface_get_stride(s), inked = 0, opaque = 0;
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            int a = d[y * stride + x * 4 + 3];
            if (a > 64) inked++;
            if (a > 250) opaque++;
        }
    }
    /* A stroked outline: plenty of ink, but nothing like a filled body. */
    LP_ASSERT(inked > 20);
    LP_ASSERT(opaque < inked / 2);
    cairo_surface_destroy(s);
}

int main(void) {
    LP_RUN(has_every_object_from_objects_json);
    LP_RUN(every_object_names_a_silhouette_among_its_parts);
    LP_RUN(parses_every_object_path);
    LP_RUN(tiers_follow_the_logical_size_alone);
    LP_RUN(draws_ink_inside_the_box);
    LP_RUN(falls_back_to_the_glyph_when_small);
    LP_TEST_MAIN_END();
}
