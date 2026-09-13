/* A knowledge graph on a canvas (the Thread app's Graph tab): the entities and
 * relationships threadd answers to `graph`, laid out by Fruchterman–Reingold (at most
 * LP_GRAPH_MAX_NODES nodes, LP_GRAPH_ITERATIONS steps at load, then settled), nodes
 * coloured by kind, edge width by weight, labels once zoomed in enough. Ctrl+wheel (or
 * pinch-wheel) zooms, a drag pans, a click selects, a double-click asks the caller to
 * re-seed on that node. Cairo only: usable headlessly. */
#ifndef MARYUI_LP_GRAPH_H
#define MARYUI_LP_GRAPH_H

#include "maryui/lp_ui.h"

#define LP_GRAPH_MAX_NODES 120
#define LP_GRAPH_MAX_EDGES 400
#define LP_GRAPH_ITERATIONS 300
#define LP_GRAPH_LABEL_ZOOM 0.8f

struct json_object;

typedef struct lp_graph_node {
    char id[100];
    char name[128];
    char kind[32];
    int mentions;
    int documents;
    float x, y;             /* layout space, [0, 1] */
    float dx, dy;           /* the step's displacement */
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
} lp_graph;

void lp_graph_init(lp_graph *g);
/* Loads threadd's {entities, relationships, entity_count, relationship_count}; the layout is seeded and
 * LP_GRAPH_ITERATIONS steps are run. `keep` keeps the selection when its id survives. */
void lp_graph_load(lp_graph *g, struct json_object *answer, int keep);
/* One layout step (a frame while settling). 1 while more are needed. */
int lp_graph_step(lp_graph *g);
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
