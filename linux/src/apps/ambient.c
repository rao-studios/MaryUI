/* Ambient — the world Mary holds and the route every turn took, as the Mac's ambient panes
 * show them (PARITY D29): World (each place, its surface line as the prompt sees it, the facts she
 * holds there), Realms (the last turn's need, the candidates, the chosen place), Routes (every
 * turn's route, field by field, with what retrieval returned) and Runs (the skills a turn
 * invoked). Everything comes from maryd through lp_mary (ambient.state, trace.list); Copy Report
 * puts the Mac's RouteReport text on the clipboard. Linux only. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_mary.h"
#include "maryui/lp_pane.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

#define TABS 4
#define CARD_PAD LP_PANE_CARD_PAD
#define REFRESH_MS 5000
#define ROW LP_PANE_ROW
#define MAX_ROUTES 50

enum { TAB_WORLD, TAB_REALMS, TAB_ROUTES, TAB_RUNS };
static const char *const TAB_NAMES[TABS] = { "world", "realms", "routes", "runs" };

struct ambient_app {
    char window_id[12];
    lp_desktop *desk;
    int tab;
    lp_scroll_state scroll[TABS];
    int asked;
    lp_source *timer;
    char filter[24];            /* the Routes tab's intent, or "" */
    char status[240];
    int want_report;            /* Copy Report asked; the next trace.report goes to the clipboard */
};

/* MARK: - JSON helpers */

#ifdef HAVE_JSONC
static const char *str(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string) ? json_object_get_string(v) : NULL;
}
static double num(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) ? json_object_get_double(v) : 0;
}
static int boolean(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) && json_object_get_boolean(v);
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

/* "a, b, c" from an array of strings, or of objects' `key`; "none" when empty. */
static void list(struct json_object *a, const char *key, char *out, size_t n) {
    out[0] = 0;
    for (size_t i = 0; i < alen(a); i++) {
        struct json_object *item = at(a, i);
        const char *s = key ? str(item, key) : json_object_get_string(item);
        if (!s) continue;
        if (out[0]) strncat(out, ", ", n - strlen(out) - 1);
        strncat(out, s, n - strlen(out) - 1);
    }
    if (!out[0]) snprintf(out, n, "none");
}
#endif

static const char *or_dash(const char *s) { return s && *s ? s : "—"; }

static void age_text(double seconds, char *out, size_t n) {
    if (seconds < 0) seconds = 0;
    if (seconds < 60) snprintf(out, n, "%.1fs", seconds);
    else if (seconds < 3600) snprintf(out, n, "%dm %ds", (int)(seconds / 60), (int)fmod(seconds, 60));
    else snprintf(out, n, "%dh %dm", (int)(seconds / 3600), (int)(fmod(seconds, 3600) / 60));
}

/* MARK: - Requests */

static void ask_all(struct ambient_app *a) {
    lp_mary *m = &a->desk->mary;
    if (!lp_mary_connected(m)) return;
    lp_mary_ambient_state(m);
    lp_mary_list_trace(m);
    a->asked = 1;
}

static int on_tick(int fd, uint32_t mask, void *data) {
    struct ambient_app *a = data;
    int w = lp_wm_find(&a->desk->wm, a->window_id);
    if (w >= 0 && a->desk->wm.windows[w].state != LP_WIN_SHADED && lp_mary_connected(&a->desk->mary)) {
        if (a->tab == TAB_WORLD || a->tab == TAB_REALMS) lp_mary_ambient_state(&a->desk->mary);
        else lp_mary_list_trace(&a->desk->mary);
    }
    lp_desktop_update_timer(a->desk, a->timer, REFRESH_MS);
    return 0;
}

/* MARK: - Drawing helpers: the pane kit's (lp_pane.h), so Ambient lines up with Threads and Abilities */

static int drawing(lp_ctx *ctx) { return ctx->pass == LP_PASS_DRAW && ctx->cr != NULL; }
static int lines_of(const char *text, float w) { return lp_pane_lines_of(text, w); }
static void card_frame(lp_ctx *ctx, lp_rect box, const char *title) { lp_pane_card_frame(ctx, box, title); }
static void row(lp_ctx *ctx, float x, float *y, float w, const char *label, const char *value) { lp_pane_row(ctx, x, y, w, label, value); }
static void paragraph(lp_ctx *ctx, float x, float *y, float w, const char *text, lp_color color, int italic) { lp_pane_paragraph(ctx, x, y, w, text, color, italic); }
static int chip(lp_ctx *ctx, lp_id id, float x, float y, const char *label, int selected, lp_size *size) { return lp_pane_chip(ctx, id, x, y, label, selected, size); }
static void live_dot(lp_ctx *ctx, float x, float y, int on) { lp_pane_live_dot(ctx, x, y, on); }
static void empty(lp_ctx *ctx, lp_rect area, const char *text) { lp_pane_empty(ctx, area, text); }

#define GOLD LP_PANE_GOLD
#define SAGE LP_PANE_SAGE
#define BLUE LP_PANE_BLUE

/* MARK: - The tabs. Each paints when `measure` is 0 and only adds heights when it is 1. */

#ifdef HAVE_JSONC

static float paint_world(lp_ctx *ctx, struct ambient_app *a, lp_rect c, lp_id base, int measure) {
    struct json_object *state = a->desk ? a->desk->mary.ambient : NULL, *places = arr(state, "places");
    float y = c.y, w = c.w, inner = w - 2 * CARD_PAD;
    if (!alen(places)) {
        if (!measure) empty(ctx, c, !a->desk || !lp_mary_connected(&a->desk->mary) ? "maryd is not running." : "Nothing is on screen yet: no app has published a surface.");
        return 40;
    }
    for (size_t i = 0; i < alen(places); i++) {
        struct json_object *card = at(places, i), *place = obj(card, "place"), *surface = obj(card, "surface"), *facts = arr(card, "facts");
        const char *name = or_dash(str(place, "name")), *line = str(surface, "surfaceLine");
        /* the card's height: the surface line, two rows, and a mention per fact */
        float body = 0;
        body += (float)lines_of(line ? line : "No surface right now.", inner) * 17 + 4;
        body += 2 * ROW;
        for (size_t f = 0; f < alen(facts); f++) body += (float)lines_of(str(at(facts, f), "mention"), inner - 12) * 17 + 4;
        float h = body + 30 + CARD_PAD;
        lp_rect box = LP_RECT(c.x, y, w, h - CARD_PAD);
        if (!measure) {
            card_frame(ctx, box, name);
            live_dot(ctx, box.x + box.w - CARD_PAD - 4, box.y + 16, surface && boolean(surface, "fresh"));
            /* the capsules sit at the title band's right, measured, the dot outermost */
            float rx = box.x + box.w - CARD_PAD - 8 - LP_SPACE_3;
            if (boolean(card, "isGlanced")) rx -= lp_pane_capsule_right(ctx, rx, box.y + 7, "glanced", BLUE) + 6;
            if (boolean(card, "isCoActive")) rx -= lp_pane_capsule_right(ctx, rx, box.y + 7, "co-active", SAGE) + 6;
            if (boolean(card, "isLead")) lp_pane_capsule_right(ctx, rx, box.y + 7, "focus", GOLD);
            float yy = box.y + 30;
            paragraph(ctx, box.x + CARD_PAD, &yy, inner, line ? line : "No surface right now.", LP_INK_PRIMARY, 0);
            char seen[64], holding[64];
            if (surface) { char ag[32]; age_text(num(surface, "age"), ag, sizeof ag); snprintf(seen, sizeof seen, "%s ago%s", ag, boolean(surface, "fresh") ? "" : " (stale)"); }
            else snprintf(seen, sizeof seen, "no surface");
            snprintf(holding, sizeof holding, "%zu fact%s", alen(facts), alen(facts) == 1 ? "" : "s");
            row(ctx, box.x + CARD_PAD, &yy, inner, "Seen:", seen);
            row(ctx, box.x + CARD_PAD, &yy, inner, "Holding:", holding);
            for (size_t f = 0; f < alen(facts); f++) {
                if (drawing(ctx)) {
                    lp_text_style ds = lp_text_style_default();
                    ds.size_px = LP_TEXT_SM;
                    ds.color = LP_INK_TERTIARY;
                    lp_text_draw(ctx->cr, "\xE2\x80\xA2", LP_RECT(box.x + CARD_PAD, yy, 12, 17), &ds, LP_ALIGN_START);
                }
                paragraph(ctx, box.x + CARD_PAD + 12, &yy, inner - 12, str(at(facts, f), "mention"), LP_INK_SECONDARY, 1);
            }
        }
        y += h;
    }
    struct json_object *selection = obj(state, "selection");
    if (selection) {
        const char *text = str(selection, "text");
        float body = (float)lines_of(text, inner) * 17 + 4 + 2 * ROW, h = body + 30 + CARD_PAD;
        lp_rect box = LP_RECT(c.x, y, w, h - CARD_PAD);
        if (!measure) {
            card_frame(ctx, box, "Selection");
            float yy = box.y + 30;
            char where[200], ag[32];
            age_text(num(selection, "age"), ag, sizeof ag);
            snprintf(where, sizeof where, "%s%s%s", or_dash(str(obj(selection, "place"), "name")), str(selection, "subject") ? " \xC2\xB7 " : "", str(selection, "subject") ? str(selection, "subject") : "");
            row(ctx, box.x + CARD_PAD, &yy, inner, "In:", where);
            row(ctx, box.x + CARD_PAD, &yy, inner, "Captured:", ag);
            paragraph(ctx, box.x + CARD_PAD, &yy, inner, text, LP_INK_PRIMARY, 1);
        }
        y += h;
    }
    return y - c.y;
}

/* The header's second line: what the section is looking at right now. */
static void subtitle_of(struct ambient_app *a, lp_desktop *d, char *out, size_t n) {
    struct json_object *state = d ? d->mary.ambient : NULL, *records = d ? d->mary.trace : NULL, *places = arr(state, "places");
    size_t count = alen(places), turns = alen(records);
    const char *lead = NULL;
    for (size_t i = 0; i < count && !lead; i++) if (boolean(at(places, i), "isLead")) lead = str(obj(at(places, i), "place"), "name");
    switch (a->tab) {
    case TAB_WORLD: snprintf(out, n, "%zu place%s on screen%s%s", count, count == 1 ? "" : "s", lead ? " \xC2\xB7 focus: " : "", lead ? lead : ""); break;
    case TAB_REALMS: snprintf(out, n, "The last turn's need, and who could have served it"); break;
    case TAB_ROUTES: snprintf(out, n, "%zu turn%s routed%s%s", turns, turns == 1 ? "" : "s", a->filter[0] ? " \xC2\xB7 only " : "", a->filter[0] ? a->filter : ""); break;
    default: snprintf(out, n, "The skills each turn invoked, and what came of them"); break;
    }
}

static float realm_card(lp_ctx *ctx, lp_rect c, float y, const char *title, struct json_object *realm, int measure) {
    float w = c.w, inner = w - 2 * CARD_PAD;
    struct json_object *need = obj(realm, "need"), *candidates = arr(realm, "candidates");
    float body = 3 * ROW + (float)(alen(candidates) ? alen(candidates) : 1) * ROW + ROW;
    float h = body + 30 + CARD_PAD;
    lp_rect box = LP_RECT(c.x, y, w, h - CARD_PAD);
    if (!measure) {
        card_frame(ctx, box, title);
        float yy = box.y + 30;
        char abilities[300];
        list(arr(need, "abilities"), NULL, abilities, sizeof abilities);
        row(ctx, box.x + CARD_PAD, &yy, inner, "Need:", abilities);
        row(ctx, box.x + CARD_PAD, &yy, inner, "Discipline:", str(need, "discipline"));
        char chosen[200];
        snprintf(chosen, sizeof chosen, "%s%s%s", obj(realm, "place") ? or_dash(str(obj(realm, "place"), "name")) : "none",
                 str(realm, "decidedBy") && strcmp(str(realm, "decidedBy"), "none") ? " \xC2\xB7 decided by " : "",
                 str(realm, "decidedBy") && strcmp(str(realm, "decidedBy"), "none") ? str(realm, "decidedBy") : "");
        row(ctx, box.x + CARD_PAD, &yy, inner, "Place:", chosen);
        yy += 4;
        if (!alen(candidates)) row(ctx, box.x + CARD_PAD, &yy, inner, "Candidates:", "nobody could serve");
        for (size_t i = 0; i < alen(candidates); i++) {
            struct json_object *cand = at(candidates, i);
            char detail[300] = "", by[200], ag[32] = "";
            list(arr(cand, "conformsByAbilities"), NULL, by, sizeof by);
            if (str(cand, "evidence")) age_text(num(cand, "evidenceAgeSeconds"), ag, sizeof ag);
            if (strcmp(by, "none")) { strncat(detail, "by ", sizeof detail - strlen(detail) - 1); strncat(detail, by, sizeof detail - strlen(detail) - 1); }
            if (boolean(cand, "conformsByDiscipline")) strncat(detail, detail[0] ? ", by discipline" : "by discipline", sizeof detail - strlen(detail) - 1);
            if (!detail[0]) strncat(detail, alen(arr(need, "abilities")) || str(need, "discipline") ? "does not conform" : "admitted: nothing was asked for", sizeof detail - strlen(detail) - 1);
            strncat(detail, boolean(cand, "hasEyes") ? " \xC2\xB7 eyes" : " \xC2\xB7 eyeless", sizeof detail - strlen(detail) - 1);
            if (str(cand, "evidence")) {
                strncat(detail, " \xC2\xB7 ", sizeof detail - strlen(detail) - 1);
                strncat(detail, str(cand, "evidence"), sizeof detail - strlen(detail) - 1);
                strncat(detail, " ", sizeof detail - strlen(detail) - 1);
                strncat(detail, ag, sizeof detail - strlen(detail) - 1);
                strncat(detail, " ago", sizeof detail - strlen(detail) - 1);
            } else {
                strncat(detail, " \xC2\xB7 no evidence", sizeof detail - strlen(detail) - 1);
            }
            char label[80];
            snprintf(label, sizeof label, "%s:", or_dash(str(obj(cand, "place"), "name")));
            row(ctx, box.x + CARD_PAD, &yy, inner, label, detail);
        }
    }
    return h;
}

static float paint_realms(lp_ctx *ctx, struct ambient_app *a, lp_rect c, lp_id base, int measure) {
    struct json_object *records = a->desk ? a->desk->mary.trace : NULL;
    float y = c.y;
    if (!alen(records)) {
        if (!measure) empty(ctx, c, "No turn has been resolved yet.");
        return 40;
    }
    struct json_object *last = at(records, 0);
    char title[600];
    snprintf(title, sizeof title, "This turn \xC2\xB7 %s", or_dash(str(last, "utterance")));
    y += realm_card(ctx, c, y, title, obj(obj(last, "route"), "realm"), measure);
    /* the last twenty turns: what each settled on */
    size_t n = alen(records) < 20 ? alen(records) : 20;
    float w = c.w, inner = w - 2 * CARD_PAD, h = (float)n * ROW + 30 + CARD_PAD;
    lp_rect box = LP_RECT(c.x, y, w, h - CARD_PAD);
    if (!measure) {
        card_frame(ctx, box, "History");
        float yy = box.y + 30;
        for (size_t i = 0; i < n; i++) {
            struct json_object *rec = at(records, i), *realm = obj(obj(rec, "route"), "realm");
            char label[80], value[400], ag[32];
            age_text(num(rec, "age"), ag, sizeof ag);
            snprintf(label, sizeof label, "%s ago:", ag);
            snprintf(value, sizeof value, "%s \xE2\x86\x92 %s", or_dash(str(rec, "utterance")), obj(realm, "place") ? or_dash(str(obj(realm, "place"), "name")) : "nowhere");
            row(ctx, box.x + CARD_PAD, &yy, inner, label, value);
        }
    }
    y += h;
    return y - c.y;
}

static int route_matches(struct ambient_app *a, struct json_object *rec) {
    const char *intent = str(obj(rec, "route"), "intent");
    return !a->filter[0] || (intent && strcmp(intent, a->filter) == 0);
}

static float route_card(lp_ctx *ctx, lp_rect c, float y, struct json_object *rec, int measure) {
    float w = c.w, inner = w - 2 * CARD_PAD;
    struct json_object *route = obj(rec, "route"), *gate = obj(route, "gate"), *memory = obj(gate, "memory"), *verdicts = obj(route, "verdicts"), *retrieval = arr(rec, "retrieval");
    const char *utterance = str(rec, "utterance");
    int retrieval_rows = 0;
    for (size_t i = 0; i < alen(retrieval); i++) retrieval_rows += 1 + (int)alen(arr(at(retrieval, i), "returned"));
    float body = (float)lines_of(utterance, inner) * 17 + 4 + 21 * ROW + (float)retrieval_rows * ROW + (str(rec, "contribution") ? ROW : 0);
    float h = body + 30 + CARD_PAD;
    lp_rect box = LP_RECT(c.x, y, w, h - CARD_PAD);
    if (measure) return h;
    char title[160], ag[32];
    age_text(num(rec, "age"), ag, sizeof ag);
    snprintf(title, sizeof title, "%s via %s \xC2\xB7 %s ago", or_dash(str(route, "intent")), or_dash(str(route, "decidedBy")), ag);
    card_frame(ctx, box, title);
    float yy = box.y + 30, x = box.x + CARD_PAD;
    paragraph(ctx, x, &yy, inner, utterance, LP_INK_PRIMARY, 1);
    char text[600];
    struct json_object *lead = obj(route, "leadPlace");
    row(ctx, x, &yy, inner, "lead:", lead ? str(lead, "token") : "none");
    row(ctx, x, &yy, inner, "lead.class:", lead ? str(lead, "class") : "none");
    list(arr(route, "namedPlaces"), "token", text, sizeof text);
    row(ctx, x, &yy, inner, "named:", text);
    list(arr(route, "candidateAttentions"), NULL, text, sizeof text);
    row(ctx, x, &yy, inner, "world.candidates:", text);
    row(ctx, x, &yy, inner, "ranking:", str(route, "rankingMode"));
    struct json_object *world = obj(route, "world");
    if (world) snprintf(text, sizeof text, "%s@%s%s%s", or_dash(str(world, "sense")), or_dash(str(world, "attention")), str(world, "subject") ? "#" : "", str(world, "subject") ? str(world, "subject") : "");
    else snprintf(text, sizeof text, "none");
    row(ctx, x, &yy, inner, "attention:", text);
    row(ctx, x, &yy, inner, "writing.target:", str(route, "writingTarget"));
    row(ctx, x, &yy, inner, "writing.context:", str(route, "supportingContext"));
    list(arr(gate, "questions"), NULL, text, sizeof text);
    row(ctx, x, &yy, inner, "questions:", text);
    list(arr(gate, "requestedAbilities"), NULL, text, sizeof text);
    row(ctx, x, &yy, inner, "abilities:", text);
    list(arr(gate, "applications"), NULL, text, sizeof text);
    row(ctx, x, &yy, inner, "applications:", text);
    char lanes[200], storage[200];
    list(arr(memory, "lanes"), NULL, lanes, sizeof lanes);
    list(arr(memory, "storageLanes"), NULL, storage, sizeof storage);
    snprintf(text, sizeof text, "%s (%s)", lanes, storage);
    row(ctx, x, &yy, inner, "threads:", text);
    snprintf(text, sizeof text, "locate %s \xC2\xB7 pre-read %s \xC2\xB7 execution %s", boolean(route, "needsLocate") ? "yes" : "no", boolean(route, "needsPreRead") ? "yes" : "no", boolean(route, "needsExecution") ? "yes" : "no");
    row(ctx, x, &yy, inner, "needs:", text);
    snprintf(text, sizeof text, "%.0f", num(rec, "systemPromptChars"));
    row(ctx, x, &yy, inner, "prompt.chars:", text);
    list(arr(rec, "packageIDs"), NULL, text, sizeof text);
    row(ctx, x, &yy, inner, "registry.packages:", text);
    snprintf(text, sizeof text, "%.0f", num(rec, "exposedSkillCount"));
    row(ctx, x, &yy, inner, "skills.exposed:", text);
    list(arr(rec, "skillRuns"), "invocation", text, sizeof text);
    row(ctx, x, &yy, inner, "skills.invoked:", text);
    list(arr(rec, "coActivePlaces"), "name", text, sizeof text);
    row(ctx, x, &yy, inner, "co-active:", text);
    struct json_object *edit = obj(verdicts, "editIntent");
    if (edit) { char targets[300]; list(arr(edit, "target"), NULL, targets, sizeof targets); snprintf(text, sizeof text, "%s \xE2\x80\x94 %s", or_dash(str(edit, "shape")), targets); }
    else snprintf(text, sizeof text, "none");
    row(ctx, x, &yy, inner, "verdict.edit:", text);
    row(ctx, x, &yy, inner, "verdict.named-part:", str(verdicts, "namedPart"));
    snprintf(text, sizeof text, "deictic %s \xC2\xB7 transform %s \xC2\xB7 ambient source %s \xC2\xB7 effectful %s", boolean(verdicts, "isDeictic") ? "yes" : "no",
             boolean(verdicts, "namesTransform") ? "yes" : "no", boolean(verdicts, "namesAmbientSource") ? "yes" : "no", boolean(verdicts, "actionTurn") ? "yes" : "no");
    row(ctx, x, &yy, inner, "verdicts:", text);
    row(ctx, x, &yy, inner, "verdict.override:", str(verdicts, "focusOverride"));
    for (size_t i = 0; i < alen(retrieval); i++) {
        struct json_object *purpose = at(retrieval, i), *returned = arr(purpose, "returned");
        char label[64], value[400];
        list(arr(purpose, "lanes"), NULL, lanes, sizeof lanes);
        snprintf(label, sizeof label, "retrieval.%s:", or_dash(str(purpose, "name")));
        snprintf(value, sizeof value, "asked %s \xE2\x86\x92 %zu back%s%s", lanes, alen(returned), str(purpose, "warning") ? " \xC2\xB7 " : "", str(purpose, "warning") ? str(purpose, "warning") : "");
        row(ctx, x, &yy, inner, label, value);
        for (size_t k = 0; k < alen(returned); k++) {
            struct json_object *doc = at(returned, k);
            snprintf(value, sizeof value, "%s \xC2\xB7 %s \xC2\xB7 %.2f", or_dash(str(doc, "document_id")), or_dash(str(doc, "family")), num(doc, "score"));
            row(ctx, x, &yy, inner, "", value);
        }
    }
    if (str(rec, "contribution")) row(ctx, x, &yy, inner, "contribution:", str(rec, "contribution"));
    return h;
}

static float paint_routes(lp_ctx *ctx, struct ambient_app *a, lp_rect c, lp_id base, int measure) {
    struct json_object *records = a->desk ? a->desk->mary.trace : NULL;
    float y = c.y;
    if (!alen(records)) {
        if (!measure) empty(ctx, c, "No turn has been resolved yet.");
        return 40;
    }
    /* the intents present, as chips */
    static const char *const INTENTS[] = { "architect", "decide", "halt", "revise", "compose", "operate", "perceive", "ask", "converse" };
    float x = c.x;
    lp_size size;
    if (!measure) {
        if (chip(ctx, lp_id_index(base, 300), x, y, "All", !a->filter[0], &size)) { a->filter[0] = 0; ctx->dirty = 1; }
        x += size.w + LP_SPACE_1;
        for (size_t i = 0; i < sizeof INTENTS / sizeof *INTENTS; i++) {
            int present = 0;
            for (size_t k = 0; k < alen(records) && !present; k++) present = str(obj(at(records, k), "route"), "intent") && strcmp(str(obj(at(records, k), "route"), "intent"), INTENTS[i]) == 0;
            if (!present) continue;
            if (chip(ctx, lp_id_index(base, 301 + (int)i), x, y, INTENTS[i], strcmp(a->filter, INTENTS[i]) == 0, &size)) {
                snprintf(a->filter, sizeof a->filter, "%s", INTENTS[i]);
                ctx->dirty = 1;
            }
            x += size.w + LP_SPACE_1;
        }
    }
    y += 24 + LP_SPACE_2;
    int shown = 0;
    for (size_t i = 0; i < alen(records) && shown < MAX_ROUTES; i++) {
        struct json_object *rec = at(records, i);
        if (!route_matches(a, rec)) continue;
        y += route_card(ctx, c, y, rec, measure);
        shown++;
    }
    return y - c.y;
}

static float paint_runs(lp_ctx *ctx, struct ambient_app *a, lp_rect c, lp_id base, int measure) {
    struct json_object *records = a->desk ? a->desk->mary.trace : NULL;
    float y = c.y, w = c.w, inner = w - 2 * CARD_PAD;
    int any = 0;
    for (size_t i = 0; i < alen(records); i++) {
        struct json_object *rec = at(records, i), *runs = arr(rec, "skillRuns");
        if (!alen(runs) && !str(rec, "confirmation")) continue;
        any = 1;
        float body = (float)alen(runs) * 4 * ROW + (str(rec, "confirmation") ? ROW : 0), h = body + 30 + CARD_PAD;
        lp_rect box = LP_RECT(c.x, y, w, h - CARD_PAD);
        if (!measure) {
            char title[600], ag[32];
            age_text(num(rec, "age"), ag, sizeof ag);
            snprintf(title, sizeof title, "%s \xC2\xB7 %s ago", or_dash(str(rec, "utterance")), ag);
            card_frame(ctx, box, title);
            float yy = box.y + 30, x = box.x + CARD_PAD;
            for (size_t k = 0; k < alen(runs); k++) {
                struct json_object *run = at(runs, k);
                char value[300];
                snprintf(value, sizeof value, "%s \xC2\xB7 %s%s", or_dash(str(run, "status")), or_dash(str(run, "effect")), boolean(run, "foundNothing") ? " \xC2\xB7 found nothing" : "");
                char label[120];
                snprintf(label, sizeof label, "%s:", or_dash(str(run, "invocation")));
                row(ctx, x, &yy, inner, label, value);
                row(ctx, x, &yy, inner, "arguments:", str(run, "args"));
                row(ctx, x, &yy, inner, "result:", str(run, "result"));
                double started = num(run, "started"), finished = num(run, "finished");
                if (finished > 0 && started > 0) snprintf(value, sizeof value, "%.0f ms", finished - started);
                else snprintf(value, sizeof value, "running");
                row(ctx, x, &yy, inner, "took:", value);
            }
            if (str(rec, "confirmation")) row(ctx, x, &yy, inner, "confirmation:", str(rec, "confirmation"));
        }
        y += h;
    }
    if (!any) {
        if (!measure) empty(ctx, c, "No skill has run yet.");
        return 40;
    }
    return y - c.y;
}

#endif

/* MARK: - The window */

static const char *const SECTION_TITLES[TABS] = { "World", "Realms", "Routes", "Runs" };
static const lp_icon SECTION_ICONS[TABS] = { LP_ICON_DESKTOP, LP_ICON_HOME, LP_ICON_FORWARD, LP_ICON_PLAY };

static void ambient_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct ambient_app preview;
    struct ambient_app *a = state ? state : &preview;
    if (!a->desk) a->desk = d;
    lp_id base = LP_ID("ambient");
    if (state && !a->asked && lp_mary_connected(&d->mary)) ask_all(a);
    /* the sections, as a source list */
    lp_rect area = body;
    lp_rect cursor = lp_sidebar(ctx, &area);
    lp_sidebar_section(ctx, &cursor, "Ambient");
    for (int i = 0; i < TABS; i++)
        if (lp_sidebar_item(ctx, lp_id_index(base, 200 + i), &cursor, SECTION_ICONS[i], SECTION_TITLES[i], a->tab == i) && a->tab != i) { a->tab = i; ctx->dirty = 1; }
    if (drawing(ctx)) lp_fill_solid(ctx->cr, area, LP_SURFACE_BODY, 0);
    /* the header: the section, what it looks at, the daemon's dot; Copy Report and Reload at its right */
    int connected = d && lp_mary_connected(&d->mary);
    char subtitle[240] = "";
#ifdef HAVE_JSONC
    subtitle_of(a, d, subtitle, sizeof subtitle);
#endif
    lp_pane_header h = { .title = SECTION_TITLES[a->tab], .subtitle = a->status[0] ? a->status : subtitle, .live = connected, .status = connected ? "maryd" : "maryd is away" };
    lp_rect controls = lp_pane_header_paint(ctx, &area, &h);
    lp_button_opts ro = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_RELOAD, 1, 0 };
    lp_size rs = lp_button_measure(ctx, "", ro);
    float right = controls.x + controls.w, cy = controls.y + controls.h / 2;
    if (lp_button(ctx, lp_id_index(base, 2), LP_RECT(right - rs.w, cy - rs.h / 2, rs.w, rs.h), "", ro) && state) { ask_all(a); ctx->dirty = 1; }
    right -= rs.w + LP_SPACE_2;
    lp_button_opts co = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, !connected };
    lp_size cs = lp_button_measure(ctx, "Copy Report", co);
    if (lp_button(ctx, lp_id_index(base, 3), LP_RECT(right - cs.w, cy - cs.h / 2, cs.w, cs.h), "Copy Report", co) && state && d) {
        a->want_report = lp_mary_trace_report(&d->mary) == 0;
        snprintf(a->status, sizeof a->status, a->want_report ? "Asking maryd for the report\xE2\x80\xA6" : "maryd is not running.");
        ctx->dirty = 1;
    }
#ifdef HAVE_JSONC
    float (*paint)(lp_ctx *, struct ambient_app *, lp_rect, lp_id, int) =
        a->tab == TAB_WORLD ? paint_world : a->tab == TAB_REALMS ? paint_realms : a->tab == TAB_ROUTES ? paint_routes : paint_runs;
    lp_rect column = lp_pane_content(area);
    lp_rect probe = LP_RECT(column.x, 0, column.w, 1e6f);
    float extent = paint(ctx, a, probe, base, 1) + CARD_PAD;
    lp_rect content = lp_scroll_begin(ctx, lp_id_index(base, 10 + a->tab), column, (lp_size){ column.w, extent }, &a->scroll[a->tab]);
    paint(ctx, a, content, base, 0);
    lp_scroll_end(ctx);
#else
    empty(ctx, area, "Built without json-c: nothing to show.");
#endif
}

static void ambient_open(void *state, lp_desktop *d, const char *path) {
    struct ambient_app *a = state;
    if (!a || !path) return;
    for (int i = 0; i < TABS; i++) if (strcmp(path, TAB_NAMES[i]) == 0) a->tab = i;
    if (strncmp(path, "routes:", 7) == 0) { a->tab = TAB_ROUTES; snprintf(a->filter, sizeof a->filter, "%s", path + 7); }
}

static int ambient_model_changed(void *state, lp_desktop *d, unsigned model, unsigned what) {
    struct ambient_app *a = state;
    if (model != LP_MODEL_MARY || !a) return 0;
    if ((what & LP_MARY_CHANGED_CONNECTION) && lp_mary_connected(&d->mary) && !a->asked) ask_all(a);
    if (what & LP_MARY_CHANGED_MESSAGES) {
        /* a reply landed: the turn has a route and a trace row now */
        int streaming = 0;
        for (int i = 0; i < d->mary.message_count; i++) streaming |= d->mary.messages[i].streaming;
        if (!streaming && lp_mary_connected(&d->mary)) ask_all(a);
    }
    if ((what & LP_MARY_CHANGED_TRACE) && a->want_report && d->mary.trace_report) {
        lp_text_clipboard_set(lp_text_clipboard_shared(), d->mary.trace_report, (int)strlen(d->mary.trace_report));
        snprintf(a->status, sizeof a->status, "Copied the report to the clipboard.");
        a->want_report = 0;
    }
    return (what & (LP_MARY_CHANGED_AMBIENT | LP_MARY_CHANGED_TRACE | LP_MARY_CHANGED_CONNECTION)) != 0;
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

static void ambient_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    struct ambient_app *a = state;
    if (!a || menu != LP_MENU_VIEW) return;
    static const char *const LABELS[TABS] = { "World", "Realms", "Routes", "Runs" };
    static const char *const KEYS[TABS] = { "⌘1", "⌘2", "⌘3", "⌘4" };
    for (int i = 0; i < TABS; i++) entry(m, LABELS[i], KEYS[i], LP_AMBIENT_TAB_WORLD + i, a->tab == i);
    if (m->count < LP_MENU_MAX_ENTRIES) { memset(&m->entries[m->count], 0, sizeof m->entries[0]); m->entries[m->count++].separator = 1; }
    entry(m, "Reload", "⌘R", LP_AMBIENT_RELOAD, 0);
    entry(m, "Copy Report", NULL, LP_AMBIENT_COPY_REPORT, 0);
}

static void ambient_command(void *state, lp_desktop *d, int cmd) {
    struct ambient_app *a = state;
    if (!a) return;
    if (cmd >= LP_AMBIENT_TAB_WORLD && cmd < LP_AMBIENT_TAB_WORLD + TABS) a->tab = cmd - LP_AMBIENT_TAB_WORLD;
    else if (cmd == LP_AMBIENT_RELOAD) ask_all(a);
    else if (cmd == LP_AMBIENT_COPY_REPORT && d) a->want_report = lp_mary_trace_report(&d->mary) == 0;
}

static void *ambient_create(lp_desktop *d, const char *window_id) {
    struct ambient_app *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    snprintf(a->window_id, sizeof a->window_id, "%s", window_id);
    a->desk = d;
    if (d) {
        a->timer = lp_desktop_add_timer(d, REFRESH_MS, on_tick, a);
        if (lp_mary_connected(&d->mary)) ask_all(a);
    }
    return a;
}

static void ambient_destroy(void *state) {
    struct ambient_app *a = state;
    if (!a) return;
    if (a->timer) lp_desktop_remove_source(a->desk, a->timer);
    free(a);
}

const lp_app lp_app_ambient = {
    .id = "ambient", .title = "Ambient", .name = "Ambient", .icon = LP_ICON_STAR, .dock = 0,
    .default_rect = { NAN, NAN, 860, 600 }, .min_size = { 600, 400 }, .singleton = 1, .resizable = 1,
    .create = ambient_create, .paint = ambient_paint, .destroy = ambient_destroy, .open = ambient_open,
    .command = ambient_command, .menu_entries = ambient_menu_entries, .model_changed = ambient_model_changed,
};

/* MARK: - For tests and renders */

int lp_ambient_app_tab(const void *state) { return state ? ((const struct ambient_app *)state)->tab : 0; }
void lp_ambient_app_set_tab(void *state, int tab) { if (state && tab >= 0 && tab < TABS) ((struct ambient_app *)state)->tab = tab; }
void lp_ambient_app_set_filter(void *state, const char *intent) { if (state) snprintf(((struct ambient_app *)state)->filter, 24, "%s", intent ? intent : ""); }
const char *lp_ambient_app_status(const void *state) { return state ? ((const struct ambient_app *)state)->status : ""; }

#ifdef HAVE_JSONC
int lp_ambient_app_places(const void *state) {
    const struct ambient_app *a = state;
    return a && a->desk ? (int)alen(arr(a->desk->mary.ambient, "places")) : 0;
}
const char *lp_ambient_app_place_name(const void *state, int i) {
    const struct ambient_app *a = state;
    return a && a->desk ? str(obj(at(arr(a->desk->mary.ambient, "places"), (size_t)i), "place"), "name") : NULL;
}
int lp_ambient_app_routes(const void *state) {
    struct ambient_app *a = (struct ambient_app *)state;
    if (!a || !a->desk) return 0;
    int n = 0;
    struct json_object *records = a->desk->mary.trace;
    for (size_t i = 0; i < alen(records); i++) n += route_matches(a, at(records, i));
    return n;
}
const char *lp_ambient_app_route_intent(const void *state, int index) {
    struct ambient_app *a = (struct ambient_app *)state;
    if (!a || !a->desk) return NULL;
    struct json_object *records = a->desk->mary.trace;
    int n = 0;
    for (size_t i = 0; i < alen(records); i++) {
        if (!route_matches(a, at(records, i))) continue;
        if (n++ == index) return str(obj(at(records, i), "route"), "intent");
    }
    return NULL;
}
#else
int lp_ambient_app_places(const void *state) { return 0; }
const char *lp_ambient_app_place_name(const void *state, int i) { return NULL; }
int lp_ambient_app_routes(const void *state) { return 0; }
const char *lp_ambient_app_route_intent(const void *state, int i) { return NULL; }
#endif
