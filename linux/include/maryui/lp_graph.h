/* A knowledge graph on a canvas (the Thread app's Graph tab): the entities and
 * relationships threadd answers to `graph`, laid out by Fruchterman–Reingold (at most
 * LP_GRAPH_MAX_NODES nodes, LP_GRAPH_ITERATIONS steps at load, then a few frames of
 * settling on screen), nodes coloured by kind, edge width by weight, labels once zoomed in
 * enough. Flat by default; in 3D the same layout runs in the unit cube and is projected
 * with a little perspective, the nearest nodes painted last and largest, and a slow turn
 * keeps it alive while nothing is selected and the pointer is elsewhere. Ctrl+wheel (or
 * pinch-wheel) zooms; a drag pans (in 3D it turns the cube; Shift+drag pans); a click
 * selects; a double-click asks the caller to re-seed on that node. The settling and the
 * turn run in the DRAW pass and ask for their frames with lp_want_frame_rect, so a graph at
 * rest costs nothing. Labels are laid out once per load and kept as small surfaces. Cairo
 * only: usable headlessly. */
#ifndef MARYUI_LP_GRAPH_H
#define MARYUI_LP_GRAPH_H

#include <cairo.h>

#include "maryui/lp_ui.h"

#define LP_GRAPH_MAX_NODES 120
#define LP_GRAPH_MAX_EDGES 400
#define LP_GRAPH_ITERATIONS 300
#define LP_GRAPH_LABEL_ZOOM 0.8f
/* The 3D camera's distance from the cube's centre (cube units) and the idle turn (radians per second). */
#define LP_GRAPH_CAMERA 2.2f
#define LP_GRAPH_SPIN 0.15f
/* How much of the canvas's shorter side the turned cube may span: a near corner would otherwise leave it. */
#define LP_GRAPH_FIT_3D 0.5f

struct json_object;

enum lp_graph_mode { LP_GRAPH_2D, LP_GRAPH_3D };

typedef struct lp_graph_node {
    char id[100];
    char name[128];
    char kind[32];
    int mentions;
    int documents;
    float x, y, z;          /* layout space, [0, 1] (z unused in 2D) */
    float dx, dy, dz;       /* the step's displacement */
    float px, py, depth;    /* where the last projection put it on the canvas, and how near (higher: nearer) */
    cairo_surface_t *label; /* the name at the label size, laid out once; NULL until first drawn */
    float label_w, label_h;
} lp_graph_node;

typedef struct lp_graph_edge {
    char id[100];
    int a, b;               /* node indices */
    char predicate[64];
    int weight;
} lp_graph_edge;

typedef struct lp_graph {
    lp_graph_node nodes[LP_GRAPH_MAX_NODES];
    int node_count;
    lp_graph_edge edges[LP_GRAPH_MAX_EDGES];
    int edge_count;
    int selected;           /* node index, or -1 */
    int settled;            /* layout steps left before rest, 0 at rest */
    float zoom;             /* 1: the layout fills the canvas */
    float pan_x, pan_y;     /* canvas pixels */
    int dragging;
    float drag_x, drag_y;
    int64_t total_entities, total_relationships;
    enum lp_graph_mode mode;
    float yaw, pitch;       /* the cube's turn, radians */
    int pointer_inside;     /* the pointer was over the canvas at the last EVENT pass */
    double last_ms;         /* when the last frame was drawn (the idle turn's clock) */
} lp_graph;

void lp_graph_init(lp_graph *g);
/* Frees the label surfaces. The graph may be loaded again afterwards. */
void lp_graph_free(lp_graph *g);
/* Loads threadd's {entities, relationships, entity_count, relationship_count}; the layout is seeded and
 * LP_GRAPH_ITERATIONS steps are run. `keep` keeps the selection when its id survives. */
void lp_graph_load(lp_graph *g, struct json_object *answer, int keep);
/* Switches between flat and 3D; the layout relaxes into the new space over the next frames. */
void lp_graph_set_mode(lp_graph *g, enum lp_graph_mode mode);
/* One layout step (a frame while settling). 1 while more are needed. */
int lp_graph_step(lp_graph *g);
/* Where node i lands on the canvas (its centre), with how near it is: the 3D projection, or the flat
 * placement with depth 0. 1 when i is a node. */
int lp_graph_project(const lp_graph *g, lp_rect canvas, int i, float *x, float *y, float *depth);
/* The node under (x, y) in canvas coordinates, or -1. */
int lp_graph_hit(const lp_graph *g, lp_rect canvas, float x, float y);
/* Paints the canvas and handles its input. Returns the node double-clicked (a re-seed), else -1;
 * *selected_changed says the selection moved. */
int lp_graph_widget(lp_ctx *ctx, lp_id id, lp_rect canvas, lp_graph *g, int *selected_changed);
/* The colour for a kind (the same one every time). */
lp_color lp_graph_kind_color(const char *kind);
/* The index of the node whose name matches (case-insensitive), else -1. */
int lp_graph_find(const lp_graph *g, const char *name);

#endif
