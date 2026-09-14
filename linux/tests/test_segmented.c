/* The segmented control's motion: the thumb springs to a new selection over about motion.normal, the
 * hovered segment's bead eases in and out, both from the DRAW pass with frames asked for the track
 * alone; and the text painters skip what the damage clip cannot show. */
#include <cairo.h>

#include "lp_test.h"
#include "maryui/components/lp_controls.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static const lp_segment THREE[3] = { { "Drive", LP_ICON_COUNT }, { "Library", LP_ICON_COUNT }, { "Graph", LP_ICON_COUNT } };

static cairo_surface_t *surface;
static cairo_t *cr;
static lp_settings settings;

static void setup(void) {
    if (cr) cairo_destroy(cr);
    if (surface) cairo_surface_destroy(surface);
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 320, 60);
    cr = cairo_create(surface);
    settings = lp_settings_defaults();
}

static lp_input pointer(float x, float y) {
    lp_input in = { 0 };
    in.mx = x; in.my = y;
    return in;
}

/* One DRAW pass of the control at `now`; returns whether it asked for another frame. */
static int draw(lp_ctx *ctx, lp_id id, int *index, double now) {
    lp_ctx_begin(ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, 320, 60), now);
    lp_segmented(ctx, id, 10, 10, THREE, 3, index, LP_CONTROL_SM);
    int wants = ctx->wants_frame;
    lp_ctx_end(ctx);
    return wants;
}

static void event(lp_ctx *ctx, lp_id id, int *index, lp_input in, double now) {
    lp_ctx_begin(ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 320, 60), now);
    lp_segmented(ctx, id, 10, 10, THREE, 3, index, LP_CONTROL_SM);
    lp_ctx_end(ctx);
}

LP_TEST(a_new_selection_slides_the_thumb_over_motion_normal) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_id id = LP_ID("tabs");
    int index = 0;
    LP_ASSERT_EQ(draw(&ctx, id, &index, 1000), 0);            /* at rest on the first segment */
    LP_ASSERT_NEAR(lp_segmented_thumb(id), 0, 1e-6);
    index = 2;
    LP_ASSERT_EQ(draw(&ctx, id, &index, 1016), 1);            /* the slide begins: another frame, please */
    double now = 1016;
    int frames = 0;
    for (; frames < 200 && draw(&ctx, id, &index, now + 16); frames++) now += 16;
    float mid = lp_segmented_thumb(id);
    LP_ASSERT(frames > 3);                                     /* it took several frames … */
    LP_ASSERT(now - 1016 < 600);                               /* … and settled within a few hundred ms */
    LP_ASSERT_NEAR(mid, 2, 1e-6);
}

LP_TEST(midway_the_thumb_is_between_the_segments_and_the_frame_is_the_tracks) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_id id = LP_ID("hops");
    int index = 0;
    draw(&ctx, id, &index, 2000);
    index = 2;
    draw(&ctx, id, &index, 2016);
    draw(&ctx, id, &index, 2032);
    draw(&ctx, id, &index, 2048);
    float at = lp_segmented_thumb(id);
    LP_ASSERT(at > 0.01f && at < 1.99f);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, 320, 60), 2064);
    lp_segmented(&ctx, id, 10, 10, THREE, 3, &index, LP_CONTROL_SM);
    lp_size sz = lp_segmented_measure(&ctx, THREE, 3, LP_CONTROL_SM);
    LP_ASSERT(ctx.wants_frame);
    lp_rect r = ctx.wants_frame_rect;
    LP_ASSERT(r.x >= 0 && r.x <= 10 && r.w <= sz.w + 12 && r.h <= sz.h + 12);   /* the track, padded, not the chrome */
    lp_ctx_end(&ctx);
}

LP_TEST(without_a_clock_or_with_reduced_motion_the_thumb_is_simply_there) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_id id = LP_ID("render");
    int index = 1;
    LP_ASSERT_EQ(draw(&ctx, id, &index, 0), 0);
    index = 2;
    LP_ASSERT_EQ(draw(&ctx, id, &index, 0), 0);
    LP_ASSERT_NEAR(lp_segmented_thumb(id), 2, 1e-6);
    settings.reduced_motion = 1;
    lp_id quiet = LP_ID("quiet");
    index = 0;
    draw(&ctx, quiet, &index, 5000);
    index = 2;
    LP_ASSERT_EQ(draw(&ctx, quiet, &index, 5016), 0);
    LP_ASSERT_NEAR(lp_segmented_thumb(quiet), 2, 1e-6);
}

LP_TEST(hovering_a_segment_raises_the_bead_and_leaving_lets_it_fall) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_id id = LP_ID("bead");
    int index = 0;
    draw(&ctx, id, &index, 3000);
    lp_size sz = lp_segmented_measure(&ctx, THREE, 3, LP_CONTROL_SM);
    float seg = (sz.w - 2 * LP_SIZE_SEGMENTED_PAD) / 3;
    event(&ctx, id, &index, pointer(10 + LP_SIZE_SEGMENTED_PAD + seg * 1.5f, 10 + sz.h / 2), 3010);   /* over the second segment */
    LP_ASSERT(lp_is_hot(&ctx, lp_id_index(id, 1)));
    LP_ASSERT_EQ(draw(&ctx, id, &index, 3020), 1);            /* rising */
    LP_ASSERT_EQ(draw(&ctx, id, &index, 3400), 0);            /* fully up: nothing left to animate */
    event(&ctx, id, &index, pointer(300, 55), 3410);           /* away */
    LP_ASSERT(!lp_is_hot(&ctx, lp_id_index(id, 1)));
    LP_ASSERT_EQ(draw(&ctx, id, &index, 3420), 1);            /* falling */
    LP_ASSERT_EQ(draw(&ctx, id, &index, 3900), 0);            /* gone */
    LP_ASSERT_NEAR(lp_segmented_thumb(id), 0, 1e-6);          /* the thumb never moved */
}

LP_TEST(a_click_still_selects_and_the_arrows_still_wrap) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_id id = LP_ID("keys");
    int index = 0;
    draw(&ctx, id, &index, 100);
    lp_size sz = lp_segmented_measure(&ctx, THREE, 3, LP_CONTROL_SM);
    float seg = (sz.w - 2 * LP_SIZE_SEGMENTED_PAD) / 3;
    lp_input in = pointer(10 + LP_SIZE_SEGMENTED_PAD + seg * 2.5f, 10 + sz.h / 2);
    in.pressed = in.buttons = LP_BUTTON_LEFT;
    event(&ctx, id, &index, in, 110);
    in = pointer(10 + LP_SIZE_SEGMENTED_PAD + seg * 2.5f, 10 + sz.h / 2);
    in.released = LP_BUTTON_LEFT;
    event(&ctx, id, &index, in, 120);
    LP_ASSERT_EQ(index, 2);
    ctx.focus = id;
    in = pointer(NAN, NAN);
    in.key_pressed = 1;
    in.keysym = 0xff53;   /* Right: wraps to the first */
    event(&ctx, id, &index, in, 130);
    LP_ASSERT_EQ(index, 0);
}

static int painted_pixels(void) {
    cairo_surface_flush(surface);
    const unsigned char *d = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface), n = 0;
    for (int y = 0; y < 60; y++)
        for (int x = 0; x < 320; x++) n += ((const uint32_t *)(d + y * stride))[x] != 0;
    return n;
}

LP_TEST(text_outside_the_clip_is_never_laid_out) {
    setup();
    lp_text_style st = lp_text_style_default();
    cairo_save(cr);
    cairo_rectangle(cr, 200, 0, 120, 60);
    cairo_clip(cr);
    lp_text_draw(cr, "Library", LP_RECT(10, 10, 60, 18), &st, LP_ALIGN_START);
    cairo_restore(cr);
    LP_ASSERT_EQ(painted_pixels(), 0);
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, 120, 60);
    cairo_clip(cr);
    lp_text_draw(cr, "Library", LP_RECT(10, 10, 60, 18), &st, LP_ALIGN_START);
    cairo_restore(cr);
    LP_ASSERT(painted_pixels() > 20);
}

int main(void) {
    LP_RUN(a_new_selection_slides_the_thumb_over_motion_normal);
    LP_RUN(midway_the_thumb_is_between_the_segments_and_the_frame_is_the_tracks);
    LP_RUN(without_a_clock_or_with_reduced_motion_the_thumb_is_simply_there);
    LP_RUN(hovering_a_segment_raises_the_bead_and_leaving_lets_it_fall);
    LP_RUN(a_click_still_selects_and_the_arrows_still_wrap);
    LP_RUN(text_outside_the_clip_is_never_laid_out);
    LP_TEST_MAIN_END();
}
