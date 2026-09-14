/* Threads — the hard drive's memory, as the Mac's Thread app shows it (PARITY D25):
 * the Drive (the volume, the database, parity with the disk), the Library (the record
 * families, their groups and documents), the Graph (an interactive node-link canvas
 * with the repair bench), the Schemas (one card per family), the Ledger (every event)
 * and Retrieval (what each search returned). Everything comes from threadd through
 * lp_thread; a window whose answers are stale asks again. Linux only. */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_files.h"
#include "maryui/lp_graph.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_job.h"
#include "maryui/lp_pane.h"
#include "maryui/lp_text.h"
#include "maryui/lp_thread.h"
#include "maryui/lp_tokens.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

#define TABS 6
#define CARD_PAD LP_PANE_CARD_PAD
#define SIDE_W 280
#define REFRESH_MS 5000

enum { TAB_DRIVE, TAB_LIBRARY, TAB_GRAPH, TAB_SCHEMAS, TAB_LEDGER, TAB_RETRIEVAL };
static const char *const TAB_NAMES[TABS] = { "drive", "library", "graph", "schemas", "ledger", "retrieval" };

struct thread_app {
    char window_id[12];
    lp_desktop *desk;
    int tab;
    lp_scroll_state scroll[TABS];
    int asked;                      /* the first requests went out */
    lp_source *timer;
    /* library */
    char open_group[200];
    char open_document[200];
    char family_filter[32];
    /* graph */
    lp_graph graph;
    lp_text_buffer seed;
    lp_text_buffer repair;
    int hops;
    int with_documents;
    char kind_filter[32];
    char file_path[1024];           /* the file the graph is centred on (View Thread) */
    /* ledger */
    char ledger_kind[24];
    char status[240];
    lp_job *(*run)(lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user);
    lp_job *job;
};

/* MARK: - JSON helpers */

#ifdef HAVE_JSONC
static const char *str(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string) ? json_object_get_string(v) : NULL;
}
static int64_t num(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) ? json_object_get_int64(v) : 0;
}
static struct json_object *arr(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_array) ? v : NULL;
}
static struct json_object *obj(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_object) ? v : NULL;
}
static size_t alen(struct json_object *a) { return a ? json_object_array_length(a) : 0; }
static struct json_object *at(struct json_object *a, size_t i) { return a ? json_object_array_get_idx(a, i) : NULL; }
#endif

static const char *or_dash(const char *s) { return s && *s ? s : "—"; }

static void thousands(int64_t n, char *out, size_t cap) {
    char digits[32];
    snprintf(digits, sizeof digits, "%lld", (long long)n);
    size_t len = strlen(digits), o = 0;
    for (size_t i = 0; i < len && o + 2 < cap; i++) {
        if (i && (len - i) % 3 == 0) out[o++] = ',';
        out[o++] = digits[i];
    }
    out[o] = 0;
}

static void when(int64_t ms, char *out, size_t cap) {
    if (ms <= 0) { snprintf(out, cap, "never"); return; }
    time_t t = (time_t)(ms > 100000000000LL ? ms / 1000 : ms);
    lp_files_format_date(t, time(NULL), out, cap);
}

static void age(int64_t ms, char *out, size_t cap) {
    if (ms <= 0) { snprintf(out, cap, "—"); return; }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int64_t now = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000, d = (now - ms) / 1000;
    if (d < 5) snprintf(out, cap, "just now");
    else if (d < 60) snprintf(out, cap, "%llds ago", (long long)d);
    else if (d < 3600) snprintf(out, cap, "%lldm ago", (long long)(d / 60));
    else if (d < 86400) snprintf(out, cap, "%lldh ago", (long long)(d / 3600));
    else snprintf(out, cap, "%lldd ago", (long long)(d / 86400));
}

/* MARK: - Requests */

static void ask_graph(struct thread_app *a) {
    const char *kinds[1] = { a->kind_filter };
    lp_thread_graph(&a->desk->thread, a->seed.text[0] ? a->seed.text : NULL, NULL, kinds, a->kind_filter[0] ? 1 : 0, a->hops, 40, a->with_documents);
}

static void ask_tab(struct thread_app *a, int tab) {
    lp_thread *t = &a->desk->thread;
    if (!lp_thread_connected(t)) return;
    switch (tab) {
    case TAB_DRIVE: lp_thread_stats(t); lp_thread_parity(t); break;
    case TAB_LIBRARY: lp_thread_schemas(t); lp_thread_library(t, 0, NULL); break;
    case TAB_GRAPH: ask_graph(a); break;
    case TAB_SCHEMAS: lp_thread_schemas(t); break;
    case TAB_LEDGER: lp_thread_ledger(t, a->ledger_kind[0] ? a->ledger_kind : NULL, 200); break;
    case TAB_RETRIEVAL: lp_thread_ledger(t, "search", 100); break;
    }
}

static void ask_all(struct thread_app *a) {
    lp_thread *t = &a->desk->thread;
    if (!lp_thread_connected(t)) return;
    lp_thread_stats(t);
    lp_thread_parity(t);
    lp_thread_schemas(t);
    lp_thread_library(t, 0, NULL);
    ask_graph(a);
    a->asked = 1;
}

static int on_tick(int fd, uint32_t mask, void *data) {
    struct thread_app *a = data;
    int w = lp_wm_find(&a->desk->wm, a->window_id);
    if (w >= 0 && a->desk->wm.windows[w].state != LP_WIN_SHADED && lp_thread_connected(&a->desk->thread)) {
        if (!a->asked) ask_all(a);
        else if (a->tab == TAB_DRIVE) ask_tab(a, TAB_DRIVE);
    }
    lp_desktop_update_timer(a->desk, a->timer, REFRESH_MS);
    return 0;
}

static void set_tab(struct thread_app *a, int tab) {
    if (tab < 0 || tab >= TABS) return;
    if (a->tab != tab) a->status[0] = 0;
    a->tab = tab;
    ask_tab(a, tab);
}

/* MARK: - Drawing helpers: the pane kit's (lp_pane.h), shared with Ambient and Abilities */

/* A card cut off the top of *area; returns the y its body starts at. */
static float card(lp_ctx *ctx, lp_rect *area, const char *title, float body_h) { return lp_pane_card(ctx, area, title, body_h, NULL).y; }
static void row(lp_ctx *ctx, float x, float *y, float w, const char *label, const char *value) { lp_pane_row(ctx, x, y, w, label, value); }
static void mono(lp_ctx *ctx, lp_rect r, const char *text, lp_color color) { lp_pane_mono(ctx, r.x, r.y, r.w, text, color); }
static int chip(lp_ctx *ctx, lp_id id, float x, float y, const char *label, int selected, lp_size *size) { return lp_pane_chip(ctx, id, x, y, label, selected, size); }
static void paragraph(lp_ctx *ctx, lp_rect r, const char *text, lp_color color, int italic) {
    float y = r.y;
    lp_pane_paragraph(ctx, r.x, &y, r.w, text, color, italic);
}

/* MARK: - Drive */

static void reconcile_done(int status, const char *output, void *user) {
    struct thread_app *a = user;
    a->job = NULL;
    snprintf(a->status, sizeof a->status, status == 0 ? "Reconciled the home with the Thread." : "indexd could not reconcile: %.150s", output ? output : "");
    ask_tab(a, TAB_DRIVE);
    if (a->desk && a->desk->on_app_dirty) a->desk->on_app_dirty(a->desk, a->window_id);
}

static void reconcile(struct thread_app *a) {
    if (a->job || !a->run) return;
    static const char *const argv[] = { "indexd", "--once", NULL };
    snprintf(a->status, sizeof a->status, "Reconciling the home with the Thread…");
    a->job = (lp_job *)1;
    lp_job *job = a->run(a->desk, argv, reconcile_done, a);
    if (a->job) a->job = job;
}

static void paint_drive(lp_ctx *ctx, struct thread_app *a, lp_rect area, lp_id base) {
    lp_thread *t = &a->desk->thread;
    lp_rect inner = lp_pane_content(area);
    const float chrome = LP_PANE_CARD_TITLE_H + LP_PANE_CARD_PAD + LP_PANE_CARD_GAP;
    lp_size extent = { inner.w, 3 * chrome + 3 * LP_PANE_ROW + 8 * LP_PANE_ROW + 84 + LP_SPACE_4 };
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 10), inner, extent, &a->scroll[TAB_DRIVE]);
    char v1[64], v2[64], v3[64];
    /* the volume */
    float y = card(ctx, &c, "The volume", 3 * LP_PANE_ROW);
    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        int64_t total = (int64_t)vfs.f_blocks * (int64_t)vfs.f_frsize, avail = (int64_t)vfs.f_bavail * (int64_t)vfs.f_frsize;
        lp_files_format_size(total, 0, v1, sizeof v1);
        lp_files_format_size(avail, 0, v2, sizeof v2);
    } else {
        snprintf(v1, sizeof v1, "—");
        snprintf(v2, sizeof v2, "—");
    }
    row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Name", a->desk->branding.name[0] ? a->desk->branding.name : "MaryOS");
    row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Size", v1);
    row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Available", v2);
    /* the database */
    y = card(ctx, &c, "The database", 8 * LP_PANE_ROW);
#ifdef HAVE_JSONC
    struct json_object *stats = lp_thread_answer(t, LP_THREAD_STATS);
    if (stats) {
        char b[64];
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Node", or_dash(str(stats, "node_id")));
        lp_files_format_size(num(stats, "db_bytes") + num(stats, "wal_bytes"), 0, b, sizeof b);
        snprintf(v1, sizeof v1, "%s in /var/lib/thread/thread.db", b);
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Size", v1);
        thousands(num(stats, "documents"), v1, sizeof v1);
        thousands(num(stats, "partitions"), v2, sizeof v2);
        thousands(num(stats, "embedded"), v3, sizeof v3);
        char line[160];
        snprintf(line, sizeof line, "%s documents · %s partitions, %s embedded", v1, v2, v3);
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Documents", line);
        thousands(num(stats, "entities"), v1, sizeof v1);
        thousands(num(stats, "relationships"), v2, sizeof v2);
        thousands(num(stats, "predicates"), v3, sizeof v3);
        snprintf(line, sizeof line, "%s entities · %s relationships · %s predicates", v1, v2, v3);
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Graph", line);
        struct json_object *vectors = obj(stats, "vectors");
        lp_files_format_size(num(vectors, "bytes"), 0, b, sizeof b);
        thousands(num(vectors, "count"), v1, sizeof v1);
        snprintf(line, sizeof line, "%s vectors of %lld · %s resident", v1, (long long)num(vectors, "dim"), b);
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Vectors", line);
        snprintf(line, sizeof line, "%lld pending · %lld failed", (long long)num(stats, "jobs_pending"), (long long)num(stats, "jobs_failed"));
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Enrichment", line);
        thousands(num(stats, "ledger_rows"), v1, sizeof v1);
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Ledger", v1);
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Embeddings", or_dash(str(stats, "embedding_model")));
    } else
#endif
    {
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Database", lp_thread_connected(t) ? "Asking threadd…" : "threadd is not running");
    }
    /* parity */
    y = card(ctx, &c, "Parity with the disk", 84);
#ifdef HAVE_JSONC
    struct json_object *parity = lp_thread_answer(t, LP_THREAD_PARITY);
    int64_t files = num(parity, "files"), seen = num(parity, "seen"), recorded = num(parity, "recorded");
    int64_t missing = num(parity, "missing"), stale = num(parity, "stale"), orphaned = num(parity, "orphaned");
    if (parity && num(parity, "run_id")) {
        char gauge[160], last[48], counts[160];
        thousands(recorded, v1, sizeof v1);
        thousands(seen, v2, sizeof v2);
        snprintf(gauge, sizeof gauge, "%s of %s files in the graph", v1, v2);
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            lp_text_style gs = lp_text_style_default();
            gs.size_px = LP_TEXT_LG;
            gs.weight = LP_TEXT_WEIGHT_SEMIBOLD;
            lp_text_draw(ctx->cr, gauge, LP_RECT(c.x + CARD_PAD, y, c.w - 2 * CARD_PAD, 22), &gs, LP_ALIGN_START);
            lp_progress(ctx, LP_RECT(c.x + CARD_PAD, y + 26, c.w - 2 * CARD_PAD, LP_PROGRESS_H), seen ? (float)((double)recorded / (double)seen) : 1);
        }
        when(num(parity, "finished_ms"), last, sizeof last);
        snprintf(counts, sizeof counts, "%lld missing · %lld stale · %lld orphaned · last reconcile %s", (long long)missing, (long long)stale, (long long)orphaned, last);
        y += 40;
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Last run", counts);
        thousands(files, v1, sizeof v1);
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "File records", v1);
    } else
#endif
    {
        row(ctx, c.x + CARD_PAD, &y, c.w - 2 * CARD_PAD, "Parity", "No reconcile has run yet.");
    }
    lp_scroll_end(ctx);
}

/* MARK: - Library */

static void paint_document(lp_ctx *ctx, struct thread_app *a, lp_rect area, lp_id base) {
#ifdef HAVE_JSONC
    struct json_object *docs = lp_thread_answer(&a->desk->thread, LP_THREAD_DOCUMENTS), *doc = NULL;
    struct json_object *list = arr(docs, "documents");
    for (size_t i = 0; i < alen(list); i++) {
        const char *id = str(at(list, i), "id");
        if (id && strcmp(id, a->open_document) == 0) doc = at(list, i);
    }
    lp_rect inner = lp_pane_content(area);
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_BACK, 0, 0 };
    lp_size bs = lp_button_measure(ctx, "Library", bo);
    if (lp_button(ctx, lp_id_index(base, 20), LP_RECT(inner.x, inner.y, bs.w, bs.h), "Library", bo)) {
        a->open_document[0] = 0;
        ctx->dirty = 1;
        return;
    }
    inner.y += bs.h + LP_SPACE_3;
    inner.h -= bs.h + LP_SPACE_3;
    if (!doc) {
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            lp_text_style ss = lp_text_style_default();
            ss.color = LP_INK_TERTIARY;
            lp_text_draw(ctx->cr, "Reading the document…", inner, &ss, LP_ALIGN_CENTER);
        }
        return;
    }
    /* the head: name, badge, group, date, id */
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style ns = lp_text_style_default();
        ns.size_px = LP_TEXT_LG;
        ns.weight = LP_TEXT_WEIGHT_SEMIBOLD;
        ns.ellipsize = 1;
        lp_text_draw(ctx->cr, or_dash(str(doc, "name")), LP_RECT(inner.x, inner.y, inner.w - 100, 22), &ns, LP_ALIGN_START);
        char meta[320], date[48];
        when(num(doc, "created_at"), date, sizeof date);
        const char *label = str(doc, "group_label");
        snprintf(meta, sizeof meta, "%s · %s · %s · %s", or_dash(str(doc, "family")), label && *label ? label : or_dash(str(doc, "group_id")), date,
                 or_dash(str(doc, "enrich_state")));
        lp_text_style ms = lp_text_style_default();
        ms.size_px = LP_TEXT_SM;
        ms.color = (lp_color){ 0.68f, 0.56f, 0.38f, 1 };
        lp_text_draw(ctx->cr, meta, LP_RECT(inner.x, inner.y + 24, inner.w, 18), &ms, LP_ALIGN_START);
    }
    mono(ctx, LP_RECT(inner.x, inner.y + 44, inner.w, 14), or_dash(str(doc, "id")), LP_INK_TERTIARY);
    lp_rect body = LP_RECT(inner.x, inner.y + 66, inner.w, inner.h - 66);
    /* the body: each text as a passage; a behaviour record as its codec view */
    struct json_object *texts = arr(doc, "texts");
    char *joined = NULL;
    size_t jl = 0;
    for (size_t i = 0; i < alen(texts); i++) {
        const char *t = json_object_get_string(at(texts, i));
        size_t tl = t ? strlen(t) : 0;
        joined = realloc(joined, jl + tl + 3);
        if (jl) { joined[jl++] = '\n'; joined[jl++] = '\n'; }
        memcpy(joined + jl, t ? t : "", tl);
        jl += tl;
        joined[jl] = 0;
    }
    lp_size extent = { body.w, 4000 };
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 21), body, extent, &a->scroll[TAB_LIBRARY]);
    const char *family = str(doc, "family");
    if (family && strcmp(family, "behavior") == 0 && alen(texts)) {
        /* BehavioralCodec: one JSON line → Query / Prior / Ambient / Did act / Sealed / actions */
        struct json_object *ep = json_tokener_parse(json_object_get_string(at(texts, 0)));
        float y = c.y;
        struct json_object *input = obj(ep, "input"), *output = obj(ep, "output");
        row(ctx, c.x, &y, c.w, "Query", or_dash(str(input, "query")));
        row(ctx, c.x, &y, c.w, "Prior", or_dash(str(input, "priorEpisodeID")));
        row(ctx, c.x, &y, c.w, "Ambient", or_dash(str(input, "ambient")));
        row(ctx, c.x, &y, c.w, "Sealed", or_dash(str(ep, "sealedReason")));
        struct json_object *actions = arr(output, "actions");
        char n[32];
        snprintf(n, sizeof n, "%zu", alen(actions));
        row(ctx, c.x, &y, c.w, "Did act", alen(actions) ? "yes" : "no");
        row(ctx, c.x, &y, c.w, "Actions", n);
        for (size_t i = 0; i < alen(actions); i++) {
            struct json_object *act = at(actions, i);
            char line[240];
            snprintf(line, sizeof line, "%s · %s — %s", or_dash(str(act, "application")), or_dash(str(act, "skill")), or_dash(str(act, "outcome")));
            row(ctx, c.x, &y, c.w, "", line);
        }
        if (ep) json_object_put(ep);
    } else {
        paragraph(ctx, LP_RECT(c.x, c.y, c.w, extent.h), joined ? joined : "", LP_INK_PRIMARY, family && strcmp(family, "conversation") == 0);
    }
    free(joined);
    lp_scroll_end(ctx);
#else
    (void)ctx; (void)a; (void)area; (void)base;
#endif
}

static void paint_library(lp_ctx *ctx, struct thread_app *a, lp_rect area, lp_id base) {
    if (a->open_document[0]) {
        paint_document(ctx, a, area, base);
        return;
    }
#ifdef HAVE_JSONC
    lp_thread *t = &a->desk->thread;
    struct json_object *schemas = lp_thread_answer(t, LP_THREAD_SCHEMAS), *library = lp_thread_answer(t, LP_THREAD_LIBRARY);
    lp_rect inner = lp_pane_content(area);
    /* family chips with counts */
    float x = inner.x, y = inner.y;
    lp_size cs;
    if (chip(ctx, lp_id_index(base, 30), x, y, "All", a->family_filter[0] == 0, &cs)) { a->family_filter[0] = 0; ctx->dirty = 1; }
    x += cs.w + LP_SPACE_1;
    struct json_object *families = arr(schemas, "families");
    for (size_t i = 0; i < alen(families) && i < 12; i++) {
        struct json_object *f = at(families, i);
        char label[64];
        snprintf(label, sizeof label, "%s %lld", or_dash(str(f, "name")), (long long)num(f, "count"));
        if (x + 90 > inner.x + inner.w) { x = inner.x; y += cs.h + LP_SPACE_1; }
        int selected = strcmp(a->family_filter, or_dash(str(f, "name"))) == 0;
        if (chip(ctx, lp_id_index(base, 31 + (int)i), x, y, label, selected, &cs)) {
            snprintf(a->family_filter, sizeof a->family_filter, "%s", selected ? "" : or_dash(str(f, "name")));
            ctx->dirty = 1;
        }
        x += cs.w + LP_SPACE_1;
    }
    y += cs.h + LP_SPACE_3;
    lp_rect list = LP_RECT(inner.x, y, inner.w, inner.y + inner.h - y);
    struct json_object *groups = arr(library, "groups");
    /* the extent: each group a card of 56, expanded by its documents */
    float extent_h = 0;
    for (size_t g = 0; g < alen(groups); g++) {
        struct json_object *group = at(groups, g);
        const char *family = str(group, "family");
        if (a->family_filter[0] && (!family || strcmp(family, a->family_filter) != 0)) continue;
        extent_h += 56;
        if (strcmp(a->open_group, or_dash(str(group, "id"))) == 0) extent_h += (float)alen(arr(group, "documents")) * LP_LIST_ROW_H + LP_SPACE_2;
    }
    lp_size extent = { list.w, extent_h > list.h ? extent_h : list.h };
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 50), list, extent, &a->scroll[TAB_LIBRARY]);
    float gy = c.y;
    int n_shown = 0;
    for (size_t g = 0; g < alen(groups); g++) {
        struct json_object *group = at(groups, g);
        const char *family = str(group, "family"), *id = or_dash(str(group, "id")), *label = str(group, "label");
        if (a->family_filter[0] && (!family || strcmp(family, a->family_filter) != 0)) continue;
        n_shown++;
        struct json_object *docs = arr(group, "documents");
        int open = strcmp(a->open_group, id) == 0;
        lp_rect gr = LP_RECT(c.x, gy, c.w, 52);
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            lp_fill_solid(ctx->cr, gr, open ? LP_SURFACE_SELECTED : LP_RGBA(1, 1, 1, 0.6f), LP_RADIUS_SM);
            lp_text_style ns = lp_text_style_default();
            ns.weight = LP_TEXT_WEIGHT_SEMIBOLD;
            ns.ellipsize = 1;
            lp_text_draw(ctx->cr, label && *label ? label : id, LP_RECT(gr.x + LP_SPACE_3, gr.y + 6, gr.w - 220, 18), &ns, LP_ALIGN_START);
            char count[48];
            snprintf(count, sizeof count, "%zu document%s", alen(docs), alen(docs) == 1 ? "" : "s");
            lp_text_style cs2 = lp_text_style_default();
            cs2.size_px = LP_TEXT_SM;
            cs2.color = LP_INK_TERTIARY;
            lp_text_draw(ctx->cr, count, LP_RECT(gr.x + gr.w - 280, gr.y + 6, 150, 18), &cs2, LP_ALIGN_END);
            lp_text_style bs = cs2;
            bs.color = lp_graph_kind_color(family ? family : "");
            bs.weight = LP_TEXT_WEIGHT_SEMIBOLD;
            lp_text_draw(ctx->cr, or_dash(family), LP_RECT(gr.x + gr.w - 116, gr.y + 6, 104, 18), &bs, LP_ALIGN_END);
        }
        mono(ctx, LP_RECT(gr.x + LP_SPACE_3, gr.y + 28, gr.w - 24, 14), id, LP_INK_TERTIARY);
        if (lp_clicked(ctx, lp_id_index(base, 100 + (int)g), gr)) {
            snprintf(a->open_group, sizeof a->open_group, "%s", open ? "" : id);
            ctx->dirty = 1;
        }
        gy += 56;
        if (!open) continue;
        for (size_t k = 0; k < alen(docs); k++) {
            struct json_object *doc = at(docs, k);
            char date[48];
            when(num(doc, "created_at"), date, sizeof date);
            const char *cols[2] = { date, or_dash(str(doc, "family")) };
            lp_rect rr = LP_RECT(c.x + LP_SPACE_4, gy, c.w - LP_SPACE_4, LP_LIST_ROW_H);
            if (lp_list_row(ctx, lp_id_index(base, 1000 + (int)g * 64 + (int)k), rr, LP_ICON_DOCUMENT, or_dash(str(doc, "name")), cols, 2, 0, (int)k % 2)) {
                const char *did = str(doc, "id");
                if (did) {
                    snprintf(a->open_document, sizeof a->open_document, "%s", did);
                    const char *ids[1] = { did };
                    lp_thread_documents(t, ids, 1);
                    a->scroll[TAB_LIBRARY].y = 0;
                    ctx->dirty = 1;
                }
            }
            gy += LP_LIST_ROW_H;
        }
        gy += LP_SPACE_2;
    }
    if (!n_shown && ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style es = lp_text_style_default();
        es.color = LP_INK_TERTIARY;
        lp_text_draw(ctx->cr, library ? "Nothing here yet." : lp_thread_connected(t) ? "Reading the library…" : "threadd is not running.", list, &es, LP_ALIGN_CENTER);
    }
    lp_scroll_end(ctx);
#else
    (void)ctx; (void)a; (void)area; (void)base;
#endif
}

/* MARK: - Graph */

static const char *totals_text(const lp_graph *g) {
    static char totals[128];
    char e[32], r[32];
    thousands(g->total_entities, e, sizeof e);
    thousands(g->total_relationships, r, sizeof r);
    snprintf(totals, sizeof totals, "%d of %s entities shown \xC2\xB7 %s relationships in all", g->node_count, e, r);
    return totals;
}

static void paint_node_card(lp_ctx *ctx, struct thread_app *a, lp_rect side, lp_id base) {
    lp_thread *t = &a->desk->thread;
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_fill_solid(ctx->cr, side, LP_SURFACE_SIDEBAR, 0);
    lp_rect inner = lp_rect_inset(side, LP_SPACE_3, LP_SPACE_3);
    float y = inner.y;
#ifdef HAVE_JSONC
    /* the file the graph was opened on */
    struct json_object *record = a->file_path[0] ? lp_thread_answer(t, LP_THREAD_FILE_RECORD) : NULL;
    if (record) {
        struct json_object *file = obj(record, "file"), *doc = obj(record, "document");
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            lp_text_style hs = lp_text_style_default();
            hs.weight = LP_TEXT_WEIGHT_SEMIBOLD;
            hs.ellipsize = 1;
            lp_text_draw(ctx->cr, or_dash(str(doc, "name")), LP_RECT(inner.x, y, inner.w, 18), &hs, LP_ALIGN_START);
        }
        y += 22;
        char v[64], d1[48], d2[48];
        row(ctx, inner.x, &y, inner.w, "Path", or_dash(str(file, "path")));
        row(ctx, inner.x, &y, inner.w, "Kind", or_dash(str(file, "kind")));
        lp_files_format_size(num(file, "size"), 0, v, sizeof v);
        row(ctx, inner.x, &y, inner.w, "Size", v);
        when(num(file, "mtime_ms"), d1, sizeof d1);
        row(ctx, inner.x, &y, inner.w, "Modified", d1);
        when(num(file, "seen_ms"), d2, sizeof d2);
        row(ctx, inner.x, &y, inner.w, "Indexed", d2);
        snprintf(v, sizeof v, "%zu", alen(arr(record, "partitions")));
        row(ctx, inner.x, &y, inner.w, "Chunks", v);
        row(ctx, inner.x, &y, inner.w, "State", or_dash(str(record, "enrich_state")));
        row(ctx, inner.x, &y, inner.w, "Group", or_dash(str(doc, "group_id")));
        mono(ctx, LP_RECT(inner.x, y, inner.w, 14), or_dash(str(file, "content_hash")), LP_INK_TERTIARY);
        y += 20;
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_draw_hairline(ctx->cr, LP_RECT(inner.x, y, inner.w, 0), LP_EDGE_TOP, LP_EDGE_DIVIDER);
        y += LP_SPACE_3;
    }
#endif
    lp_graph *g = &a->graph;
    if (g->selected < 0 || g->selected >= g->node_count) {
        paragraph(ctx, LP_RECT(inner.x, y, inner.w, 76), a->graph.mode == LP_GRAPH_3D
                  ? "Click a node to see it, double-click to re-seed the graph on it; drag to turn the graph, Shift+drag to pan; Ctrl+wheel zooms. It turns on its own while nothing is selected."
                  : "Click a node to see it, double-click to re-seed the graph on it, drag to pan; Ctrl+wheel zooms.", LP_INK_TERTIARY, 0);
        return;
    }
    const lp_graph_node *n = &g->nodes[g->selected];
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style hs = lp_text_style_default();
        hs.size_px = LP_TEXT_LG;
        hs.weight = LP_TEXT_WEIGHT_SEMIBOLD;
        hs.ellipsize = 1;
        lp_text_draw(ctx->cr, n->name, LP_RECT(inner.x, y, inner.w, 22), &hs, LP_ALIGN_START);
        cairo_arc(ctx->cr, inner.x + inner.w - 8, y + 11, 5, 0, 2 * M_PI);
        lp_set_color(ctx->cr, lp_graph_kind_color(n->kind));
        cairo_fill(ctx->cr);
    }
    y += 26;
    char v[64];
    row(ctx, inner.x, &y, inner.w, "Kind", n->kind);
    snprintf(v, sizeof v, "%d", n->mentions);
    row(ctx, inner.x, &y, inner.w, "Mentions", v);
    snprintf(v, sizeof v, "%d", n->documents);
    row(ctx, inner.x, &y, inner.w, "Documents", v);
    mono(ctx, LP_RECT(inner.x, y, inner.w, 14), n->id, LP_INK_TERTIARY);
    y += 20;
    /* edges */
    int shown = 0;
    for (int e = 0; e < g->edge_count && shown < 12; e++) {
        if (g->edges[e].a != g->selected && g->edges[e].b != g->selected) continue;
        char line[256];
        snprintf(line, sizeof line, "%s \xE2\x80\x94%s\xE2\x86\x92 %s \xC3\x97%d", g->nodes[g->edges[e].a].name, g->edges[e].predicate, g->nodes[g->edges[e].b].name, g->edges[e].weight);
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            lp_text_style es = lp_text_style_default();
            es.size_px = LP_TEXT_SM;
            es.ellipsize = 1;
            lp_text_draw(ctx->cr, line, LP_RECT(inner.x, y, inner.w, 16), &es, LP_ALIGN_START);
        }
        y += 16;
        shown++;
    }
    y += LP_SPACE_3;
    /* the repair bench */
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_draw_hairline(ctx->cr, LP_RECT(inner.x, y, inner.w, 0), LP_EDGE_TOP, LP_EDGE_DIVIDER);
    y += LP_SPACE_3;
    lp_text_field(ctx, lp_id_index(base, 70), LP_RECT(inner.x, y, inner.w, LP_SIZE_CONTROL_HEIGHT), &a->repair,
                  (lp_text_field_opts){ .placeholder = "A new name, a node to merge into, or a kind" });
    y += LP_SIZE_CONTROL_HEIGHT + LP_SPACE_2;
    static const struct { const char *label; const char *op; } OPS[] = { { "Rename", "rename" }, { "Merge into", "merge" }, { "Set kind", "set_kind" } };
    float bx = inner.x;
    for (int i = 0; i < 3; i++) {
        lp_button_opts o = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, a->repair.text[0] == 0 };
        lp_size s = lp_button_measure(ctx, OPS[i].label, o);
        if (bx + s.w > inner.x + inner.w) { bx = inner.x; y += s.h + LP_SPACE_1; }
        if (lp_button(ctx, lp_id_index(base, 71 + i), LP_RECT(bx, y, s.w, s.h), OPS[i].label, o) && a->repair.text[0]) {
            const char *into = NULL;
            char into_id[100] = "";
            if (i == 1) {
                int k = lp_graph_find(g, a->repair.text);
                if (k >= 0) snprintf(into_id, sizeof into_id, "%s", g->nodes[k].id);
                into = into_id[0] ? into_id : a->repair.text;
            }
            lp_thread_mutate(t, OPS[i].op, n->id, i == 0 ? a->repair.text : NULL, into, i == 2 ? a->repair.text : NULL);
            snprintf(a->status, sizeof a->status, "%s \xE2\x80\x9C%s\xE2\x80\x9D\xE2\x80\xA6", OPS[i].label, n->name);
            ctx->dirty = 1;
        }
        bx += s.w + LP_SPACE_1;
    }
    y += LP_SIZE_CONTROL_HEIGHT_SM + LP_SPACE_2;
    lp_button_opts del = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_TRASH, 0, 0 };
    lp_size ds = lp_button_measure(ctx, "Delete node", del);
    if (lp_button(ctx, lp_id_index(base, 75), LP_RECT(inner.x, y, ds.w, ds.h), "Delete node", del)) {
        lp_thread_mutate(t, "delete_entity", n->id, NULL, NULL, NULL);
        g->selected = -1;
        ctx->dirty = 1;
    }
    lp_button_opts re = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_RELOAD, 0, 0 };
    lp_size rs = lp_button_measure(ctx, "Re-extract", re);
    if (lp_button(ctx, lp_id_index(base, 76), LP_RECT(inner.x + ds.w + LP_SPACE_1, y, rs.w, rs.h), "Re-extract", re)) {
        /* the node's documents are asked to be read again; the graph answer names them */
#ifdef HAVE_JSONC
        struct json_object *answer = lp_thread_answer(t, LP_THREAD_GRAPH), *ents = arr(answer, "entities");
        for (size_t i = 0; i < alen(ents); i++) {
            const char *id = str(at(ents, i), "id");
            if (!id || strcmp(id, n->id) != 0) continue;
            struct json_object *docs = arr(at(ents, i), "document_ids");
            for (size_t k = 0; k < alen(docs) && k < 8; k++) lp_thread_reextract(t, json_object_get_string(at(docs, k)));
        }
#endif
        snprintf(a->status, sizeof a->status, "Re-extracting the documents behind \xE2\x80\x9C%s\xE2\x80\x9D\xE2\x80\xA6", n->name);
        ctx->dirty = 1;
    }
}

static void paint_graph(lp_ctx *ctx, struct thread_app *a, lp_rect area, lp_id base) {
    lp_thread *t = &a->desk->thread;
    lp_rect controls = lp_pane_bar(ctx, &area);
    float x = controls.x, cy = controls.y + controls.h / 2;
    lp_id seed_id = lp_id_index(base, 60);
    lp_rect seed = LP_RECT(x, cy - LP_SIZE_CONTROL_HEIGHT / 2, 220, LP_SIZE_CONTROL_HEIGHT);
    lp_text_field(ctx, seed_id, seed, &a->seed, (lp_text_field_opts){ .placeholder = "Seed: an entity's name", .icon = LP_ICON_SEARCH, .round = 1 });
    if (ctx->pass == LP_PASS_EVENT && ctx->focus == seed_id && ctx->in.key_pressed && ctx->in.keysym == XKB_KEY_Return) {
        a->file_path[0] = 0;
        ask_graph(a);
        ctx->dirty = 1;
    }
    x += 220 + LP_SPACE_3;
    static const lp_segment HOPS[3] = { { "1", LP_ICON_COUNT }, { "2", LP_ICON_COUNT }, { "3", LP_ICON_COUNT } };
    int hop_index = a->hops - 1;
    lp_size hs = lp_segmented_measure(ctx, HOPS, 3, LP_CONTROL_SM);
    if (lp_segmented(ctx, lp_id_index(base, 61), x, cy - hs.h / 2, HOPS, 3, &hop_index, LP_CONTROL_SM)) {
        a->hops = hop_index + 1;
        ask_graph(a);
        ctx->dirty = 1;
    }
    x += hs.w + LP_SPACE_3;
    static const lp_segment DIMS[2] = { { "2D", LP_ICON_COUNT }, { "3D", LP_ICON_COUNT } };
    int dim = a->graph.mode == LP_GRAPH_3D;
    lp_size ds = lp_segmented_measure(ctx, DIMS, 2, LP_CONTROL_SM);
    if (lp_segmented(ctx, lp_id_index(base, 65), x, cy - ds.h / 2, DIMS, 2, &dim, LP_CONTROL_SM)) {
        lp_graph_set_mode(&a->graph, dim ? LP_GRAPH_3D : LP_GRAPH_2D);
        ctx->dirty = 1;
    }
    x += ds.w + LP_SPACE_3;
    if (lp_checkbox(ctx, lp_id_index(base, 62), x, cy - 9, &a->with_documents, "Documents", 0)) {
        ask_graph(a);
        ctx->dirty = 1;
    }
    x += lp_checkbox_measure(ctx, "Documents").w + LP_SPACE_3;
    /* kind chips: the kinds on the canvas */
    char kinds[12][32];
    int nk = 0;
    for (int i = 0; i < a->graph.node_count && nk < 12; i++) {
        int seen = 0;
        for (int k = 0; k < nk; k++) if (strcmp(kinds[k], a->graph.nodes[i].kind) == 0) seen = 1;
        if (!seen) snprintf(kinds[nk++], 32, "%s", a->graph.nodes[i].kind);
    }
    for (int k = 0; k < nk && x < controls.x + controls.w - SIDE_W; k++) {
        lp_size cs;
        int selected = strcmp(a->kind_filter, kinds[k]) == 0;
        if (chip(ctx, lp_id_index(base, 80 + k), x, cy - 9, kinds[k], selected, &cs)) {
            snprintf(a->kind_filter, sizeof a->kind_filter, "%s", selected ? "" : kinds[k]);
            ask_graph(a);
            ctx->dirty = 1;
        }
        x += cs.w + LP_SPACE_1;
    }
    lp_rect side = lp_rect_cut_right(&area, SIDE_W);
    int selection_changed = 0;
    int reseed = lp_graph_widget(ctx, lp_id_index(base, 64), area, &a->graph, &selection_changed);
    if (reseed >= 0) {
        lp_text_buffer_set(&a->seed, a->graph.nodes[reseed].name);
        a->file_path[0] = 0;
        ask_graph(a);
        ctx->dirty = 1;
    }
    if (selection_changed) a->repair.text[0] = 0;
    paint_node_card(ctx, a, side, base);
    if (!lp_thread_connected(t) && ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style ss = lp_text_style_default();
        ss.color = LP_INK_TERTIARY;
        lp_text_draw(ctx->cr, "threadd is not running.", area, &ss, LP_ALIGN_CENTER);
    }
}

/* MARK: - Schemas */

static void paint_schemas(lp_ctx *ctx, struct thread_app *a, lp_rect area, lp_id base) {
#ifdef HAVE_JSONC
    lp_thread *t = &a->desk->thread;
    struct json_object *schemas = lp_thread_answer(t, LP_THREAD_SCHEMAS), *families = arr(schemas, "families");
    lp_rect inner = lp_pane_content(area);
    float card_h = 0;
    for (size_t i = 0; i < alen(families); i++) card_h += LP_PANE_CARD_TITLE_H + LP_PANE_CARD_PAD + LP_PANE_CARD_GAP + 8 * LP_PANE_ROW + (float)alen(arr(at(families, i), "fields")) * 16 + 44;
    lp_size extent = { inner.w, card_h > inner.h ? card_h : inner.h };
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 90), inner, extent, &a->scroll[TAB_SCHEMAS]);
    for (size_t i = 0; i < alen(families); i++) {
        struct json_object *f = at(families, i), *fields = arr(f, "fields");
        char title[96], v[96], last[48];
        snprintf(title, sizeof title, "%s \xE2\x80\x94 %s", or_dash(str(f, "name")), or_dash(str(f, "label")));
        float y = card(ctx, &c, title, 8 * LP_PANE_ROW + (float)alen(fields) * 16 + 44);
        float x = c.x + CARD_PAD, w = c.w - 2 * CARD_PAD;
        paragraph(ctx, LP_RECT(x, y, w, 36), or_dash(str(f, "description")), LP_INK_SECONDARY, 0);
        y += 40;
        row(ctx, x, &y, w, "Lane", or_dash(str(f, "lane")));
        row(ctx, x, &y, w, "Written by", or_dash(str(f, "writer")));
        snprintf(v, sizeof v, "%s\xE2\x80\xA6", or_dash(str(f, "id_prefix")));
        row(ctx, x, &y, w, "Document ids", v);
        snprintf(v, sizeof v, "%s\xE2\x80\xA6", or_dash(str(f, "group_prefix")));
        row(ctx, x, &y, w, "Groups", v);
        thousands(num(f, "count"), v, sizeof v);
        row(ctx, x, &y, w, "Records", v);
        when(num(f, "last_written_ms"), last, sizeof last);
        row(ctx, x, &y, w, "Last written", last);
        row(ctx, x, &y, w, "Graph", or_dash(str(f, "graph")));
        row(ctx, x, &y, w, "Fields", alen(fields) ? "" : "—");
        for (size_t k = 0; k < alen(fields); k++) {
            struct json_object *field = at(fields, k);
            char line[240];
            snprintf(line, sizeof line, "%s: %s \xE2\x80\x94 %s", or_dash(str(field, "name")), or_dash(str(field, "type")), or_dash(str(field, "description")));
            mono(ctx, LP_RECT(x + LP_PANE_LABEL_W + LP_PANE_GUTTER, y, w - LP_PANE_LABEL_W - LP_PANE_GUTTER, 14), line, LP_INK_SECONDARY);
            y += 16;
        }
    }
    if (!alen(families) && ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style es = lp_text_style_default();
        es.color = LP_INK_TERTIARY;
        lp_text_draw(ctx->cr, lp_thread_connected(t) ? "Reading the schemas…" : "threadd is not running.", inner, &es, LP_ALIGN_CENTER);
    }
    lp_scroll_end(ctx);
#else
    (void)ctx; (void)a; (void)area; (void)base;
#endif
}

/* MARK: - Ledger and Retrieval */

static const char *const LEDGER_KINDS[] = { "deposit", "index", "search", "remove", "embed", "extract", "reconcile", "mutation" };

static void paint_ledger(lp_ctx *ctx, struct thread_app *a, lp_rect area, lp_id base, int retrieval) {
#ifdef HAVE_JSONC
    lp_thread *t = &a->desk->thread;
    lp_rect inner = lp_pane_content(area);
    float y = inner.y;
    if (!retrieval) {
        float x = inner.x;
        lp_size cs;
        if (chip(ctx, lp_id_index(base, 110), x, y, "All", a->ledger_kind[0] == 0, &cs)) { a->ledger_kind[0] = 0; ask_tab(a, TAB_LEDGER); ctx->dirty = 1; }
        x += cs.w + LP_SPACE_1;
        for (size_t k = 0; k < sizeof LEDGER_KINDS / sizeof LEDGER_KINDS[0]; k++) {
            int selected = strcmp(a->ledger_kind, LEDGER_KINDS[k]) == 0;
            if (chip(ctx, lp_id_index(base, 111 + (int)k), x, y, LEDGER_KINDS[k], selected, &cs)) {
                snprintf(a->ledger_kind, sizeof a->ledger_kind, "%s", selected ? "" : LEDGER_KINDS[k]);
                ask_tab(a, TAB_LEDGER);
                ctx->dirty = 1;
            }
            x += cs.w + LP_SPACE_1;
        }
        y += cs.h + LP_SPACE_3;
    }
    struct json_object *ledger = lp_thread_answer(t, LP_THREAD_LEDGER), *rows = arr(ledger, "rows");
    lp_rect list = LP_RECT(inner.x, y, inner.w, inner.y + inner.h - y);
    float extent_h = 0;
    for (size_t i = 0; i < alen(rows); i++) {
        const char *kind = str(at(rows, i), "kind");
        if (retrieval && (!kind || strcmp(kind, "search") != 0)) continue;
        extent_h += retrieval ? 44 + (float)alen(arr(at(rows, i), "documents")) * 16 + 8 : LP_LIST_ROW_H;
    }
    lp_size extent = { list.w, extent_h > list.h ? extent_h : list.h };
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(base, 130), list, extent, &a->scroll[retrieval ? TAB_RETRIEVAL : TAB_LEDGER]);
    float ry = c.y;
    for (size_t i = 0; i < alen(rows); i++) {
        struct json_object *r = at(rows, i);
        const char *row_kind = str(r, "kind");
        if (retrieval && (!row_kind || strcmp(row_kind, "search") != 0)) continue;   /* the Retrieval tab shows searches only */
        char ago[32], count[32], ms[32];
        age(num(r, "at_ms"), ago, sizeof ago);
        snprintf(count, sizeof count, "%lld", (long long)num(r, "count"));
        snprintf(ms, sizeof ms, "%lld ms", (long long)num(r, "ms"));
        if (!retrieval) {
            const char *cols[5] = { or_dash(str(r, "source")), or_dash(str(r, "document_id")), count, ms, ago };
            lp_list_row(ctx, lp_id_index(base, 200 + (int)i), LP_RECT(c.x, ry, c.w, LP_LIST_ROW_H), LP_ICON_COUNT, or_dash(str(r, "kind")), cols, 5, 0, (int)i % 2);
            ry += LP_LIST_ROW_H;
            continue;
        }
        /* a search: its request, then what came back in rank order */
        struct json_object *hits = arr(r, "documents");
        lp_rect card_r = LP_RECT(c.x, ry, c.w, 40 + (float)alen(hits) * 16);
        if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
            lp_fill_solid(ctx->cr, card_r, LP_SURFACE_WELL, LP_RADIUS_SM);
            char head[240];
            const char *request_id = str(r, "request_id"), *detail = str(r, "detail");
            snprintf(head, sizeof head, "%s · %s · %zu returned in %s · %s", or_dash(str(r, "source")), request_id && *request_id ? request_id : "no request id",
                     alen(hits), ms, ago);
            lp_text_style hs = lp_text_style_default();
            hs.size_px = LP_TEXT_SM;
            hs.weight = LP_TEXT_WEIGHT_SEMIBOLD;
            hs.ellipsize = 1;
            lp_text_draw(ctx->cr, head, LP_RECT(card_r.x + LP_SPACE_3, card_r.y + 4, card_r.w - 2 * LP_SPACE_3, 16), &hs, LP_ALIGN_START);
            lp_text_style ds = lp_text_style_default();
            ds.size_px = LP_TEXT_XS;
            ds.color = LP_INK_TERTIARY;
            ds.ellipsize = 1;
            lp_text_draw(ctx->cr, detail && *detail ? detail : (alen(hits) ? "" : "asked nothing back"), LP_RECT(card_r.x + LP_SPACE_3, card_r.y + 20, card_r.w - 2 * LP_SPACE_3, 14), &ds, LP_ALIGN_START);
        }
        for (size_t k = 0; k < alen(hits); k++) {
            struct json_object *h = at(hits, k);
            char line[240];
            struct json_object *sv;
            double score = json_object_object_get_ex(h, "score", &sv) ? json_object_get_double(sv) : 0;
            snprintf(line, sizeof line, "%lld  %.3f  %s", (long long)num(h, "rank"), score, or_dash(str(h, "document_id")));
            lp_rect hr = LP_RECT(card_r.x + LP_SPACE_4, card_r.y + 38 + (float)k * 16, card_r.w - 2 * LP_SPACE_4, 16);
            mono(ctx, hr, line, lp_is_hot(ctx, lp_id_index(base, 300 + (int)i * 16 + (int)k)) ? LP_ACCENT_BLUE_DEEP : LP_INK_SECONDARY);
            lp_hot(ctx, lp_id_index(base, 300 + (int)i * 16 + (int)k), hr);
            if (lp_clicked(ctx, lp_id_index(base, 300 + (int)i * 16 + (int)k), hr)) {
                const char *did = str(h, "document_id");
                if (did) {
                    snprintf(a->open_document, sizeof a->open_document, "%s", did);
                    const char *ids[1] = { did };
                    lp_thread_documents(t, ids, 1);
                    set_tab(a, TAB_LIBRARY);
                    ctx->dirty = 1;
                }
            }
        }
        ry += card_r.h + 8;
    }
    if (!alen(rows) && ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style es = lp_text_style_default();
        es.color = LP_INK_TERTIARY;
        lp_text_draw(ctx->cr, ledger ? (retrieval ? "No search has run yet." : "Nothing in the ledger yet.") : lp_thread_connected(t) ? "Reading…" : "threadd is not running.",
                     list, &es, LP_ALIGN_CENTER);
    }
    lp_scroll_end(ctx);
#else
    (void)ctx; (void)a; (void)area; (void)base; (void)retrieval;
#endif
}

/* MARK: - The window */

static const char *const SECTION_TITLES[TABS] = { "Drive", "Library", "Graph", "Schemas", "Ledger", "Retrieval" };
static const lp_icon SECTION_ICONS[TABS] = { LP_ICON_DRIVE, LP_ICON_FOLDER, LP_ICON_CLOUD, LP_ICON_CODE, LP_ICON_LIST, LP_ICON_SEARCH };

/* The header's second line: what the section holds right now. */
static void subtitle_of(struct thread_app *a, char *out, size_t n) {
    lp_thread *t = &a->desk->thread;
    if (!lp_thread_connected(t)) { snprintf(out, n, "threadd is not running"); return; }
#ifdef HAVE_JSONC
    char v1[48], v2[48];
    switch (a->tab) {
    case TAB_DRIVE: {
        struct json_object *parity = lp_thread_answer(t, LP_THREAD_PARITY);
        if (parity && num(parity, "run_id")) {
            thousands(num(parity, "recorded"), v1, sizeof v1);
            thousands(num(parity, "seen"), v2, sizeof v2);
            snprintf(out, n, "%s \xC2\xB7 %s of %s files in the graph", a->desk->branding.name[0] ? a->desk->branding.name : "MaryOS", v1, v2);
        } else snprintf(out, n, "%s \xC2\xB7 no reconcile has run yet", a->desk->branding.name[0] ? a->desk->branding.name : "MaryOS");
        return;
    }
    case TAB_LIBRARY: {
        struct json_object *groups = arr(lp_thread_answer(t, LP_THREAD_LIBRARY), "groups");
        size_t records = 0;
        for (size_t g = 0; g < alen(groups); g++) records += alen(arr(at(groups, g), "documents"));
        thousands((int64_t)records, v1, sizeof v1);
        snprintf(out, n, "%zu group%s \xC2\xB7 %s record%s", alen(groups), alen(groups) == 1 ? "" : "s", v1, records == 1 ? "" : "s");
        return;
    }
    case TAB_GRAPH: snprintf(out, n, "%s", totals_text(&a->graph)); return;
    case TAB_SCHEMAS: {
        size_t families = alen(arr(lp_thread_answer(t, LP_THREAD_SCHEMAS), "families"));
        snprintf(out, n, "%zu record famil%s, declared once and stamped on every document", families, families == 1 ? "y" : "ies");
        return;
    }
    case TAB_LEDGER: {
        size_t rows = alen(arr(lp_thread_answer(t, LP_THREAD_LEDGER), "rows"));
        snprintf(out, n, "%zu event%s%s%s", rows, rows == 1 ? "" : "s", a->ledger_kind[0] ? " \xC2\xB7 only " : "", a->ledger_kind[0] ? a->ledger_kind : "");
        return;
    }
    default: {
        struct json_object *rows = arr(lp_thread_answer(t, LP_THREAD_LEDGER), "rows");
        size_t searches = 0;
        for (size_t i = 0; i < alen(rows); i++) searches += str(at(rows, i), "kind") && strcmp(str(at(rows, i), "kind"), "search") == 0;
        snprintf(out, n, "%zu search%s, newest first, with what each returned", searches, searches == 1 ? "" : "es");
        return;
    }
    }
#else
    snprintf(out, n, "Built without json-c");
#endif
}

static void thread_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct thread_app empty;
    struct thread_app *a = state ? state : &empty;
    if (!a->desk) a->desk = d;
    lp_id base = LP_ID("thread");
    if (state && !a->asked && lp_thread_connected(&d->thread)) ask_all(a);
    /* the sections, as a source list */
    lp_rect area = body;
    lp_rect cursor = lp_sidebar(ctx, &area);
    lp_sidebar_section(ctx, &cursor, "Threads");
    for (int i = 0; i < TABS; i++)
        if (lp_sidebar_item(ctx, lp_id_index(base, 200 + i), &cursor, SECTION_ICONS[i], SECTION_TITLES[i], a->tab == i) && a->tab != i) { set_tab(a, i); ctx->dirty = 1; }
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_fill_solid(ctx->cr, area, LP_SURFACE_BODY, 0);
    /* the header: the section, what it holds, threadd's dot; Reload (and Reconcile on the Drive) at its right */
    char subtitle[240];
    subtitle_of(a, subtitle, sizeof subtitle);
    int connected = lp_thread_connected(&d->thread);
    lp_pane_header h = { .title = SECTION_TITLES[a->tab], .subtitle = a->status[0] ? a->status : subtitle, .live = connected, .status = connected ? "threadd" : "threadd is away" };
    lp_rect controls = lp_pane_header_paint(ctx, &area, &h);
    lp_button_opts ro = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_RELOAD, 1, 0 };
    lp_size rs = lp_button_measure(ctx, "", ro);
    float right = controls.x + controls.w, cy = controls.y + controls.h / 2;
    if (lp_button(ctx, lp_id_index(base, 2), LP_RECT(right - rs.w, cy - rs.h / 2, rs.w, rs.h), "", ro)) {
        a->status[0] = 0;
        ask_all(a);
        ctx->dirty = 1;
    }
    right -= rs.w + LP_SPACE_2;
    if (a->tab == TAB_DRIVE) {
        lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, a->job != NULL || !a->run };
        lp_size bs = lp_button_measure(ctx, "Reconcile", bo);
        if (lp_button(ctx, lp_id_index(base, 11), LP_RECT(right - bs.w, cy - bs.h / 2, bs.w, bs.h), "Reconcile", bo)) {
            reconcile(a);
            ctx->dirty = 1;
        }
    }
    switch (a->tab) {
    case TAB_DRIVE: paint_drive(ctx, a, area, base); break;
    case TAB_LIBRARY: paint_library(ctx, a, area, base); break;
    case TAB_GRAPH: paint_graph(ctx, a, area, base); break;
    case TAB_SCHEMAS: paint_schemas(ctx, a, area, base); break;
    case TAB_LEDGER: paint_ledger(ctx, a, area, base, 0); break;
    default: paint_ledger(ctx, a, area, base, 1); break;
    }
}

static void thread_open(void *state, lp_desktop *d, const char *path) {
    struct thread_app *a = state;
    if (!a || !path) return;
    for (int i = 0; i < TABS; i++) {
        if (strcmp(path, TAB_NAMES[i]) == 0) {
            set_tab(a, i);
            return;
        }
    }
    if (strncmp(path, "graph:file=", 11) == 0) {
        snprintf(a->file_path, sizeof a->file_path, "%s", path + 11);
        a->tab = TAB_GRAPH;
        lp_text_buffer_set(&a->seed, "");
        lp_thread_file_record(&d->thread, a->file_path);
        return;
    }
    if (strcmp(path, "graph3d") == 0) {
        lp_graph_set_mode(&a->graph, LP_GRAPH_3D);
        set_tab(a, TAB_GRAPH);
        return;
    }
    if (strncmp(path, "graph:", 6) == 0) {
        lp_text_buffer_set(&a->seed, path + 6);
        a->file_path[0] = 0;
        set_tab(a, TAB_GRAPH);
        return;
    }
    if (strncmp(path, "document:", 9) == 0) {
        snprintf(a->open_document, sizeof a->open_document, "%s", path + 9);
        const char *ids[1] = { a->open_document };
        lp_thread_documents(&d->thread, ids, 1);
        set_tab(a, TAB_LIBRARY);
    }
}

static int thread_model_changed(void *state, lp_desktop *d, unsigned model, unsigned what) {
    struct thread_app *a = state;
    if (model != LP_MODEL_THREAD || !a) return 0;
    lp_thread *t = &d->thread;
    if ((what & LP_THREAD_CHANGED_CONNECTION) && lp_thread_connected(t) && !a->asked) ask_all(a);
    if (what & LP_THREAD_CHANGED_GRAPH) lp_graph_load(&a->graph, lp_thread_answer(t, LP_THREAD_GRAPH), 1);
    if (what & LP_THREAD_CHANGED_MUTATION) {
        snprintf(a->status, sizeof a->status, "Done.");
        ask_graph(a);
    }
    if (what & LP_THREAD_CHANGED_ERROR) snprintf(a->status, sizeof a->status, "threadd: %s", t->error);
#ifdef HAVE_JSONC
    if ((what & LP_THREAD_CHANGED_FILE) && a->file_path[0] && strcmp(t->file_path, a->file_path) == 0) {
        /* the file's node: seed the graph on it */
        struct json_object *record = lp_thread_answer(t, LP_THREAD_FILE_RECORD), *ents = arr(record, "entities");
        const char *name = NULL;
        for (size_t i = 0; i < alen(ents) && !name; i++) {
            const char *kind = str(at(ents, i), "kind");
            if (kind && strcmp(kind, "file") == 0) name = str(at(ents, i), "name");
        }
        if (!name && alen(ents)) name = str(at(ents, 0), "name");
        if (name) {
            lp_text_buffer_set(&a->seed, name);
            a->with_documents = 1;
            ask_graph(a);
        }
    }
#endif
    return 1;
}

static void entry(lp_menu_model *m, const char *label, const char *shortcut, int arg, int checked) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "%s", label);
    e->shortcut = shortcut;
    e->command = LP_CMD_APP;
    e->arg = arg;
    e->checked = checked;
}

static void thread_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    struct thread_app *a = state;
    if (!a || menu != LP_MENU_VIEW) return;
    static const char *const LABELS[TABS] = { "Drive", "Library", "Graph", "Schemas", "Ledger", "Retrieval" };
    static const char *const KEYS[TABS] = { "⌘1", "⌘2", "⌘3", "⌘4", "⌘5", "⌘6" };
    for (int i = 0; i < TABS; i++) entry(m, LABELS[i], KEYS[i], LP_THREAD_TAB_DRIVE + i, a->tab == i);
    if (m->count < LP_MENU_MAX_ENTRIES) { memset(&m->entries[m->count], 0, sizeof m->entries[0]); m->entries[m->count++].separator = 1; }
    entry(m, "Reload", "⌘R", LP_THREAD_RELOAD, 0);
    entry(m, "Reconcile with the Disk", NULL, LP_THREAD_RECONCILE, 0);
}

static void thread_command(void *state, lp_desktop *d, int cmd) {
    struct thread_app *a = state;
    if (!a) return;
    if (cmd >= LP_THREAD_TAB_DRIVE && cmd < LP_THREAD_TAB_DRIVE + TABS) set_tab(a, cmd - LP_THREAD_TAB_DRIVE);
    else if (cmd == LP_THREAD_RELOAD) ask_all(a);
    else if (cmd == LP_THREAD_RECONCILE) reconcile(a);
}

static void *thread_create(lp_desktop *d, const char *window_id) {
    struct thread_app *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    snprintf(a->window_id, sizeof a->window_id, "%s", window_id);
    a->desk = d;
    a->hops = 1;
    a->with_documents = 0;
    a->run = lp_job_run;
    lp_graph_init(&a->graph);
    lp_text_buffer_set(&a->seed, "");
    lp_text_buffer_set(&a->repair, "");
    if (d) {
        a->timer = lp_desktop_add_timer(d, REFRESH_MS, on_tick, a);
        if (lp_thread_connected(&d->thread)) ask_all(a);
        if (lp_thread_answer(&d->thread, LP_THREAD_GRAPH)) lp_graph_load(&a->graph, lp_thread_answer(&d->thread, LP_THREAD_GRAPH), 0);
    }
    return a;
}

static void thread_destroy(void *state) {
    struct thread_app *a = state;
    if (!a) return;
    if (a->timer) lp_desktop_remove_source(a->desk, a->timer);
    lp_graph_free(&a->graph);
    free(a);
}

const lp_app lp_app_thread = {
    .id = "thread", .title = "Threads", .name = "Threads", .aka = "Thread", .icon = LP_ICON_DRIVE, .object = "volumeInternal", .dock = 1,
    .default_rect = { NAN, NAN, 900, 600 }, .min_size = { 640, 420 }, .singleton = 1, .resizable = 1,
    .create = thread_create, .paint = thread_paint, .destroy = thread_destroy, .open = thread_open,
    .command = thread_command, .menu_entries = thread_menu_entries, .model_changed = thread_model_changed,
};

/* MARK: - Tests and renders */

int lp_thread_app_tab(const void *state) { return ((const struct thread_app *)state)->tab; }
void lp_thread_app_set_tab(void *state, int tab) { set_tab(state, tab); }
const char *lp_thread_app_seed(const void *state) { return ((const struct thread_app *)state)->seed.text; }
const char *lp_thread_app_file(const void *state) { return ((const struct thread_app *)state)->file_path; }
const char *lp_thread_app_document(const void *state) { return ((const struct thread_app *)state)->open_document; }
const char *lp_thread_app_status(const void *state) { return ((const struct thread_app *)state)->status; }
int lp_thread_app_graph_nodes(const void *state) { return ((const struct thread_app *)state)->graph.node_count; }
int lp_thread_app_graph_selected(const void *state) { return ((const struct thread_app *)state)->graph.selected; }
int lp_thread_app_graph_mode(const void *state) { return ((const struct thread_app *)state)->graph.mode; }
void lp_thread_app_set_runner(void *state, lp_job *(*run)(lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user)) {
    ((struct thread_app *)state)->run = run;
}
