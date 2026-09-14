/* Abilities — what Mary can do with each application, as the Mac's AbilityStudio shows an ability
 * package (PARITY D30). On MaryOS a package is an app's own declaration (lp_app.skills) or a
 * discipline several apps realize; there is nothing to record, so the panes read what the code
 * declares: the Control surface (the callable functions, as the Thread's `ability` records hold
 * them), Tune (the words and rules the package answers to) and Skills (every skill's schema).
 * Rehearse sends a sentence through maryd's triage and shows who would answer without a model.
 * Linux only. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_mary.h"
#include "maryui/lp_pane.h"
#include "maryui/lp_skill.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

#define PANES 3
#define PACKAGES_MAX 32
#define CARD_PAD LP_PANE_CARD_PAD
#define ROW LP_PANE_ROW
#define RAIL_W 200

enum { PANE_SURFACE, PANE_TUNE, PANE_SKILLS };
static const char *const PANE_NAMES[PANES] = { "surface", "tune", "skills" };

/* A package: an app that declares skills, or a discipline the apps realize. */
struct package {
    const lp_app *app;              /* NULL for a discipline */
    char discipline[32];
    char id[48];
};

struct abilities_app {
    char window_id[12];
    lp_desktop *desk;
    int selected;
    int pane;
    lp_scroll_state rail_scroll, scroll[PANES];
    lp_text_buffer rehearse;
    char rehearsal[400];            /* the last rehearsal's line, or "" */
    int asked;                      /* a rehearsal is out */
};

/* MARK: - The packages */

static int packages(const lp_desktop *d, struct package *out, int max) {
    int n = 0;
    if (!d) return 0;
    for (int i = 0; i < d->app_count && n < max; i++) {
        const lp_app *a = d->apps[i];
        if (!a->skill_count || !a->perform) continue;
        out[n].app = a;
        out[n].discipline[0] = 0;
        snprintf(out[n].id, sizeof out[n].id, "%s", a->id);
        n++;
    }
    for (int i = 0; i < d->app_count && n < max; i++) {
        const lp_app *a = d->apps[i];
        if (!a->skill_count || !a->perform || !a->discipline) continue;
        int seen = 0;
        for (int k = 0; k < n; k++) seen |= !out[k].app && strcmp(out[k].discipline, a->discipline) == 0;
        if (seen) continue;
        out[n].app = NULL;
        snprintf(out[n].discipline, sizeof out[n].discipline, "%s", a->discipline);
        snprintf(out[n].id, sizeof out[n].id, "discipline:%s", a->discipline);
        n++;
    }
    return n;
}

static const char *package_title(const struct package *p, char *buf, size_t n) {
    if (p->app) return p->app->name ? p->app->name : p->app->title;
    /* "window-management" → "Window management" */
    snprintf(buf, n, "%s", p->discipline);
    for (char *c = buf; *c; c++) if (*c == '-') *c = ' ';
    if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 'a' + 'A');
    return buf;
}

static const char *paradigm_of(const struct package *p) { return p->app ? (p->app->paradigm ? p->app->paradigm : "applicationExpertise") : "discipline"; }

/* The apps realizing a discipline, and the skills a package offers. */
static int discipline_apps(const lp_desktop *d, const char *discipline, const lp_app **out, int max) {
    int n = 0;
    for (int i = 0; d && i < d->app_count && n < max; i++)
        if (d->apps[i]->skill_count && d->apps[i]->perform && d->apps[i]->discipline && strcmp(d->apps[i]->discipline, discipline) == 0) out[n++] = d->apps[i];
    return n;
}

static int package_skills(const lp_desktop *d, const struct package *p) {
    if (p->app) return p->app->skill_count;
    const lp_app *apps[16];
    int n = discipline_apps(d, p->discipline, apps, 16), total = 0;
    for (int i = 0; i < n; i++) total += apps[i]->skill_count;
    return total;
}

static const char *kind_of(const lp_skill *s) { return s->kind ? s->kind : s->effect == LP_SKILL_READ ? "cognitive" : "effectful"; }
static const char *access_of(const lp_skill *s) { return s->access ? s->access : s->effect == LP_SKILL_READ ? "seamless" : s->effect == LP_SKILL_ACT ? "reversible" : "confirm"; }

/* MARK: - Drawing helpers: the pane kit's (lp_pane.h), shared with Threads and Ambient */

static int drawing(lp_ctx *ctx) { return ctx->pass == LP_PASS_DRAW && ctx->cr != NULL; }
static int lines_of(const char *text, float w) { return lp_pane_lines_of(text, w); }
static void card_frame(lp_ctx *ctx, lp_rect box, const char *title) { lp_pane_card_frame(ctx, box, title); }
static void row(lp_ctx *ctx, float x, float *y, float w, const char *label, const char *value) { lp_pane_row(ctx, x, y, w, label, value); }
static void paragraph(lp_ctx *ctx, float x, float *y, float w, const char *text, lp_color color, int italic) { lp_pane_paragraph(ctx, x, y, w, text, color, italic); }
static void mono(lp_ctx *ctx, float x, float y, float w, const char *text, lp_color color) { lp_pane_mono(ctx, x, y, w, text, color); }

static void effect_dot(lp_ctx *ctx, float x, float y, lp_skill_effect effect) {
    if (!drawing(ctx)) return;
    cairo_arc(ctx->cr, x, y, 4, 0, 2 * M_PI);
    lp_set_color(ctx->cr, effect == LP_SKILL_READ ? LP_PANE_SAGE : effect == LP_SKILL_ACT ? LP_PANE_GOLD : LP_PANE_RED);
    cairo_fill(ctx->cr);
}

/* "a, b, c" from a list of strings; "none" when empty. */
static void join(const char *const *items, int n, char *out, size_t cap) {
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        if (out[0]) strncat(out, ", ", cap - strlen(out) - 1);
        strncat(out, items[i], cap - strlen(out) - 1);
    }
    if (!out[0]) snprintf(out, cap, "none");
}

/* The parameters of a skill from its JSON Schema: "pane (enum, required), text (string)". */
static void inputs_of(const lp_skill *s, char *out, size_t cap) {
    out[0] = 0;
#ifdef HAVE_JSONC
    struct json_object *schema = s->params ? json_tokener_parse(s->params) : NULL, *props, *required, *v;
    if (schema && json_object_object_get_ex(schema, "properties", &props) && json_object_is_type(props, json_type_object)) {
        int has_required = json_object_object_get_ex(schema, "required", &required) && json_object_is_type(required, json_type_array);
        json_object_object_foreach(props, key, spec) {
            int req = 0;
            for (size_t i = 0; has_required && i < json_object_array_length(required); i++)
                req |= strcmp(json_object_get_string(json_object_array_get_idx(required, i)), key) == 0;
            const char *type = json_object_object_get_ex(spec, "type", &v) ? json_object_get_string(v) : "any";
            int is_enum = json_object_object_get_ex(spec, "enum", &v) && json_object_is_type(v, json_type_array);
            char piece[120];
            snprintf(piece, sizeof piece, "%s%s (%s%s)", out[0] ? ", " : "", key, is_enum ? "enum" : type, req ? ", required" : "");
            strncat(out, piece, cap - strlen(out) - 1);
        }
    }
    if (schema) json_object_put(schema);
#else
    if (s->params) snprintf(out, cap, "%s", s->params);
#endif
    if (!out[0]) snprintf(out, cap, "none");
}

/* The spoken values of a skill: "where: end ← at the end, to the end; start ← at the top". */
static void spoken_of(const lp_skill *s, char *out, size_t cap) {
    out[0] = 0;
#ifdef HAVE_JSONC
    struct json_object *spoken = s->spoken ? json_tokener_parse(s->spoken) : NULL;
    if (spoken && json_object_is_type(spoken, json_type_object)) {
        json_object_object_foreach(spoken, param, values) {
            char piece[400];
            snprintf(piece, sizeof piece, "%s%s: ", out[0] ? " \xC2\xB7 " : "", param);
            strncat(out, piece, cap - strlen(out) - 1);
            int first = 1;
            json_object_object_foreach(values, value, words) {
                snprintf(piece, sizeof piece, "%s%s \xE2\x86\x90 ", first ? "" : "; ", value);
                strncat(out, piece, cap - strlen(out) - 1);
                for (size_t i = 0; json_object_is_type(words, json_type_array) && i < json_object_array_length(words); i++) {
                    snprintf(piece, sizeof piece, "%s%s", i ? ", " : "", json_object_get_string(json_object_array_get_idx(words, i)));
                    strncat(out, piece, cap - strlen(out) - 1);
                }
                first = 0;
            }
        }
    }
    if (spoken) json_object_put(spoken);
#else
    if (s->spoken) snprintf(out, cap, "%s", s->spoken);
#endif
    if (!out[0]) snprintf(out, cap, "none");
}

/* MARK: - The panes. Each paints when `measure` is 0 and only adds heights when it is 1. */

static float paint_surface(lp_ctx *ctx, struct abilities_app *a, const struct package *p, lp_rect c, int measure) {
    float y = c.y, w = c.w, inner = w - 2 * CARD_PAD;
    const lp_app *apps[16];
    int napps = p->app ? 1 : discipline_apps(a->desk, p->discipline, apps, 16);
    if (p->app) apps[0] = p->app;
    int rows = 0;
    for (int i = 0; i < napps; i++) rows += apps[i]->skill_count;
    float h = 30 + (float)rows * (ROW + 6) + (p->app ? 0 : (float)napps * ROW) + CARD_PAD;
    lp_rect box = LP_RECT(c.x, y, w, h);
    if (!measure) {
        card_frame(ctx, box, p->app ? "Callable functions" : "Realized by");
        float yy = box.y + 30;
        for (int i = 0; i < napps; i++) {
            const lp_app *app = apps[i];
            if (!p->app) {
                char line[120];
                snprintf(line, sizeof line, "%s \xC2\xB7 %d skill%s", app->name ? app->name : app->title, app->skill_count, app->skill_count == 1 ? "" : "s");
                if (drawing(ctx)) {
                    lp_text_style s = lp_text_style_default();
                    s.size_px = LP_TEXT_SM;
                    s.weight = LP_TEXT_WEIGHT_SEMIBOLD;
                    lp_text_draw(ctx->cr, line, LP_RECT(box.x + CARD_PAD, yy, inner, ROW), &s, LP_ALIGN_START);
                }
                yy += ROW;
            }
            for (int k = 0; k < app->skill_count; k++) {
                const lp_skill *s = &app->skills[k];
                char invocation[128], inputs[300];
                snprintf(invocation, sizeof invocation, "%s__%s", app->id, s->id);
                inputs_of(s, inputs, sizeof inputs);
                effect_dot(ctx, box.x + CARD_PAD + 4, yy + ROW / 2.0f, s->effect);
                mono(ctx, box.x + CARD_PAD + 14, yy, 220, invocation, LP_INK_PRIMARY);
                if (drawing(ctx)) {
                    lp_text_style s2 = lp_text_style_default();
                    s2.size_px = LP_TEXT_SM;
                    s2.color = LP_INK_SECONDARY;
                    s2.ellipsize = 1;
                    char rest[420];
                    snprintf(rest, sizeof rest, "%s \xC2\xB7 %s", s->title, inputs);
                    lp_text_draw(ctx->cr, rest, LP_RECT(box.x + CARD_PAD + 240, yy, inner - 240, ROW), &s2, LP_ALIGN_START);
                }
                yy += ROW + 6;
            }
        }
    }
    y += h + CARD_PAD;
    /* what the graph holds of it */
    char note[300];
    snprintf(note, sizeof note, "Nothing here is recorded: Mary is the operating system, so every function is hers to call as declared. The Thread keeps "
                                "one style record per discipline naming the apps that realize it; each call she makes is kept as a behaviour record.");
    float nh = (float)lines_of(note, inner) * 17 + 4 + 30 + CARD_PAD;
    lp_rect nbox = LP_RECT(c.x, y, w, nh);
    if (!measure) {
        card_frame(ctx, nbox, "In the Thread");
        float yy = nbox.y + 30;
        paragraph(ctx, nbox.x + CARD_PAD, &yy, inner, note, LP_INK_SECONDARY, 1);
    }
    y += nh;
    return y - c.y;
}

static float paint_tune(lp_ctx *ctx, struct abilities_app *a, const struct package *p, lp_rect c, int measure) {
    float y = c.y, w = c.w, inner = w - 2 * CARD_PAD;
    const lp_app *apps[16];
    int napps = p->app ? 1 : discipline_apps(a->desk, p->discipline, apps, 16);
    if (p->app) apps[0] = p->app;
    char tokens[600] = "", phrases[900] = "", sounds[300] = "", careful[400] = "", competes[200] = "", settled[200] = "", order[300] = "";
    const char *summary = p->app ? p->app->summary : NULL;
    for (int i = 0; i < napps; i++) {
        const lp_app *app = apps[i];
        if (i) { strncat(sounds, ", ", sizeof sounds - strlen(sounds) - 1); strncat(order, ", ", sizeof order - strlen(order) - 1); }
        strncat(sounds, app->name ? app->name : app->title, sizeof sounds - strlen(sounds) - 1);
        if (app->aka) { strncat(sounds, ", ", sizeof sounds - strlen(sounds) - 1); strncat(sounds, app->aka, sizeof sounds - strlen(sounds) - 1); }
        for (int k = 0; k < app->alias_count; k++) { strncat(sounds, ", ", sizeof sounds - strlen(sounds) - 1); strncat(sounds, app->aliases[k], sizeof sounds - strlen(sounds) - 1); }
        strncat(order, app->id, sizeof order - strlen(order) - 1);
        for (int k = 0; k < app->skill_count; k++) {
            const lp_skill *s = &app->skills[k];
            for (int t = 0; t < s->trigger_count; t++) { if (tokens[0]) strncat(tokens, ", ", sizeof tokens - strlen(tokens) - 1); strncat(tokens, s->triggers[t], sizeof tokens - strlen(tokens) - 1); }
            for (int t = 0; t < s->phrase_count; t++) { if (phrases[0]) strncat(phrases, " \xC2\xB7 ", sizeof phrases - strlen(phrases) - 1); strncat(phrases, s->phrases[t], sizeof phrases - strlen(phrases) - 1); }
            if (s->effect == LP_SKILL_DESTRUCTIVE || strcmp(access_of(s), "confirm") == 0) {
                if (careful[0]) strncat(careful, ", ", sizeof careful - strlen(careful) - 1);
                strncat(careful, s->title, sizeof careful - strlen(careful) - 1);
            }
        }
        if (p->app && a->desk) {
            const lp_skill_policy *policy = &a->desk->skill_policy;
            snprintf(settled, sizeof settled, "System Settings \xE2\x80\xBA Mary: %s, asks %s", lp_skill_app_enabled(policy, app->id) ? "on" : "off",
                     lp_skill_ask_name(lp_skill_app_ask(policy, app->id)));
        }
    }
    if (!p->app) snprintf(settled, sizeof settled, "each app's own policy in System Settings \xE2\x80\xBA Mary");
    snprintf(competes, sizeof competes, "%s", p->app ? (p->app->discipline ? p->app->discipline : "no discipline: the app alone") : p->discipline);
    struct { const char *title, *text; } cards[] = {
        { "Summary", summary ? summary : "The apps that realize this discipline answer for it." },
        { "Listens for", tokens[0] ? tokens : "no tokens declared" },
        { "Phrases", phrases[0] ? phrases : "no phrases declared" },
        { "Sounds like", sounds },
        { "Order", order },
        { "Competes in", competes },
        { "Settled by", settled },
        { "Careful about", careful[0] ? careful : "nothing: every skill runs seamlessly or can be undone" },
    };
    for (size_t i = 0; i < sizeof cards / sizeof *cards; i++) {
        float h = (float)lines_of(cards[i].text, inner) * 17 + 4 + 30 + CARD_PAD;
        lp_rect box = LP_RECT(c.x, y, w, h);
        if (!measure) {
            card_frame(ctx, box, cards[i].title);
            float yy = box.y + 30;
            paragraph(ctx, box.x + CARD_PAD, &yy, inner, cards[i].text, LP_INK_PRIMARY, i == 0);
        }
        y += h + CARD_PAD;
    }
    const char *note = "Declared in the app's code, published to Mary as its skills. There is no override to save: tuning a package is a change to the app (PARITY D30).";
    float nh = (float)lines_of(note, inner) * 17 + 4 + CARD_PAD;
    if (!measure) { float yy = y; paragraph(ctx, c.x + 2 * CARD_PAD, &yy, inner, note, LP_INK_TERTIARY, 1); }
    y += nh;
    return y - c.y;
}

static float paint_skills(lp_ctx *ctx, struct abilities_app *a, const struct package *p, lp_rect c, int measure) {
    float y = c.y, w = c.w, inner = w - 2 * CARD_PAD;
    const lp_app *apps[16];
    int napps = p->app ? 1 : discipline_apps(a->desk, p->discipline, apps, 16);
    if (p->app) apps[0] = p->app;
    for (int i = 0; i < napps; i++) {
        const lp_app *app = apps[i];
        for (int k = 0; k < app->skill_count; k++) {
            const lp_skill *s = &app->skills[k];
            char inputs[300], triggers[400], phrases[600], classes[200], spoken[600], invocation[128], title[160];
            inputs_of(s, inputs, sizeof inputs);
            join(s->triggers, s->trigger_count, triggers, sizeof triggers);
            join(s->phrases, s->phrase_count, phrases, sizeof phrases);
            join(s->target_classes, s->target_class_count, classes, sizeof classes);
            spoken_of(s, spoken, sizeof spoken);
            snprintf(invocation, sizeof invocation, "%s__%s", app->id, s->id);
            snprintf(title, sizeof title, "%s%s%s", p->app ? "" : app->name ? app->name : app->title, p->app ? "" : " \xC2\xB7 ", s->title);
            float body = (float)lines_of(s->summary, inner) * 17 + 4 + 7 * ROW + (float)lines_of(spoken, inner - LP_PANE_LABEL_W - LP_PANE_GUTTER) * 17;
            float h = body + 30 + CARD_PAD;
            lp_rect box = LP_RECT(c.x, y, w, h);
            if (!measure) {
                card_frame(ctx, box, title);
                float cx = box.x + box.w - CARD_PAD;
                cx -= lp_pane_capsule_right(ctx, cx, box.y + 7, access_of(s), LP_PANE_BLUE) + 6;
                lp_pane_capsule_right(ctx, cx, box.y + 7, kind_of(s), LP_PANE_SAGE);
                float yy = box.y + 30;
                paragraph(ctx, box.x + CARD_PAD, &yy, inner, s->summary, LP_INK_PRIMARY, 1);
                row(ctx, box.x + CARD_PAD, &yy, inner, "Invocation:", invocation);
                row(ctx, box.x + CARD_PAD, &yy, inner, "Inputs:", inputs);
                row(ctx, box.x + CARD_PAD, &yy, inner, "Effect:", lp_skill_effect_name(s->effect));
                row(ctx, box.x + CARD_PAD, &yy, inner, "Listens for:", triggers);
                row(ctx, box.x + CARD_PAD, &yy, inner, "Phrases:", phrases);
                row(ctx, box.x + CARD_PAD, &yy, inner, "Targets:", classes);
                row(ctx, box.x + CARD_PAD, &yy, inner, "Enabled:", a->desk && lp_skill_enabled(&a->desk->skill_policy, app->id, s->id) ? "yes" : "no (System Settings \xE2\x80\xBA Mary)");
                if (drawing(ctx)) {
                    lp_text_style ls = lp_text_style_default();
                    ls.size_px = LP_TEXT_SM;
                    ls.color = LP_INK_TERTIARY;
                    lp_text_draw(ctx->cr, "Spoken as:", LP_RECT(box.x + CARD_PAD, yy, LP_PANE_LABEL_W, ROW), &ls, LP_ALIGN_END);
                }
                paragraph(ctx, box.x + CARD_PAD + LP_PANE_LABEL_W + LP_PANE_GUTTER, &yy, inner - LP_PANE_LABEL_W - LP_PANE_GUTTER, spoken, LP_INK_PRIMARY, 0);
            }
            y += h + CARD_PAD;
        }
    }
    return y - c.y;
}

/* MARK: - The rehearsal */

static void read_rehearsal(struct abilities_app *a) {
#ifdef HAVE_JSONC
    struct json_object *result = a->desk ? a->desk->mary.triage : NULL, *v, *winner;
    if (!result) return;
    int ok = json_object_object_get_ex(result, "ok", &v) && json_object_get_boolean(v);
    if (!ok) {
        const char *message = json_object_object_get_ex(result, "message", &v) ? json_object_get_string(v) : "triage failed";
        snprintf(a->rehearsal, sizeof a->rehearsal, "Not rehearsed: %s", message);
    } else if (json_object_object_get_ex(result, "winner", &winner) && json_object_is_type(winner, json_type_object)) {
        const char *invocation = json_object_object_get_ex(winner, "invocation", &v) ? json_object_get_string(v) : "?";
        const char *shape = json_object_object_get_ex(winner, "shape", &v) ? json_object_get_string(v) : "none";
        double score = json_object_object_get_ex(winner, "score", &v) ? json_object_get_double(v) : 0;
        int single = json_object_object_get_ex(winner, "singleClause", &v) && json_object_get_boolean(v);
        int dispatchable = json_object_object_get_ex(winner, "dispatchable", &v) && json_object_get_boolean(v);
        snprintf(a->rehearsal, sizeof a->rehearsal, "%s answers (%.2f) \xC2\xB7 %s \xC2\xB7 %s \xC2\xB7 %s", invocation, score, shape,
                 single ? "one clause" : "more than one clause", dispatchable ? "dispatches without a model" : "the model decides");
    } else {
        snprintf(a->rehearsal, sizeof a->rehearsal, "No unique winner: the model decides.");
    }
#endif
    a->asked = 0;
}

static void rehearse(struct abilities_app *a) {
    if (!a->desk || !a->rehearse.len) return;
    if (lp_mary_triage(&a->desk->mary, a->rehearse.text) == 0) {
        a->asked = 1;
        snprintf(a->rehearsal, sizeof a->rehearsal, "Asking maryd\xE2\x80\xA6");
    } else {
        snprintf(a->rehearsal, sizeof a->rehearsal, "maryd is not running.");
    }
}

/* MARK: - The window */

static void abilities_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct abilities_app preview;
    struct abilities_app *a = state ? state : &preview;
    if (!a->desk) a->desk = d;
    lp_id base = LP_ID("abilities");
    struct package list[PACKAGES_MAX];
    int n = packages(d, list, PACKAGES_MAX);
    if (a->selected >= n) a->selected = n ? n - 1 : 0;

    /* the rail: applications, then disciplines */
    lp_rect area = body;
    lp_rect cursor = lp_sidebar(ctx, &area);
    int section = -1;
    for (int i = 0; i < n; i++) {
        int kind = list[i].app ? 0 : 1;
        if (kind != section) { lp_sidebar_section(ctx, &cursor, kind ? "Disciplines" : "Applications"); section = kind; }
        char buf[64];
        const char *title = package_title(&list[i], buf, sizeof buf);
        if (lp_sidebar_item(ctx, lp_id_index(base, 100 + i), &cursor, list[i].app ? list[i].app->icon : LP_ICON_STAR, title, a->selected == i) && state) {
            a->selected = i;
            ctx->dirty = 1;
        }
    }
    if (drawing(ctx)) lp_fill_solid(ctx->cr, area, LP_SURFACE_BODY, 0);
    if (!n) return;
    const struct package *p = &list[a->selected];
    char buf[64];
    const char *title = package_title(p, buf, sizeof buf);

    /* the header: the package, its paradigm and count, its summary in the serif; the daemon's dot */
    int skills = package_skills(d, p);
    char subtitle[200];
    snprintf(subtitle, sizeof subtitle, "%s \xC2\xB7 by MaryOS \xC2\xB7 no model calls \xC2\xB7 %d skill%s", paradigm_of(p), skills, skills == 1 ? "" : "s");
    int connected = d && lp_mary_connected(&d->mary);
    lp_pane_header h = { .title = title, .subtitle = subtitle, .note = p->app ? p->app->summary : "The apps that realize this discipline answer for it.",
                         .live = connected, .status = connected ? "maryd" : "maryd is away" };
    lp_pane_header_paint(ctx, &area, &h);

    /* the pane bar: the segmented control, then Rehearse at the right */
    lp_rect bar = lp_pane_bar(ctx, &area);
    static const lp_segment PANES_UI[PANES] = { { "Control surface", LP_ICON_COUNT }, { "Tune", LP_ICON_COUNT }, { "Skills", LP_ICON_COUNT } };
    lp_size ss = lp_segmented_measure(ctx, PANES_UI, PANES, LP_CONTROL_SM);
    int pane = a->pane;
    if (lp_segmented(ctx, lp_id_index(base, 1), bar.x, bar.y + bar.h / 2 - ss.h / 2, PANES_UI, PANES, &pane, LP_CONTROL_SM) && pane != a->pane) {
        a->pane = pane;
        ctx->dirty = 1;
    }
    lp_button_opts ro = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, !connected };
    lp_size rs = lp_button_measure(ctx, "Rehearse", ro);
    float right = bar.x + bar.w;
    if (lp_button(ctx, lp_id_index(base, 2), LP_RECT(right - rs.w, bar.y + bar.h / 2 - rs.h / 2, rs.w, rs.h), "Rehearse", ro) && state) { rehearse(a); ctx->dirty = 1; }
    float fw = right - rs.w - LP_SPACE_2 - (bar.x + ss.w + LP_SPACE_4);
    if (fw > 160) {
        lp_rect field = LP_RECT(right - rs.w - LP_SPACE_2 - (fw > 320 ? 320 : fw), bar.y + bar.h / 2 - 12, fw > 320 ? 320 : fw, 24);
        lp_text_field(ctx, lp_id_index(base, 3), field, &a->rehearse, (lp_text_field_opts){ .placeholder = "Say it as you would\xE2\x80\xA6", .icon = LP_ICON_COUNT });
        if (ctx->pass == LP_PASS_EVENT && ctx->focus == lp_id_index(base, 3) && ctx->in.key_pressed && ctx->in.keysym == XKB_KEY_Return && state) { rehearse(a); ctx->dirty = 1; }
    }
    if (a->rehearsal[0]) {
        lp_rect line = lp_rect_cut_top(&area, 24);
        if (drawing(ctx)) {
            lp_text_style s = lp_text_style_default();
            s.size_px = LP_TEXT_SM;
            s.color = LP_INK_SECONDARY;
            s.ellipsize = 1;
            lp_text_draw(ctx->cr, a->rehearsal, LP_RECT(line.x + LP_PANE_INSET, line.y + 4, line.w - 2 * LP_PANE_INSET, line.h - 4), &s, LP_ALIGN_START);
        }
    }

    float (*paint)(lp_ctx *, struct abilities_app *, const struct package *, lp_rect, int) =
        a->pane == PANE_SURFACE ? paint_surface : a->pane == PANE_TUNE ? paint_tune : paint_skills;
    lp_rect column = lp_pane_content(area);
    lp_rect probe = LP_RECT(column.x, 0, column.w, 1e6f);
    float extent = paint(ctx, a, p, probe, 1) + CARD_PAD;
    lp_rect content = lp_scroll_begin(ctx, lp_id_index(base, 10 + a->pane), column, (lp_size){ column.w, extent }, &a->scroll[a->pane]);
    paint(ctx, a, p, content, 0);
    lp_scroll_end(ctx);
}

/* "textedit", "skills", "textedit:skills", "discipline:writing:tune". */
static void abilities_open(void *state, lp_desktop *d, const char *path) {
    struct abilities_app *a = state;
    if (!a || !path) return;
    char copy[80];
    snprintf(copy, sizeof copy, "%s", path);
    char *colon = strrchr(copy, ':');
    for (int i = 0; i < PANES; i++) {
        if (strcmp(copy, PANE_NAMES[i]) == 0) { a->pane = i; return; }
        if (colon && strcmp(colon + 1, PANE_NAMES[i]) == 0) { a->pane = i; *colon = 0; }
    }
    struct package list[PACKAGES_MAX];
    int n = packages(d, list, PACKAGES_MAX);
    for (int i = 0; i < n; i++) if (strcmp(list[i].id, copy) == 0) a->selected = i;
}

static int abilities_model_changed(void *state, lp_desktop *d, unsigned model, unsigned what) {
    struct abilities_app *a = state;
    if (model != LP_MODEL_MARY || !a) return 0;
    if ((what & LP_MARY_CHANGED_TRIAGE) && a->asked) read_rehearsal(a);
    return (what & (LP_MARY_CHANGED_TRIAGE | LP_MARY_CHANGED_CONNECTION)) != 0;
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

static void abilities_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    struct abilities_app *a = state;
    if (!a || menu != LP_MENU_VIEW) return;
    static const char *const LABELS[PANES] = { "Control Surface", "Tune", "Skills" };
    static const char *const KEYS[PANES] = { "⌘1", "⌘2", "⌘3" };
    for (int i = 0; i < PANES; i++) entry(m, LABELS[i], KEYS[i], LP_ABILITIES_PANE_SURFACE + i, a->pane == i);
    if (m->count < LP_MENU_MAX_ENTRIES) { memset(&m->entries[m->count], 0, sizeof m->entries[0]); m->entries[m->count++].separator = 1; }
    entry(m, "Rehearse", "⌘R", LP_ABILITIES_REHEARSE, 0);
}

static void abilities_command(void *state, lp_desktop *d, int cmd) {
    struct abilities_app *a = state;
    if (!a) return;
    if (cmd >= LP_ABILITIES_PANE_SURFACE && cmd < LP_ABILITIES_PANE_SURFACE + PANES) a->pane = cmd - LP_ABILITIES_PANE_SURFACE;
    else if (cmd == LP_ABILITIES_REHEARSE) rehearse(a);
}

static void *abilities_create(lp_desktop *d, const char *window_id) {
    struct abilities_app *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    snprintf(a->window_id, sizeof a->window_id, "%s", window_id);
    a->desk = d;
    lp_text_buffer_set(&a->rehearse, "");
    return a;
}

static void abilities_destroy(void *state) { free(state); }

const lp_app lp_app_abilities = {
    .id = "abilities", .title = "Abilities", .name = "Abilities", .icon = LP_ICON_STAR, .dock = 0,
    .default_rect = { NAN, NAN, 900, 620 }, .min_size = { 640, 420 }, .singleton = 1, .resizable = 1,
    .create = abilities_create, .paint = abilities_paint, .destroy = abilities_destroy, .open = abilities_open,
    .command = abilities_command, .menu_entries = abilities_menu_entries, .model_changed = abilities_model_changed,
};

/* MARK: - For tests and renders */

int lp_abilities_app_packages(const void *state) {
    const struct abilities_app *a = state;
    struct package list[PACKAGES_MAX];
    return a ? packages(a->desk, list, PACKAGES_MAX) : 0;
}
const char *lp_abilities_app_package_title(const void *state, int i) {
    const struct abilities_app *a = state;
    static char buf[64];
    struct package list[PACKAGES_MAX];
    int n = a ? packages(a->desk, list, PACKAGES_MAX) : 0;
    return i >= 0 && i < n ? package_title(&list[i], buf, sizeof buf) : NULL;
}
int lp_abilities_app_selected(const void *state) { return state ? ((const struct abilities_app *)state)->selected : 0; }
void lp_abilities_app_select(void *state, int i) { if (state && i >= 0) ((struct abilities_app *)state)->selected = i; }
int lp_abilities_app_pane(const void *state) { return state ? ((const struct abilities_app *)state)->pane : 0; }
int lp_abilities_app_skill_count(const void *state) {
    const struct abilities_app *a = state;
    struct package list[PACKAGES_MAX];
    int n = a ? packages(a->desk, list, PACKAGES_MAX) : 0;
    return a && a->selected < n ? package_skills(a->desk, &list[a->selected]) : 0;
}
const char *lp_abilities_app_rehearsal(const void *state) { return state ? ((const struct abilities_app *)state)->rehearsal : ""; }
