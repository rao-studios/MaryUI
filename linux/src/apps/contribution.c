/* "From the thread" — what a highlighted passage drew on (Mary's ContributionInspectorSheet,
 * PARITY D27): the owner's contribution (royalty, documents, passages, the thread id) and its
 * sources by influence, each with the group it lives in, its share as a bar, a three-line
 * italic preview read from threadd, and a way into the Threads app. Internal: opened by
 * Spotlight on a tap with "m<message>:o<owner>". */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_monogram.h"
#include "maryui/lp_brush.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_thread.h"
#include "maryui/lp_tokens.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

#define SOURCES_MAX 12
#define PREVIEW_MAX 300

struct source {
    char id[200];
    char name[128];
    char group[64];
    float influence;
    char preview[PREVIEW_MAX + 4];
};

struct contribution {
    char window_id[12];
    lp_desktop *desk;
    int message, owner;         /* which reply, which of its owners; -1 until opened */
    char thread_id[48], owner_id[64];
    float royalty;
    int documents, passages;
    struct source sources[SOURCES_MAX];
    int source_count;
    int asked;                  /* documents{ids} went out */
    lp_scroll_state scroll;
};

static void *contribution_create(lp_desktop *d, const char *window_id) {
    struct contribution *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    snprintf(c->window_id, sizeof c->window_id, "%s", window_id);
    c->desk = d;
    c->message = c->owner = -1;
    return c;
}

static void contribution_destroy(void *state) { free(state); }

#ifdef HAVE_JSONC
static const char *str(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string) ? json_object_get_string(v) : NULL;
}

/* The previews, once threadd answers documents{ids}. */
static void read_previews(struct contribution *c) {
    struct json_object *docs = lp_thread_answer(&c->desk->thread, LP_THREAD_DOCUMENTS), *list, *v;
    if (!docs || !json_object_object_get_ex(docs, "documents", &list)) return;
    for (size_t i = 0; i < json_object_array_length(list); i++) {
        struct json_object *doc = json_object_array_get_idx(list, i);
        const char *id = str(doc, "id");
        for (int k = 0; id && k < c->source_count; k++) {
            if (strcmp(c->sources[k].id, id) != 0) continue;
            const char *name = str(doc, "name"), *label = str(doc, "group_label");
            if (name && *name) snprintf(c->sources[k].name, sizeof c->sources[k].name, "%s", name);
            if (label && *label) snprintf(c->sources[k].group, sizeof c->sources[k].group, "%s", label);
            if (json_object_object_get_ex(doc, "texts", &v) && json_object_is_type(v, json_type_array) && json_object_array_length(v)) {
                const char *text = json_object_get_string(json_object_array_get_idx(v, 0));
                size_t n = text ? strlen(text) : 0;
                if (n > PREVIEW_MAX) {
                    n = PREVIEW_MAX;
                    while (n && ((unsigned char)text[n] & 0xC0) == 0x80) n--;
                }
                snprintf(c->sources[k].preview, sizeof c->sources[k].preview, "%.*s%s", (int)n, text ? text : "", text && strlen(text) > n ? "\xE2\x80\xA6" : "");
            }
        }
    }
}
#endif

/* "m<message>:o<owner>": the reply and the owner, read from the desktop's conversation. */
static void contribution_open(void *state, lp_desktop *d, const char *path) {
    struct contribution *c = state;
    int message = -1, owner = -1;
    if (!c || !path || sscanf(path, "m%d:o%d", &message, &owner) != 2) return;
    if (message < 0 || message >= d->mary.message_count) return;
    const lp_mary_message *msg = &d->mary.messages[message];
    if (owner < 0 || owner >= msg->owner_count) return;
    c->message = message;
    c->owner = owner;
    c->source_count = 0;
    c->asked = 0;
    const lp_mary_owner *o = &msg->owners[owner];
    snprintf(c->thread_id, sizeof c->thread_id, "%s", o->thread_id);
    snprintf(c->owner_id, sizeof c->owner_id, "%s", o->owner_id);
    c->royalty = o->royalty;
    c->documents = o->documents;
    c->passages = 0;
    for (int i = 0; i < msg->span_count; i++) if (msg->spans[i].owner == owner) c->passages++;
#ifdef HAVE_JSONC
    struct json_object *contribution = msg->contribution, *owners, *entry = NULL, *ids, *influence, *v;
    if (contribution && json_object_object_get_ex(contribution, "owners", &owners) && json_object_is_type(owners, json_type_array) &&
        (size_t)owner < json_object_array_length(owners))
        entry = json_object_array_get_idx(owners, owner);
    if (entry && json_object_object_get_ex(entry, "document_ids", &ids) && json_object_is_type(ids, json_type_array)) {
        json_object_object_get_ex(entry, "influence", &influence);
        for (size_t i = 0; i < json_object_array_length(ids) && c->source_count < SOURCES_MAX; i++) {
            const char *id = json_object_get_string(json_object_array_get_idx(ids, i));
            if (!id) continue;
            struct source *src = &c->sources[c->source_count++];
            memset(src, 0, sizeof *src);
            snprintf(src->id, sizeof src->id, "%s", id);
            snprintf(src->name, sizeof src->name, "Document %.8s\xE2\x80\xA6", id);
            src->influence = influence && json_object_object_get_ex(influence, id, &v) ? (float)json_object_get_double(v) : 0;
            /* the retrieved list knows the name, group and family */
            struct json_object *retrieved = msg->retrieved;
            for (size_t k = 0; retrieved && k < json_object_array_length(retrieved); k++) {
                struct json_object *r = json_object_array_get_idx(retrieved, k);
                const char *rid = str(r, "document_id");
                if (!rid || strcmp(rid, id) != 0) continue;
                const char *name = str(r, "name"), *group = str(r, "group_id"), *family = str(r, "family");
                if (name && *name) snprintf(src->name, sizeof src->name, "%s", name);
                snprintf(src->group, sizeof src->group, "%s", family && *family ? family : group ? group : "");
            }
        }
    }
    /* by influence, the way the sheet lists them */
    for (int i = 1; i < c->source_count; i++)
        for (int k = i; k > 0 && c->sources[k].influence > c->sources[k - 1].influence; k--) {
            struct source t = c->sources[k];
            c->sources[k] = c->sources[k - 1];
            c->sources[k - 1] = t;
        }
    if (c->source_count && lp_thread_connected(&d->thread)) {
        const char *ids_v[SOURCES_MAX];
        for (int i = 0; i < c->source_count; i++) ids_v[i] = c->sources[i].id;
        c->asked = lp_thread_documents(&d->thread, ids_v, c->source_count) == 0;
    }
#endif
}

static int contribution_model_changed(void *state, lp_desktop *d, unsigned model, unsigned what) {
    struct contribution *c = state;
    if (!c || model != LP_MODEL_THREAD) return 0;
#ifdef HAVE_JSONC
    if ((what & LP_THREAD_CHANGED_CONNECTION) && lp_thread_connected(&d->thread) && !c->asked && c->source_count) {
        const char *ids_v[SOURCES_MAX];
        for (int i = 0; i < c->source_count; i++) ids_v[i] = c->sources[i].id;
        c->asked = lp_thread_documents(&d->thread, ids_v, c->source_count) == 0;
        return 0;
    }
    if (!(what & LP_THREAD_CHANGED_DOCUMENTS)) return 0;
    read_previews(c);
    return 1;
#else
    return 0;
#endif
}

static void section(lp_ctx *ctx, lp_rect r, const char *label) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style ts = lp_text_style_default();
    ts.size_px = LP_TEXT_XS;
    ts.weight = LP_TEXT_WEIGHT_SEMIBOLD;
    ts.uppercase = 1;
    ts.letter_spacing = 0.8f;
    ts.color = LP_INK_TERTIARY;
    lp_text_draw(ctx->cr, label, r, &ts, LP_ALIGN_START);
}

static void stat(lp_ctx *ctx, float x, float y, const char *value, const char *label) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style vs = lp_text_style_default();
    vs.font = LP_FONT_DISPLAY;
    vs.size_px = 16;
    vs.weight = LP_TEXT_WEIGHT_MEDIUM;
    lp_text_draw(ctx->cr, value, LP_RECT(x, y, 100, 20), &vs, LP_ALIGN_START);
    lp_text_style ls = lp_text_style_default();
    ls.size_px = 10;
    ls.color = LP_INK_TERTIARY;
    lp_text_draw(ctx->cr, label, LP_RECT(x, y + 22, 100, 14), &ls, LP_ALIGN_START);
}

static void contribution_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct contribution preview;
    struct contribution *c = state ? state : &preview;
    if (!state && !preview.source_count) {
        snprintf(preview.thread_id, sizeof preview.thread_id, "6b0c7a2e-1f4d-4a8e-9c3b-2d5e7f8a9b0c");
        preview.royalty = 0.62f;
        preview.documents = 2;
        preview.passages = 3;
        preview.source_count = 2;
        snprintf(preview.sources[0].id, 200, "file-2b3c4d5e6f7a8b9c");
        snprintf(preview.sources[0].name, 128, "notes.txt");
        snprintf(preview.sources[0].group, 64, "file");
        preview.sources[0].influence = 0.7f;
        snprintf(preview.sources[0].preview, PREVIEW_MAX, "Paris is the capital of France, a note I keep for the spring trip. The Marais for the hotel, April for the dates.");
        snprintf(preview.sources[1].id, 200, "1122334455667788990011");
        snprintf(preview.sources[1].name, 128, "Paris in spring");
        snprintf(preview.sources[1].group, 64, "memory");
        preview.sources[1].influence = 0.3f;
        snprintf(preview.sources[1].preview, PREVIEW_MAX, "The user planned a spring trip to Paris with two friends.");
        preview.owner = 0;
    }
    lp_id base = LP_ID("contribution");
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    if (draw) lp_fill_solid(cr, body, LP_SURFACE_BODY, 0);
    lp_rect area = lp_rect_inset(body, LP_SPACE_4, LP_SPACE_4);
    /* the header: the mark and the title */
    lp_rect head = lp_rect_cut_top(&area, 28);
    if (draw) {
        lp_monogram_paint(cr, LP_RECT(head.x, head.y + 5, 18, 18), LP_MONOGRAM_FLAT, LP_INK_PRIMARY);
        lp_text_style ts = lp_text_style_default();
        ts.font = LP_FONT_DISPLAY;
        ts.italic = 1;
        ts.size_px = 18;
        lp_text_draw(cr, "From the thread", LP_RECT(head.x + 26, head.y, head.w - 26, head.h), &ts, LP_ALIGN_START);
    }
    area.y += LP_SPACE_3;
    if (c->owner < 0 && state) {
        if (draw) {
            lp_text_style es = lp_text_style_default();
            es.color = LP_INK_TERTIARY;
            lp_text_draw(cr, "Tap a highlighted passage in Mary's reply.", area, &es, LP_ALIGN_CENTER);
        }
        return;
    }
    /* Contribution */
    lp_rect card = lp_rect_cut_top(&area, 96);
    if (draw) {
        lp_fill_solid(cr, card, LP_SURFACE_WELL, LP_RADIUS_MD);
        lp_draw_hairline(cr, card, LP_EDGE_TOP | LP_EDGE_BOTTOM | LP_EDGE_LEFT | LP_EDGE_RIGHT, LP_EDGE_DIVIDER);
    }
    section(ctx, LP_RECT(card.x + LP_SPACE_3, card.y + LP_SPACE_2, card.w, 14), "Contribution");
    char royalty[16], docs[16], passages[16];
    snprintf(royalty, sizeof royalty, "%.0f%%", c->royalty * 100);
    snprintf(docs, sizeof docs, "%d", c->documents ? c->documents : c->source_count);
    snprintf(passages, sizeof passages, "%d", c->passages);
    stat(ctx, card.x + LP_SPACE_3, card.y + 30, royalty, "Royalty");
    stat(ctx, card.x + LP_SPACE_3 + 110, card.y + 30, docs, "Documents");
    stat(ctx, card.x + LP_SPACE_3 + 220, card.y + 30, passages, "Passages");
    if (draw) {
        char thread[80];
        snprintf(thread, sizeof thread, "Thread %s", c->thread_id[0] ? c->thread_id : "local");
        lp_text_style ms = lp_text_style_default();
        ms.font = LP_FONT_MONO;
        ms.size_px = 10;
        ms.color = lp_color_with_alpha(LP_INK_PRIMARY, 0.4f);
        ms.ellipsize = 1;
        lp_text_draw(cr, thread, LP_RECT(card.x + LP_SPACE_3, card.y + 72, card.w - 2 * LP_SPACE_3, 14), &ms, LP_ALIGN_START);
    }
    area.y += LP_SPACE_3;
    /* Sources */
    lp_rect sources = area;
    if (draw) {
        lp_fill_solid(cr, sources, LP_SURFACE_WELL, LP_RADIUS_MD);
        lp_draw_hairline(cr, sources, LP_EDGE_TOP | LP_EDGE_BOTTOM | LP_EDGE_LEFT | LP_EDGE_RIGHT, LP_EDGE_DIVIDER);
    }
    section(ctx, LP_RECT(sources.x + LP_SPACE_3, sources.y + LP_SPACE_2, sources.w, 14), "Sources");
    lp_rect list = LP_RECT(sources.x + LP_SPACE_3, sources.y + 28, sources.w - 2 * LP_SPACE_3, sources.h - 28 - LP_SPACE_2);
    float row_h = 24 + 8 + 4 + 8 + 48 + 8;
    lp_size extent = { list.w, c->source_count * row_h };
    lp_rect col = lp_scroll_begin(ctx, lp_id_index(base, 5), list, extent, &c->scroll);
    float y = col.y;
    if (!c->source_count && draw) {
        lp_text_style es = lp_text_style_default();
        es.size_px = 11;
        es.color = lp_color_with_alpha(LP_INK_PRIMARY, 0.5f);
        lp_text_draw(cr, "No document detail available.", LP_RECT(col.x, y, col.w, 20), &es, LP_ALIGN_START);
    }
    lp_color gold = lp_brush_palette(0);
    for (int i = 0; i < c->source_count; i++) {
        const struct source *src = &c->sources[i];
        if (draw) {
            lp_text_style ns = lp_text_style_default();
            ns.size_px = 12;
            ns.weight = LP_TEXT_WEIGHT_MEDIUM;
            ns.ellipsize = 1;
            float name_w = col.w - 150;
            lp_text_draw(cr, src->name, LP_RECT(col.x, y + 2, name_w, 20), &ns, LP_ALIGN_START);
            if (src->group[0]) {
                lp_text_style gs = lp_text_style_default();
                gs.size_px = 9;
                gs.weight = LP_TEXT_WEIGHT_SEMIBOLD;
                gs.uppercase = 1;
                gs.letter_spacing = 0.8f;
                gs.color = gold;
                lp_size gsz = lp_text_measure(cr, src->group, &gs);
                lp_rect capsule = LP_RECT(col.x + col.w - 110 - gsz.w - 12, y + 3, gsz.w + 12, 18);
                lp_set_color(cr, lp_color_with_alpha(gold, 0.45f));
                cairo_set_line_width(cr, 1);
                lp_path_rrect(cr, lp_rect_inset(capsule, 0.5f, 0.5f), 9);
                cairo_stroke(cr);
                lp_text_draw(cr, src->group, capsule, &gs, LP_ALIGN_CENTER);
            }
            char pct[16];
            snprintf(pct, sizeof pct, "%.0f%%", src->influence * 100);
            lp_text_style ps = lp_text_style_default();
            ps.size_px = 11;
            ps.weight = LP_TEXT_WEIGHT_MEDIUM;
            ps.color = lp_color_with_alpha(LP_INK_PRIMARY, 0.6f);
            lp_text_draw(cr, pct, LP_RECT(col.x + col.w - 100, y + 2, 40, 20), &ps, LP_ALIGN_END);
            /* the influence bar */
            lp_rect bar = LP_RECT(col.x, y + 30, col.w, 4);
            lp_fill_solid(cr, bar, lp_color_with_alpha(LP_INK_PRIMARY, 0.08f), 2);
            float w = bar.w * src->influence;
            lp_fill_solid(cr, LP_RECT(bar.x, bar.y, w > 2 ? w : 2, 4), lp_color_with_alpha(gold, 0.6f), 2);
            /* three lines of the source, in the passage's voice */
            if (src->preview[0]) {
                lp_text_style vs = lp_text_style_default();
                vs.font = LP_FONT_DISPLAY;
                vs.italic = 1;
                vs.size_px = 12;
                vs.color = lp_color_with_alpha(LP_INK_PRIMARY, 0.6f);
                lp_text_layout *l = lp_text_layout_new(cr, src->preview, -1, &vs, col.w - 90);
                cairo_save(cr);
                cairo_rectangle(cr, col.x, y + 40, col.w - 90, 48);
                cairo_clip(cr);
                lp_text_layout_draw(cr, l, col.x, y + 40, vs.color);
                cairo_restore(cr);
                lp_text_layout_free(l);
            }
        }
        lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, !state };
        lp_size bs = lp_button_measure(ctx, "Open in Thread", bo);
        if (lp_button(ctx, lp_id_index(base, 100 + i), LP_RECT(col.x + col.w - bs.w, y + 42, bs.w, bs.h), "Open in Thread", bo) && state) {
            char target[256];
            snprintf(target, sizeof target, "document:%s", src->id);
            lp_desktop_open_app_with(d, "thread", target, NULL, NULL);
            ctx->dirty = 1;
        }
        y += row_h;
    }
    lp_scroll_end(ctx);
}

const lp_app lp_app_contribution = {
    .id = "contribution", .title = "From the thread", .name = "From the thread", .icon = LP_ICON_INFO, .hidden = 1, .internal = 1,
    .default_rect = { NAN, NAN, 440, 480 }, .min_size = { 380, 400 }, .singleton = 1, .resizable = 1,
    .create = contribution_create, .paint = contribution_paint, .destroy = contribution_destroy, .open = contribution_open,
    .model_changed = contribution_model_changed,
};

/* Tests. */
int lp_contribution_owner(const void *state) { return ((const struct contribution *)state)->owner; }
int lp_contribution_sources(const void *state) { return ((const struct contribution *)state)->source_count; }
const char *lp_contribution_source_name(const void *state, int i) { return ((const struct contribution *)state)->sources[i].name; }
const char *lp_contribution_source_preview(const void *state, int i) { return ((const struct contribution *)state)->sources[i].preview; }
float lp_contribution_royalty(const void *state) { return ((const struct contribution *)state)->royalty; }
