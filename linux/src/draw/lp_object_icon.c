/*
 * The lit tier, pass for pass as ObjectIcon.tsx draws it: contact shadow, body
 * ramp, tone, brushed grain, the 1px bevel pair, creases, the broad key, the
 * glossy sweep, and the keyline last.
 *
 * Layered gradients, not feSpecularLighting — at these sizes a 1px bump has no
 * gradient to light, and lp_specular_at wants an alpha height-map plane this
 * recipe never builds. Every pass here is a fill or a stroke through
 * lp_svgpath_apply, clipped to the part.
 *
 * Two numbers are load-bearing and easy to "fix" wrongly. object.headroom
 * slides a lit body DOWN its ramp so the key has somewhere to go: at zero, a
 * white key screened over a near-white body does nothing and every icon reads
 * flat. And object.grain-tile is in GRID units, deliberately finer than
 * brush.tile's 512 px — matching the window's density would fit a tenth of a
 * tile behind a 48px icon and the grain would vanish.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_blur.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_object_icon.h"
#include "maryui/lp_svgpath.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"

/* How far the 1px pair is pushed along the light axis, in grid units (RECIPE.bevelOffset). */
#define BEVEL_OFFSET 0.8f

lp_object lp_object_by_name(const char *name) {
    if (!name) return LP_OBJ_COUNT;
    for (int i = 0; i < LP_OBJ_COUNT; i++)
        if (strcmp(LP_OBJ_NAMES[i], name) == 0) return (lp_object)i;
    return LP_OBJ_COUNT;
}

/* MARK: - The material ramp */

typedef struct { lp_ramp ramp; float grain, gloss; } material;

/* `accent` and `folder` are the two the web resolves through CSS variables. */
static material material_of(lp_obj_material m, const lp_settings *settings) {
    switch (m) {
    case LP_OBJ_MATERIAL_PLATINUM: return (material){ { LP_OBJECT_MATERIAL_PLATINUM_HI, LP_OBJECT_MATERIAL_PLATINUM_MID, LP_OBJECT_MATERIAL_PLATINUM_LO }, LP_OBJECT_MATERIAL_PLATINUM_GRAIN, LP_OBJECT_MATERIAL_PLATINUM_GLOSS };
    case LP_OBJ_MATERIAL_MANILA: return (material){ { LP_OBJECT_MATERIAL_MANILA_HI, LP_OBJECT_MATERIAL_MANILA_MID, LP_OBJECT_MATERIAL_MANILA_LO }, LP_OBJECT_MATERIAL_MANILA_GRAIN, LP_OBJECT_MATERIAL_MANILA_GLOSS };
    case LP_OBJ_MATERIAL_SLATE: return (material){ { LP_OBJECT_MATERIAL_SLATE_HI, LP_OBJECT_MATERIAL_SLATE_MID, LP_OBJECT_MATERIAL_SLATE_LO }, LP_OBJECT_MATERIAL_SLATE_GRAIN, LP_OBJECT_MATERIAL_SLATE_GLOSS };
    case LP_OBJ_MATERIAL_PAPER: return (material){ { LP_OBJECT_MATERIAL_PAPER_HI, LP_OBJECT_MATERIAL_PAPER_MID, LP_OBJECT_MATERIAL_PAPER_LO }, LP_OBJECT_MATERIAL_PAPER_GRAIN, LP_OBJECT_MATERIAL_PAPER_GLOSS };
    case LP_OBJ_MATERIAL_GRAPHITE: return (material){ { LP_OBJECT_MATERIAL_GRAPHITE_HI, LP_OBJECT_MATERIAL_GRAPHITE_MID, LP_OBJECT_MATERIAL_GRAPHITE_LO }, LP_OBJECT_MATERIAL_GRAPHITE_GRAIN, LP_OBJECT_MATERIAL_GRAPHITE_GLOSS };
    case LP_OBJ_MATERIAL_RUBY: return (material){ { LP_OBJECT_MATERIAL_RUBY_HI, LP_OBJECT_MATERIAL_RUBY_MID, LP_OBJECT_MATERIAL_RUBY_LO }, LP_OBJECT_MATERIAL_RUBY_GRAIN, LP_OBJECT_MATERIAL_RUBY_GLOSS };
    case LP_OBJ_MATERIAL_AMBER: return (material){ { LP_OBJECT_MATERIAL_AMBER_HI, LP_OBJECT_MATERIAL_AMBER_MID, LP_OBJECT_MATERIAL_AMBER_LO }, LP_OBJECT_MATERIAL_AMBER_GRAIN, LP_OBJECT_MATERIAL_AMBER_GLOSS };
    case LP_OBJ_MATERIAL_VERDANT: return (material){ { LP_OBJECT_MATERIAL_VERDANT_HI, LP_OBJECT_MATERIAL_VERDANT_MID, LP_OBJECT_MATERIAL_VERDANT_LO }, LP_OBJECT_MATERIAL_VERDANT_GRAIN, LP_OBJECT_MATERIAL_VERDANT_GLOSS };
    case LP_OBJ_MATERIAL_GLASS: return (material){ { LP_OBJECT_MATERIAL_GLASS_HI, LP_OBJECT_MATERIAL_GLASS_MID, LP_OBJECT_MATERIAL_GLASS_LO }, LP_OBJECT_MATERIAL_GLASS_GRAIN, LP_OBJECT_MATERIAL_GLASS_GLOSS };
    case LP_OBJ_MATERIAL_ACCENT: {
        lp_accent a = lp_settings_accent(settings);
        return (material){ { a.light, a.base, a.deep }, LP_OBJECT_MATERIAL_ACCENT_GRAIN, LP_OBJECT_MATERIAL_ACCENT_GLOSS };
    }
    case LP_OBJ_MATERIAL_FOLDER:
    default:
        /* object.material.folder aliases manila in tokens.json; the appearance
         * setting is what actually picks the ramp (Manila / Slate). */
        return (material){ lp_settings_folders(settings), LP_OBJECT_MATERIAL_FOLDER_GRAIN, LP_OBJECT_MATERIAL_FOLDER_GLOSS };
    }
}

/** k > 0 lightens toward white, k < 0 darkens toward black (recipe.ts shade). */
static lp_color shade(lp_color c, float k) {
    float f = k >= 0 ? k : 1 + k;
    if (k >= 0) return LP_RGBA(c.r + (1 - c.r) * k, c.g + (1 - c.g) * k, c.b + (1 - c.b) * k, c.a);
    return LP_RGBA(c.r * f, c.g * f, c.b * f, c.a);
}

/* 0 is hi, 0.5 is mid, 1 is lo, and past 1 it keeps darkening for a face turned away. */
static lp_color ramp_at(lp_ramp r, float t) {
    if (t <= 0) return r.hi;
    if (t < 0.5f) return lp_color_mix(r.hi, r.mid, t / 0.5f);
    if (t <= 1) return lp_color_mix(r.mid, r.lo, (t - 0.5f) / 0.5f);
    return shade(r.lo, -(t - 1));
}

/* Which stretch of the ramp a face occupies (recipe.ts FACET_RANGE), slid down by headroom. */
static void facet_stops(lp_ramp r, lp_obj_facet facet, lp_color *from, lp_color *to) {
    float a = 0.12f, b = 0.62f; /* flat */
    if (facet == LP_OBJ_FACET_TOP) { a = 0.2f; b = 0.72f; }
    else if (facet == LP_OBJ_FACET_FRONT) { a = 0.5f; b = 1.0f; }
    else if (facet == LP_OBJ_FACET_UNDER) { a = 0.88f; b = 1.25f; }
    *from = ramp_at(r, a + LP_OBJECT_HEADROOM);
    *to = ramp_at(r, b + LP_OBJECT_HEADROOM);
}

/* `#rgb` / `#rrggbb`, which is every colour a tint names today. */
static int parse_hex(const char *v, lp_color *out) {
    if (!v || *v != '#') return 0;
    const char *h = v + 1;
    size_t n = strlen(h);
    if (n != 3 && n != 6) return 0;
    int ch[3];
    for (int i = 0; i < 3; i++) {
        char buf[3] = { n == 3 ? h[i] : h[i * 2], n == 3 ? h[i] : h[i * 2 + 1], 0 };
        char *end;
        long value = strtol(buf, &end, 16);
        if (end != buf + 2) return 0;
        ch[i] = (int)value;
    }
    *out = LP_RGBA(ch[0] / 255.0f, ch[1] / 255.0f, ch[2] / 255.0f, 1.0f);
    return 1;
}

/*
 * A tint is a dotted tokens.json path. accent.blue.* / accent.graphite.* follow
 * the live appearance, as tintColor() does; everything else is looked up in the
 * generated token table.
 */
static lp_color tint_color(const char *path, const lp_settings *settings) {
    if (strncmp(path, "accent.", 7) == 0) {
        const char *member = strchr(path + 7, '.');
        lp_accent a = lp_settings_accent(settings);
        if (member) {
            member++;
            if (strcmp(member, "base") == 0) return a.base;
            if (strcmp(member, "deep") == 0) return a.deep;
            if (strcmp(member, "light") == 0) return a.light;
            if (strcmp(member, "soft") == 0) return a.soft;
            if (strcmp(member, "focus-ring") == 0) return a.focus_ring;
        }
        return a.base;
    }
    for (int i = 0; i < LP_TOKEN_COUNT; i++) {
        if (strcmp(LP_TOKENS[i].name, path) != 0) continue;
        lp_color c;
        if (parse_hex(LP_TOKENS[i].value, &c)) return c;
        break;
    }
    return LP_INK_PRIMARY;
}

/* MARK: - Paths */

static void path_of(cairo_t *cr, const char *d, int even_odd) {
    cairo_new_path(cr);
    lp_svgpath_apply(cr, d);
    cairo_set_fill_rule(cr, even_odd ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
}

/* A linear gradient across a path's own bounding box, as SVG's objectBoundingBox
 * units do: (x1,y1)→(x2,y2) are fractions of that box. */
static cairo_pattern_t *box_gradient(cairo_t *cr, const char *d, float x2, float y2) {
    double bx1, by1, bx2, by2;
    path_of(cr, d, 0);
    cairo_path_extents(cr, &bx1, &by1, &bx2, &by2);
    double w = bx2 - bx1, h = by2 - by1;
    return cairo_pattern_create_linear(bx1, by1, bx1 + w * x2, by1 + h * y2);
}

/* MARK: - The passes */

static void body(cairo_t *cr, const lp_obj_part *p, lp_obj_material m, const lp_settings *settings, int lit) {
    material mat = material_of(m, settings);
    cairo_save(cr);
    path_of(cr, p->d, p->even_odd);
    cairo_clip_preserve(cr);

    if (p->tint) {
        lp_set_color(cr, tint_color(p->tint, settings));
        cairo_fill(cr);
    } else {
        lp_color from, to;
        facet_stops(mat.ramp, p->facet, &from, &to);
        cairo_pattern_t *g = box_gradient(cr, p->d, 0.85f, 1.0f);
        cairo_pattern_add_color_stop_rgba(g, 0, from.r, from.g, from.b, from.a);
        cairo_pattern_add_color_stop_rgba(g, 1, to.r, to.g, to.b, to.a);
        path_of(cr, p->d, p->even_odd);
        cairo_set_source(cr, g);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
    }

    /* Tone is a lighting offset, not a colour: white screened or black multiplied,
     * and it caps at object.tone-step. */
    if (p->tone != 0) {
        float a = fabsf(p->tone) * LP_OBJECT_TONE_STEP;
        path_of(cr, p->d, p->even_odd);
        cairo_set_source_rgba(cr, p->tone > 0 ? 1 : 0, p->tone > 0 ? 1 : 0, p->tone > 0 ? 1 : 0, a);
        cairo_fill(cr);
    }

    if (lit && mat.grain > 0 && !p->tint) {
        cairo_pattern_t *brush = cairo_pattern_create_for_surface(lp_brush_tile());
        cairo_pattern_set_extend(brush, CAIRO_EXTEND_REPEAT);
        /* object.grain-tile grid units per tile, not brush.tile's 512 px. */
        cairo_matrix_t mtx;
        double s = (double)LP_BRUSH_TILE / LP_OBJECT_GRAIN_TILE;
        cairo_matrix_init_scale(&mtx, s, s);
        cairo_pattern_set_matrix(brush, &mtx);
        path_of(cr, p->d, p->even_odd);
        cairo_clip(cr);
        cairo_set_source(cr, brush);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVERLAY);
        cairo_paint_with_alpha(cr, LP_OBJECT_GRAIN_OPACITY * mat.grain);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_pattern_destroy(brush);
    }

    /* The bevel pair, clipped to the part so only the inner 1px of each survives.
     * A well is lit from the opposite side: its rim sits along the bottom. */
    if (p->bevel != LP_OBJ_BEVEL_NONE) {
        float d = BEVEL_OFFSET * (p->bevel == LP_OBJ_BEVEL_WELL ? -1.0f : 1.0f);
        cairo_set_line_width(cr, 2);
        for (int i = 0; i < 2; i++) {
            cairo_save(cr);
            cairo_translate(cr, i == 0 ? d : -d, i == 0 ? d : -d);
            path_of(cr, p->d, p->even_odd);
            lp_set_color(cr, i == 0 ? LP_OBJECT_RIM : LP_OBJECT_OCCLUSION);
            cairo_stroke(cr);
            cairo_restore(cr);
        }
    }
    cairo_restore(cr);
}

/*
 * A crease is the 1px emboss pair the rest of the system uses, at icon scale: a
 * light line just below a dark one. A tinted crease is a mark rather than a
 * fold, so it drops the letterpress and just draws.
 */
static void line(cairo_t *cr, const lp_obj_part *p, const lp_settings *settings) {
    cairo_save(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    if (p->tint) {
        cairo_set_line_width(cr, 1.4);
        lp_set_color(cr, tint_color(p->tint, settings));
        path_of(cr, p->d, p->even_odd);
        cairo_stroke(cr);
    } else {
        cairo_set_line_width(cr, 1);
        cairo_save(cr);
        cairo_translate(cr, 0, 0.85);
        path_of(cr, p->d, p->even_odd);
        lp_set_color(cr, LP_OBJECT_RIM);
        cairo_stroke(cr);
        cairo_restore(cr);
        path_of(cr, p->d, p->even_odd);
        lp_set_color(cr, LP_OBJECT_KEYLINE);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

/* One wash over the whole object along the ramps' axis, scaled by how specular
 * the material is; and Aqua's sweep above it for a glossy finish. */
static void sweep(cairo_t *cr, const char *silhouette, int glossy, float gloss) {
    cairo_save(cr);
    path_of(cr, silhouette, 0);
    cairo_clip(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);

    cairo_pattern_t *key = box_gradient(cr, silhouette, 0.9f, 1.0f);
    lp_color s = LP_SHEEN_COLOR;
    cairo_pattern_add_color_stop_rgba(key, 0, s.r, s.g, s.b, LP_OBJECT_KEY_ALPHA);
    cairo_pattern_add_color_stop_rgba(key, LP_OBJECT_KEY_MID_AT, s.r, s.g, s.b, LP_OBJECT_KEY_MID);
    cairo_pattern_add_color_stop_rgba(key, LP_OBJECT_KEY_SPREAD, s.r, s.g, s.b, 0);
    path_of(cr, silhouette, 0);
    cairo_set_source(cr, key);
    cairo_paint_with_alpha(cr, gloss);
    cairo_pattern_destroy(key);

    if (glossy) {
        cairo_pattern_t *g = box_gradient(cr, silhouette, 0.0f, 1.0f);
        cairo_pattern_add_color_stop_rgba(g, 0, s.r, s.g, s.b, LP_OBJECT_GLOSS_ALPHA);
        cairo_pattern_add_color_stop_rgba(g, LP_OBJECT_GLOSS_BREAK, s.r, s.g, s.b, LP_OBJECT_GLOSS_SHOULDER);
        cairo_pattern_add_color_stop_rgba(g, LP_OBJECT_GLOSS_BREAK + 0.001f, s.r, s.g, s.b, 0);
        cairo_pattern_add_color_stop_rgba(g, 0.82f, s.r, s.g, s.b, 0);
        cairo_pattern_add_color_stop_rgba(g, 1, s.r, s.g, s.b, LP_OBJECT_GLOSS_BOUNCE);
        cairo_set_source(cr, g);
        cairo_paint(cr);
        cairo_pattern_destroy(g);
    }
    cairo_restore(cr);
}

/*
 * The one filter the web kept, and the only one here: the object's silhouette,
 * blurred and offset under it. The web drop-shadows the whole composite's
 * alpha; the silhouette is that outline by construction, and taking it directly
 * costs one small surface instead of an offscreen for every icon.
 */
static void contact_shadow(cairo_t *cr, const char *silhouette, float scale) {
    float blur = LP_OBJECT_CONTACT_BLUR * scale, dy = LP_OBJECT_CONTACT_DY * scale;
    int pad = (int)ceilf(blur * 3 + dy) + 2;
    int side = (int)ceilf(LP_OBJ_GRID * scale) + 2 * pad;
    if (side <= 0) return;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, side, side);
    cairo_t *sc = cairo_create(s);
    cairo_translate(sc, pad, pad);
    cairo_scale(sc, scale, scale);
    path_of(sc, silhouette, 0);
    cairo_set_source_rgba(sc, 0, 0, 0, 1);
    cairo_fill(sc);
    cairo_destroy(sc);
    lp_blur_surface_tinted(s, blur, LP_OBJECT_CONTACT_COLOR);
    /* The caller has translated to the icon's corner but not yet scaled, so the
     * surface goes down in those units: back by the blur's margin, forward by dy. */
    cairo_save(cr);
    cairo_set_source_surface(cr, s, -pad, -pad + dy);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(s);
}

/* MARK: - The object */

void lp_object_icon_draw(cairo_t *cr, lp_object obj, lp_rect box, const lp_settings *settings) {
    if (obj < 0 || obj >= LP_OBJ_COUNT) return;
    const lp_obj_def *def = &LP_OBJECTS[obj];
    float size = box.w < box.h ? box.w : box.h;
    if (size <= 0) return;

    enum lp_icon_tier tier = lp_icon_tier(size);
    if (tier == LP_TIER_GLYPH) {
        lp_icon icon = lp_icon_by_name(def->glyph);
        if (icon < LP_ICON_COUNT) lp_icon_draw(cr, icon, box.x, box.y, size, 0, LP_INK_SECONDARY);
        return;
    }
    int lit = tier == LP_TIER_LIT;
    float scale = size / (float)LP_OBJ_GRID;
    const lp_obj_part *silhouette = &def->parts[0];
    for (int i = 0; i < def->part_count; i++)
        if (strcmp(def->parts[i].id, def->silhouette) == 0) silhouette = &def->parts[i];

    cairo_save(cr);
    cairo_translate(cr, box.x, box.y);
    if (lit) contact_shadow(cr, silhouette->d, scale);
    cairo_scale(cr, scale, scale);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

    for (int i = 0; i < def->part_count; i++) {
        const lp_obj_part *p = &def->parts[i];
        if (size < p->min) continue;   /* `min` is in rendered px, not grid units */
        if (p->role == LP_OBJ_ROLE_BODY) body(cr, p, p->has_material ? p->material : def->material, settings, lit);
        else line(cr, p, settings);
    }

    if (lit) sweep(cr, silhouette->d, def->finish == LP_OBJ_FINISH_GLOSSY, material_of(def->material, settings).gloss);

    /* Last, always: this is what keeps an object legible when it is small. */
    cairo_set_line_width(cr, 1);
    path_of(cr, silhouette->d, 0);
    lp_set_color(cr, LP_OBJECT_KEYLINE);
    cairo_stroke(cr);
    cairo_restore(cr);
}
