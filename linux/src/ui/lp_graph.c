#include "maryui/lp_graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

void lp_graph_init(lp_graph *g) {
    memset(g, 0, sizeof *g);
    g->selected = -1;
    g->zoom = 1;
}

/* Kinds are coloured by the ontology's own names first, anything else by hash. */
lp_color lp_graph_kind_color(const char *kind) {
    static const struct { const char *kind; lp_color color; } NAMED[] = {
        { "person", { 0.22f, 0.44f, 0.65f, 1 } }, { "organization", { 0.38f, 0.55f, 0.38f, 1 } }, { "place", { 0.68f, 0.56f, 0.38f, 1 } },
        { "event", { 0.65f, 0.40f, 0.55f, 1 } }, { "work", { 0.94f, 0.56f, 0.68f, 1 } }, { "concept", { 0.45f, 0.47f, 0.52f, 1 } },
        { "file", { 0.16f, 0.37f, 0.68f, 1 } }, { "folder", { 0.56f, 0.72f, 0.93f, 1 } }, { "app", { 0.31f, 0.33f, 0.38f, 1 } },
        { "skill", { 0.2f, 0.6f, 0.6f, 1 } }, { "ability", { 0.2f, 0.5f, 0.7f, 1 } }, { "episode", { 0.7f, 0.5f, 0.3f, 1 } },
        { "turn", { 0.55f, 0.35f, 0.65f, 1 } }, { "memory", { 0.8f, 0.6f, 0.3f, 1 } },
    };
    for (size_t i = 0; i < sizeof NAMED / sizeof NAMED[0]; i++) if (kind && strcasecmp(kind, NAMED[i].kind) == 0) return NAMED[i].color;
    unsigned h = 2166136261u;
    for (const char *p = kind ? kind : ""; *p; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
    float hue = (float)(h % 360) / 360.0f, s = 0.45f, v = 0.62f;
    float c = v * s, x = c * (1 - fabsf(fmodf(hue * 6, 2) - 1)), m = v - c;
    float r = 0, gg = 0, b = 0;
    int sector = (int)(hue * 6) % 6;
    switch (sector) {
    case 0: r = c; gg = x; break;
    case 1: r = x; gg = c; break;
    case 2: gg = c; b = x; break;
    case 3: gg = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
    }
    return (lp_color){ r + m, gg + m, b + m, 1 };
}

int lp_graph_find(const lp_graph *g, const char *name) {
    for (int i = 0; name && i < g->node_count; i++) if (strcasecmp(g->nodes[i].name, name) == 0) return i;
    return -1;
}

static int index_of(const lp_graph *g, const char *id) {
    for (int i = 0; id && i < g->node_count; i++) if (strcmp(g->nodes[i].id, id) == 0) return i;
    return -1;
}

/* A deterministic seed from the id, so a reload keeps the picture. */
static float seeded(const char *id, unsigned salt) {
    unsigned h = 2166136261u ^ salt;
    for (const char *p = id; *p; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
    return (float)(h % 10000) / 10000.0f;
}

void lp_graph_load(lp_graph *g, struct json_object *answer, int keep) {
    char kept[100] = "";
    if (keep && g->selected >= 0) snprintf(kept, sizeof kept, "%s", g->nodes[g->selected].id);
    lp_graph old = *g;
    g->node_count = g->edge_count = 0;
    g->selected = -1;
    g->total_entities = g->total_relationships = 0;
#ifdef HAVE_JSONC
    struct json_object *ents = NULL, *rels = NULL, *v = NULL;
    if (json_object_object_get_ex(answer, "entities", &ents) && json_object_is_type(ents, json_type_array)) {
        for (size_t i = 0; i < json_object_array_length(ents) && g->node_count < LP_GRAPH_MAX_NODES; i++) {
            struct json_object *e = json_object_array_get_idx(ents, i), *f;
            lp_graph_node *n = &g->nodes[g->node_count];
            memset(n, 0, sizeof *n);
            if (json_object_object_get_ex(e, "id", &f)) snprintf(n->id, sizeof n->id, "%s", json_object_get_string(f));
            if (json_object_object_get_ex(e, "name", &f)) snprintf(n->name, sizeof n->name, "%s", json_object_get_string(f));
            if (json_object_object_get_ex(e, "kind", &f)) snprintf(n->kind, sizeof n->kind, "%s", json_object_get_string(f));
            if (json_object_object_get_ex(e, "mention_count", &f)) n->mentions = json_object_get_int(f);
            if (json_object_object_get_ex(e, "document_ids", &f) && json_object_is_type(f, json_type_array)) n->documents = (int)json_object_array_length(f);
            int was = index_of(&old, n->id);
            if (was >= 0) { n->x = old.nodes[was].x; n->y = old.nodes[was].y; }
            else { n->x = 0.1f + 0.8f * seeded(n->id, 1); n->y = 0.1f + 0.8f * seeded(n->id, 2); }
            g->node_count++;
        }
    }
    if (json_object_object_get_ex(answer, "relationships", &rels) && json_object_is_type(rels, json_type_array)) {
        for (size_t i = 0; i < json_object_array_length(rels) && g->edge_count < LP_GRAPH_MAX_EDGES; i++) {
            struct json_object *r = json_object_array_get_idx(rels, i), *f;
            const char *subject = json_object_object_get_ex(r, "subject_id", &f) ? json_object_get_string(f) : NULL;
            const char *object = json_object_object_get_ex(r, "object_id", &f) ? json_object_get_string(f) : NULL;
            int a = index_of(g, subject), b = index_of(g, object);
            if (a < 0 || b < 0 || a == b) continue;
            lp_graph_edge *e = &g->edges[g->edge_count++];
            memset(e, 0, sizeof *e);
            e->a = a;
            e->b = b;
            if (json_object_object_get_ex(r, "id", &f)) snprintf(e->id, sizeof e->id, "%s", json_object_get_string(f));
            if (json_object_object_get_ex(r, "predicate", &f)) snprintf(e->predicate, sizeof e->predicate, "%s", json_object_get_string(f));
            if (json_object_object_get_ex(r, "weight", &f)) e->weight = json_object_get_int(f);
        }
    }
    if (json_object_object_get_ex(answer, "entity_count", &v)) g->total_entities = json_object_get_int64(v);
    if (json_object_object_get_ex(answer, "relationship_count", &v)) g->total_relationships = json_object_get_int64(v);
#else
    (void)answer;
#endif
    if (kept[0]) g->selected = index_of(g, kept);
    g->settled = LP_GRAPH_ITERATIONS;
    for (int i = 0; i < LP_GRAPH_ITERATIONS; i++) if (!lp_graph_step(g)) break;
    g->settled = 30;    /* a few frames of settling on screen, so a reload eases into place */
}

/* Fruchterman–Reingold in the unit square: k = sqrt(area / n), temperature cooling with the steps left. */
int lp_graph_step(lp_graph *g) {
    int n = g->node_count;
    if (n < 2 || g->settled <= 0) {
        g->settled = 0;
        return 0;
    }
    float k = sqrtf(1.0f / (float)n), k2 = k * k;
    float t = 0.1f * (float)g->settled / (float)LP_GRAPH_ITERATIONS + 0.002f;
    for (int i = 0; i < n; i++) g->nodes[i].dx = g->nodes[i].dy = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float dx = g->nodes[i].x - g->nodes[j].x, dy = g->nodes[i].y - g->nodes[j].y;
            float d2 = dx * dx + dy * dy;
            if (d2 < 1e-6f) { dx = 1e-3f * (float)(i - j); dy = 1e-3f; d2 = dx * dx + dy * dy; }
            float f = k2 / d2;
            g->nodes[i].dx += dx * f;
            g->nodes[i].dy += dy * f;
            g->nodes[j].dx -= dx * f;
            g->nodes[j].dy -= dy * f;
        }
    }
    for (int e = 0; e < g->edge_count; e++) {
        lp_graph_node *a = &g->nodes[g->edges[e].a], *b = &g->nodes[g->edges[e].b];
        float dx = a->x - b->x, dy = a->y - b->y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < 1e-6f) continue;
        float f = d * d / k * (1 + 0.15f * (float)(g->edges[e].weight > 1 ? g->edges[e].weight - 1 : 0));
        a->dx -= dx / d * f;
        a->dy -= dy / d * f;
        b->dx += dx / d * f;
        b->dy += dy / d * f;
    }
    for (int i = 0; i < n; i++) {
        lp_graph_node *v = &g->nodes[i];
        /* gravity to the centre keeps disconnected pieces on the canvas */
        v->dx += (0.5f - v->x) * 0.08f;
        v->dy += (0.5f - v->y) * 0.08f;
        float d = sqrtf(v->dx * v->dx + v->dy * v->dy);
        if (d > 1e-9f) {
            float step = d < t ? d : t;
            v->x += v->dx / d * step;
            v->y += v->dy / d * step;
        }
        if (v->x < 0.08f) v->x = 0.08f;
        if (v->x > 0.92f) v->x = 0.92f;
        if (v->y < 0.08f) v->y = 0.08f;
        if (v->y > 0.92f) v->y = 0.92f;
    }
    g->settled--;
    return g->settled > 0;
}

static float radius_of(const lp_graph_node *n, float zoom) {
    float r = 5 + 2.5f * logf(1 + (float)(n->mentions > 0 ? n->mentions : 1));
    return r * (0.6f + 0.4f * zoom);
}

static void to_canvas(const lp_graph *g, lp_rect canvas, const lp_graph_node *n, float *x, float *y) {
    float side = canvas.w < canvas.h ? canvas.w : canvas.h;
    float ox = canvas.x + (canvas.w - side) / 2, oy = canvas.y + (canvas.h - side) / 2;
    float cx = canvas.x + canvas.w / 2, cy = canvas.y + canvas.h / 2;
    *x = cx + ((ox + n->x * side) - cx) * g->zoom + g->pan_x;
    *y = cy + ((oy + n->y * side) - cy) * g->zoom + g->pan_y;
}

int lp_graph_hit(const lp_graph *g, lp_rect canvas, float x, float y) {
    int best = -1;
    float best_d = 1e9f;
    for (int i = 0; i < g->node_count; i++) {
        float nx, ny;
        to_canvas(g, canvas, &g->nodes[i], &nx, &ny);
        float r = radius_of(&g->nodes[i], g->zoom) + 3, d = hypotf(x - nx, y - ny);
        if (d <= r && d < best_d) { best = i; best_d = d; }
    }
    return best;
}

int lp_graph_widget(lp_ctx *ctx, lp_id id, lp_rect canvas, lp_graph *g, int *selected_changed) {
    int reseed = -1;
    if (selected_changed) *selected_changed = 0;
    if (ctx->pass == LP_PASS_EVENT) {
        const lp_input *in = &ctx->in;
        int over = lp_hit(ctx, canvas);
        if (over && (in->scroll_y != 0 || in->scroll_x != 0) && (in->mods & (LP_MOD_CTRL | LP_MOD_LOGO))) {
            float factor = in->scroll_y < 0 ? 1.1f : 1 / 1.1f;
            float z = g->zoom * factor;
            if (z < 0.4f) z = 0.4f;
            if (z > 4) z = 4;
            /* zoom about the pointer */
            float cx = canvas.x + canvas.w / 2, cy = canvas.y + canvas.h / 2;
            float px = in->mx - cx - g->pan_x, py = in->my - cy - g->pan_y;
            g->pan_x -= px * (z / g->zoom - 1);
            g->pan_y -= py * (z / g->zoom - 1);
            g->zoom = z;
            ctx->dirty = 1;
        } else if (over && (in->scroll_y != 0 || in->scroll_x != 0)) {
            g->pan_x -= in->scroll_x;
            g->pan_y -= in->scroll_y;
            ctx->dirty = 1;
        }
        if (over && (in->pressed & LP_BUTTON_LEFT)) {
            int hit = lp_graph_hit(g, canvas, in->mx, in->my);
            if (in->double_click && hit >= 0) reseed = hit;
            else if (hit != g->selected) {
                g->selected = hit;
                if (selected_changed) *selected_changed = 1;
            }
            g->dragging = hit < 0;
            g->drag_x = in->mx;
            g->drag_y = in->my;
            ctx->active = id;
            ctx->dirty = 1;
        }
        if (g->dragging && (in->buttons & LP_BUTTON_LEFT) && !isnan(in->mx)) {
            g->pan_x += in->mx - g->drag_x;
            g->pan_y += in->my - g->drag_y;
            g->drag_x = in->mx;
            g->drag_y = in->my;
            ctx->dirty = 1;
        }
        if (in->released & LP_BUTTON_LEFT) g->dragging = 0;
        if (over) ctx->cursor = lp_graph_hit(g, canvas, in->mx, in->my) >= 0 ? LP_CURSOR_POINTER : LP_CURSOR_ARROW;
        if (g->settled > 0) {
            lp_graph_step(g);
            lp_want_frame_rect(ctx, canvas);
        }
        return reseed;
    }
    cairo_t *cr = ctx->cr;
    if (!cr) return -1;
    cairo_save(cr);
    lp_fill_solid(cr, canvas, LP_SURFACE_WELL, 0);
    cairo_rectangle(cr, canvas.x, canvas.y, canvas.w, canvas.h);
    cairo_clip(cr);
    lp_text_style ls = lp_text_style_default();
    ls.size_px = LP_TEXT_XS;
    ls.color = LP_INK_SECONDARY;
    if (!g->node_count) {
        lp_text_style es = lp_text_style_default();
        es.color = LP_INK_TERTIARY;
        lp_text_draw(cr, "Nothing in the graph yet.", canvas, &es, LP_ALIGN_CENTER);
        cairo_restore(cr);
        return -1;
    }
    /* edges */
    for (int e = 0; e < g->edge_count; e++) {
        float ax, ay, bx, by;
        to_canvas(g, canvas, &g->nodes[g->edges[e].a], &ax, &ay);
        to_canvas(g, canvas, &g->nodes[g->edges[e].b], &bx, &by);
        int touches = g->selected == g->edges[e].a || g->selected == g->edges[e].b;
        float w = 0.8f + 0.5f * logf(1 + (float)(g->edges[e].weight > 0 ? g->edges[e].weight : 1));
        lp_set_color(cr, touches ? LP_ACCENT_BLUE_BASE : lp_color_with_alpha(LP_INK_TERTIARY, 0.45f));
        cairo_set_line_width(cr, touches ? w + 0.6f : w);
        cairo_move_to(cr, ax, ay);
        cairo_line_to(cr, bx, by);
        cairo_stroke(cr);
        if (g->zoom >= LP_GRAPH_LABEL_ZOOM && touches && g->edges[e].predicate[0]) {
            lp_text_style ps = ls;
            ps.color = LP_ACCENT_BLUE_DEEP;
            lp_text_draw(cr, g->edges[e].predicate, LP_RECT((ax + bx) / 2 - 60, (ay + by) / 2 - 8, 120, 16), &ps, LP_ALIGN_CENTER);
        }
    }
    /* nodes */
    for (int i = 0; i < g->node_count; i++) {
        const lp_graph_node *n = &g->nodes[i];
        float x, y;
        to_canvas(g, canvas, n, &x, &y);
        float r = radius_of(n, g->zoom);
        lp_color c = lp_graph_kind_color(n->kind);
        cairo_arc(cr, x, y, r, 0, 2 * M_PI);
        lp_set_color(cr, c);
        cairo_fill_preserve(cr);
        lp_set_color(cr, i == g->selected ? LP_INK_PRIMARY : lp_color_with_alpha(LP_INK_PRIMARY, 0.35f));
        cairo_set_line_width(cr, i == g->selected ? 2.2f : 1);
        cairo_stroke(cr);
        if (g->zoom >= LP_GRAPH_LABEL_ZOOM || i == g->selected) {
            lp_text_style ns = ls;
            ns.ellipsize = 1;
            if (i == g->selected) ns.weight = LP_TEXT_WEIGHT_SEMIBOLD;
            lp_text_draw(cr, n->name, LP_RECT(x - 70, y + r + 2, 140, 14), &ns, LP_ALIGN_CENTER);
        }
    }
    cairo_restore(cr);
    return -1;
}
