/* Preview's documents (lp_image.h). */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#ifdef HAVE_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif
#ifdef HAVE_POPPLER
#include <poppler.h>
#endif

#include "maryui/lp_image.h"

int lp_image_kind_supported(enum lp_file_kind kind) { return kind == LP_FILE_IMAGE || kind == LP_FILE_PDF; }

static int surface_ok(cairo_surface_t *s) { return s && cairo_surface_status(s) == CAIRO_STATUS_SUCCESS; }

#ifdef HAVE_PIXBUF
/* gdk-pixbuf's straight RGB(A) rows as a premultiplied ARGB32 surface — what
 * gdk_cairo_set_source_pixbuf does, without pulling in GTK for it. */
static cairo_surface_t *surface_from_pixbuf(GdkPixbuf *pb) {
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int channels = gdk_pixbuf_get_n_channels(pb), stride_in = gdk_pixbuf_get_rowstride(pb);
    int alpha = gdk_pixbuf_get_has_alpha(pb);
    const guchar *src = gdk_pixbuf_read_pixels(pb);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (!surface_ok(s)) return s;
    cairo_surface_flush(s);
    unsigned char *dst = cairo_image_surface_get_data(s);
    int stride = cairo_image_surface_get_stride(s);
    for (int y = 0; y < h; y++) {
        const guchar *in = src + y * stride_in;
        unsigned char *row = dst + y * stride;
        for (int x = 0; x < w; x++, in += channels) {
            uint32_t a = alpha ? in[3] : 255;
            uint32_t px = a << 24 | (in[0] * a + 127) / 255 << 16 | (in[1] * a + 127) / 255 << 8 | (in[2] * a + 127) / 255;
            memcpy(row + 4 * x, &px, 4);
        }
    }
    cairo_surface_mark_dirty(s);
    return s;
}
#endif

static int open_picture(lp_image_doc *doc, const char *path) {
#ifdef HAVE_PIXBUF
    int w = 0, h = 0;
    if (!gdk_pixbuf_get_file_info(path, &w, &h)) return -EINVAL;
    GError *err = NULL;
    GdkPixbuf *pb = w > LP_IMAGE_MAX_SIDE || h > LP_IMAGE_MAX_SIDE
        ? gdk_pixbuf_new_from_file_at_scale(path, LP_IMAGE_MAX_SIDE, LP_IMAGE_MAX_SIDE, TRUE, &err)
        : gdk_pixbuf_new_from_file(path, &err);
    if (err) g_error_free(err);
    if (!pb) return -EINVAL;
    GdkPixbuf *oriented = gdk_pixbuf_apply_embedded_orientation(pb);
    g_object_unref(pb);
    if (!oriented) return -EINVAL;
    doc->picture = surface_from_pixbuf(oriented);
    g_object_unref(oriented);
#else
    /* Without gdk-pixbuf only Cairo's PNG reader is left. */
    const char *dot = strrchr(path, '.');
    if (!dot || strcasecmp(dot + 1, "png") != 0) return -ENOTSUP;
    doc->picture = cairo_image_surface_create_from_png(path);
#endif
    if (!surface_ok(doc->picture)) {
        if (doc->picture) cairo_surface_destroy(doc->picture);
        doc->picture = NULL;
        return -EINVAL;
    }
    doc->type = LP_IMAGE_PICTURE;
    doc->page_count = 1;
    return 0;
}

static int open_pdf(lp_image_doc *doc, const char *path) {
#ifdef HAVE_POPPLER
    char absolute[PATH_MAX];
    if (!realpath(path, absolute)) return -errno;
    GError *err = NULL;
    char *uri = g_filename_to_uri(absolute, NULL, &err);
    if (err) { g_error_free(err); err = NULL; }
    if (!uri) return -EINVAL;
    PopplerDocument *pdf = poppler_document_new_from_file(uri, NULL, &err);
    g_free(uri);
    if (err) g_error_free(err);
    if (!pdf) return -EINVAL;
    doc->pdf = pdf;
    doc->type = LP_IMAGE_PDF;
    doc->page_count = poppler_document_get_n_pages(pdf);
    if (doc->page_count <= 0) { lp_image_close(doc); return -EINVAL; }
    return 0;
#else
    (void)doc;
    (void)path;
    return -ENOTSUP;
#endif
}

static void reset(lp_image_doc *doc) {
    memset(doc, 0, sizeof *doc);
    doc->cache_page = -1;
}

int lp_image_open(lp_image_doc *doc, const char *path) {
    reset(doc);
    int is_dir = 0;
    if (!path || !lp_files_exists(path, &is_dir)) return -ENOENT;
    if (is_dir) return -EISDIR;
    if (access(path, R_OK) != 0) return -errno;
    enum lp_file_kind kind = lp_files_kind(lp_files_basename(path), 0);
    if (kind == LP_FILE_PDF) return open_pdf(doc, path);
    if (kind == LP_FILE_IMAGE) return open_picture(doc, path);
    return -ENOTSUP;
}

void lp_image_close(lp_image_doc *doc) {
    if (doc->picture) cairo_surface_destroy(doc->picture);
    if (doc->cache) cairo_surface_destroy(doc->cache);
#ifdef HAVE_POPPLER
    if (doc->pdf) g_object_unref(doc->pdf);
#endif
    reset(doc);
}

lp_size lp_image_page_size(const lp_image_doc *doc, int page) {
    lp_size s = { 0, 0 };
    if (page < 0 || page >= doc->page_count) return s;
    if (doc->type == LP_IMAGE_PICTURE && doc->picture) {
        s.w = (float)cairo_image_surface_get_width(doc->picture);
        s.h = (float)cairo_image_surface_get_height(doc->picture);
    }
#ifdef HAVE_POPPLER
    if (doc->type == LP_IMAGE_PDF && doc->pdf) {
        PopplerPage *p = poppler_document_get_page(doc->pdf, page);
        if (p) {
            double w, h;
            poppler_page_get_size(p, &w, &h);
            s.w = (float)w;
            s.h = (float)h;
            g_object_unref(p);
        }
    }
#endif
    return s;
}

/* Paints the page with cr already scaled to the page's own units. */
static void paint_source(cairo_t *cr, lp_image_doc *doc, int page) {
    if (doc->type == LP_IMAGE_PICTURE && doc->picture) {
        cairo_set_source_surface(cr, doc->picture, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD); /* no dark fringe where scaling samples past the edge */
        cairo_paint(cr);
    }
#ifdef HAVE_POPPLER
    if (doc->type == LP_IMAGE_PDF && doc->pdf) {
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        PopplerPage *p = poppler_document_get_page(doc->pdf, page);
        if (p) {
            poppler_page_render(p, cr);
            g_object_unref(p);
        }
    }
#endif
}

static cairo_surface_t *render_page(lp_image_doc *doc, int page, int w, int h, lp_size size) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (!surface_ok(s)) return s;
    cairo_t *cr = cairo_create(s);
    cairo_scale(cr, w / size.w, h / size.h);
    paint_source(cr, doc, page);
    cairo_destroy(cr);
    return s;
}

void lp_image_draw(cairo_t *cr, lp_image_doc *doc, int page, lp_rect r, int quarter_turns) {
    lp_size size = lp_image_page_size(doc, page);
    if (size.w <= 0 || size.h <= 0 || r.w < 1 || r.h < 1) return;
    int turns = ((quarter_turns % 4) + 4) % 4;
    float bw = turns % 2 ? r.h : r.w, bh = turns % 2 ? r.w : r.h;   /* the page's own box, before turning */
    int w = (int)lroundf(bw), h = (int)lroundf(bh);
    if (w < 1 || h < 1) return;
    cairo_save(cr);
    cairo_translate(cr, r.x + r.w / 2, r.y + r.h / 2);
    cairo_rotate(cr, turns * M_PI / 2);
    cairo_translate(cr, -bw / 2, -bh / 2);
    if ((double)w * h <= LP_IMAGE_CACHE_PIXELS) {
        if (!doc->cache || doc->cache_page != page || doc->cache_w != w || doc->cache_h != h) {
            if (doc->cache) cairo_surface_destroy(doc->cache);
            doc->cache = render_page(doc, page, w, h, size);
            doc->cache_page = page;
            doc->cache_w = w;
            doc->cache_h = h;
        }
        /* the cache is the page at this exact size: copy it, do not resample it */
        cairo_set_source_surface(cr, doc->cache, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_rectangle(cr, 0, 0, w, h);
        cairo_fill(cr);
    } else {
        /* Zoomed far in: render straight through the caller's clip, which is the viewport. */
        cairo_rectangle(cr, 0, 0, bw, bh);
        cairo_clip(cr);
        cairo_scale(cr, bw / size.w, bh / size.h);
        paint_source(cr, doc, page);
    }
    cairo_restore(cr);
}

lp_size lp_image_rotated(lp_size s, int quarter_turns) {
    int turns = ((quarter_turns % 4) + 4) % 4;
    return turns % 2 ? (lp_size){ s.h, s.w } : s;
}

float lp_image_fit_scale(lp_size content, int quarter_turns, lp_size viewport, float padding) {
    lp_size c = lp_image_rotated(content, quarter_turns);
    float aw = viewport.w - 2 * padding, ah = viewport.h - 2 * padding;
    if (c.w <= 0 || c.h <= 0 || aw <= 0 || ah <= 0) return 1;
    float s = fminf(aw / c.w, ah / c.h);
    return s < 1 ? s : 1;
}

static const float ZOOM_STEPS[] = { 0.125f, 0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 3, 4, 6, 8, 12, 16 };
#define ZOOM_STEP_COUNT ((int)(sizeof ZOOM_STEPS / sizeof ZOOM_STEPS[0]))

float lp_image_zoom_step(float scale, int direction) {
    if (direction > 0) {
        for (int i = 0; i < ZOOM_STEP_COUNT; i++) if (ZOOM_STEPS[i] > scale * 1.001f) return ZOOM_STEPS[i];
        return ZOOM_STEPS[ZOOM_STEP_COUNT - 1];
    }
    for (int i = ZOOM_STEP_COUNT - 1; i >= 0; i--) if (ZOOM_STEPS[i] < scale * 0.999f) return ZOOM_STEPS[i];
    return ZOOM_STEPS[0];
}

int lp_image_sibling(const char *path, int delta, char *out, size_t n) {
    char dir[LP_FILES_PATH_MAX];
    lp_files_parent(path, dir, sizeof dir);
    const char *name = lp_files_basename(path);
    lp_file_list l = { 0 };
    int found = 0;
    if (lp_files_list(dir, 0, &l) == 0 && l.count > 0) {
        int *docs = malloc(sizeof(int) * (size_t)l.count), count = 0, at = -1;
        for (int i = 0; docs && i < l.count; i++) {
            if (l.entries[i].is_dir || !lp_image_kind_supported(l.entries[i].kind)) continue;
            if (strcmp(l.entries[i].name, name) == 0) at = count;
            docs[count++] = i;
        }
        if (docs && (count > 1 || (count == 1 && at < 0))) {
            int k = at < 0 ? (delta > 0 ? 0 : count - 1) : (((at + delta) % count) + count) % count;
            lp_files_join(dir, l.entries[docs[k]].name, out, n);
            found = 1;
        }
        free(docs);
    }
    lp_files_list_free(&l);
    return found;
}
