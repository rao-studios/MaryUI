/* The graph canvas: the layout settles from the DRAW pass (the host schedules frames from what a draw
 * asks for, never from an EVENT pass), the 3D projection keeps the cube's centre at the canvas's centre
 * and paints the near last, and hit-testing agrees with what was painted. */
#include <cairo.h>

#include "lp_test.h"
#include "maryui/lp_graph.h"
#include "maryui/lp_settings.h"

static cairo_surface_t *surface;
static cairo_t *cr;
static lp_settings settings;
static const lp_rect CANVAS = { 0, 0, 400, 300 };

static void setup(void) {
    if (cr) cairo_destroy(cr);
    if (surface) cairo_surface_destroy(surface);
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 400, 300);
    cr = cairo_create(surface);
    settings = lp_settings_defaults();
}

static void node(lp_graph *g, const char *id, float x, float y, float z, int mentions) {
    lp_graph_node *n = &g->nodes[g->node_count++];
    memset(n, 0, sizeof *n);
    snprintf(n->id, sizeof n->id, "%s", id);
    snprintf(n->name, sizeof n->name, "%s", id);
    snprintf(n->kind, sizeof n->kind, "concept");
    n->x = x; n->y = y; n->z = z;
    n->mentions = mentions;
}

static void edge(lp_graph *g, int a, int b) {
    lp_graph_edge *e = &g->edges[g->edge_count++];
    memset(e, 0, sizeof *e);
    e->a = a; e->b = b; e->weight = 1;
    snprintf(e->predicate, sizeof e->predicate, "about");
}

static lp_graph *three(void) {
    static lp_graph g;
    lp_graph_init(&g);
    node(&g, "a", 0.3f, 0.3f, 0.5f, 3);
    node(&g, "b", 0.7f, 0.3f, 0.5f, 3);
    node(&g, "c", 0.5f, 0.7f, 0.5f, 3);
    edge(&g, 0, 1);
    edge(&g, 1, 2);
    return &g;
}

LP_TEST(an_event_pass_moves_nothing_and_a_draw_pass_settles_and_asks_for_a_frame) {
    setup();
    lp_graph *g = three();
    g->settled = 10;
    float x0 = g->nodes[0].x, y0 = g->nodes[0].y;
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_input in = { 0 };
    in.mx = 200; in.my = 150;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, CANVAS, 1000);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    lp_ctx_end(&ctx);
    LP_ASSERT_EQ(g->settled, 10);
    LP_ASSERT(g->nodes[0].x == x0 && g->nodes[0].y == y0);
    LP_ASSERT(!ctx.wants_frame);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, CANVAS, 1016);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    LP_ASSERT_EQ(g->settled, 9);
    LP_ASSERT(g->nodes[0].x != x0 || g->nodes[0].y != y0);
    LP_ASSERT(ctx.wants_frame);
    lp_ctx_end(&ctx);
    /* at rest nothing is asked for */
    g->settled = 0;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, CANVAS, 1032);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    LP_ASSERT(!ctx.wants_frame);
    lp_ctx_end(&ctx);
    lp_graph_free(g);
}

LP_TEST(the_cubes_centre_projects_to_the_canvas_centre_at_any_turn) {
    lp_graph *g = three();
    node(g, "centre", 0.5f, 0.5f, 0.5f, 1);
    g->mode = LP_GRAPH_3D;
    float x, y, depth;
    for (int i = 0; i < 5; i++) {
        g->yaw = 0.7f * i;
        g->pitch = 0.2f * i - 0.4f;
        LP_ASSERT(lp_graph_project(g, CANVAS, 3, &x, &y, &depth));
        LP_ASSERT_NEAR(x, 200, 1e-3);
        LP_ASSERT_NEAR(y, 150, 1e-3);
        LP_ASSERT_NEAR(depth, 0, 1e-5);
    }
    /* flat: the same node, no perspective */
    g->mode = LP_GRAPH_2D;
    LP_ASSERT(lp_graph_project(g, CANVAS, 0, &x, &y, &depth));
    LP_ASSERT_NEAR(x, 200 + (0.3f - 0.5f) * (300 - 2 * LP_GRAPH_MARGIN), 1e-3);   /* the square less its margin */
    LP_ASSERT_NEAR(depth, 0, 1e-6);
    lp_graph_free(g);
}

LP_TEST(nearer_nodes_are_larger_and_win_the_hit_test) {
    setup();
    lp_graph *g = three();
    g->node_count = 0;
    node(g, "far", 0.5f, 0.5f, 0.2f, 3);
    node(g, "near", 0.5f, 0.5f, 0.8f, 3);
    g->mode = LP_GRAPH_3D;
    g->yaw = 0;
    g->pitch = 0;
    float fx, fy, fd, nx, ny, nd;
    lp_graph_project(g, CANVAS, 0, &fx, &fy, &fd);
    lp_graph_project(g, CANVAS, 1, &nx, &ny, &nd);
    LP_ASSERT(nd > fd);
    LP_ASSERT_NEAR(fx, nx, 1e-3);            /* straight behind one another */
    LP_ASSERT_EQ(lp_graph_hit(g, CANVAS, nx, ny), 1);
    /* a draw paints the far one first: after it, the recorded depths say which came last */
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, CANVAS, 0);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    lp_ctx_end(&ctx);
    LP_ASSERT(g->nodes[1].depth > g->nodes[0].depth);
    lp_graph_free(g);
}

LP_TEST(in_3d_the_graph_turns_on_its_own_unless_the_pointer_is_over_it) {
    setup();
    lp_graph *g = three();
    g->mode = LP_GRAPH_3D;
    g->settled = 0;
    float yaw = g->yaw;
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, CANVAS, 5000);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    LP_ASSERT(ctx.wants_frame);
    lp_ctx_end(&ctx);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, CANVAS, 5100);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    lp_ctx_end(&ctx);
    LP_ASSERT(g->yaw > yaw);
    /* the pointer over the canvas holds it still; reduced motion too */
    lp_input in = { 0 };
    in.mx = 100; in.my = 100;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, CANVAS, 5110);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    lp_ctx_end(&ctx);
    yaw = g->yaw;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, CANVAS, 5200);
    lp_graph_widget(&ctx, LP_ID("g"), CANVAS, g, NULL);
    LP_ASSERT(!ctx.wants_frame);
    lp_ctx_end(&ctx);
    LP_ASSERT_NEAR(g->yaw, yaw, 1e-6);
    lp_graph_free(g);
}

LP_TEST(switching_modes_relaxes_the_layout_into_the_new_space) {
    lp_graph *g = three();
    g->settled = 0;
    g->nodes[0].z = 0.2f;                   /* off the others' plane: the cube's third axis has work to do */
    lp_graph_set_mode(g, LP_GRAPH_3D);
    LP_ASSERT(g->settled > 0);
    LP_ASSERT_EQ(g->mode, LP_GRAPH_3D);
    float z = g->nodes[0].z;
    while (lp_graph_step(g)) {}
    LP_ASSERT(g->nodes[0].z != z);          /* the cube's third axis took part */
    lp_graph_set_mode(g, LP_GRAPH_3D);      /* the same mode again changes nothing */
    LP_ASSERT_EQ(g->settled, 0);
    lp_graph_free(g);
}

int main(void) {
    LP_RUN(an_event_pass_moves_nothing_and_a_draw_pass_settles_and_asks_for_a_frame);
    LP_RUN(the_cubes_centre_projects_to_the_canvas_centre_at_any_turn);
    LP_RUN(nearer_nodes_are_larger_and_win_the_hit_test);
    LP_RUN(in_3d_the_graph_turns_on_its_own_unless_the_pointer_is_over_it);
    LP_RUN(switching_modes_relaxes_the_layout_into_the_new_space);
    LP_TEST_MAIN_END();
}
