/* Preview's documents: a picture or a PDF opened into something Cairo can
 * draw (gdk-pixbuf for pictures when the build has it, Cairo's own PNG reader
 * when it does not; poppler-glib for PDFs), a page drawn into a rectangle in
 * quarter turns through a size-keyed cache, the fit and zoom arithmetic, and
 * the next picture in the folder. Linux only (PARITY D15). */
#ifndef MARYUI_LP_IMAGE_H
#define MARYUI_LP_IMAGE_H

#include <cairo.h>
#include <stddef.h>

#include "maryui/lp_files.h"
#include "maryui/lp_types.h"

enum lp_image_type { LP_IMAGE_NONE, LP_IMAGE_PICTURE, LP_IMAGE_PDF };

#define LP_IMAGE_MAX_SIDE 8192             /* a larger picture is decoded scaled down to this */
#define LP_IMAGE_CACHE_PIXELS (4096 * 4096) /* a page drawn larger than this is not cached, only its visible part drawn */

typedef struct lp_image_doc {
    enum lp_image_type type;
    int page_count;               /* 1 for a picture */
    cairo_surface_t *picture;     /* ARGB32, its embedded orientation applied */
    void *pdf;                    /* a PopplerDocument */
    cairo_surface_t *cache;       /* the last page drawn, at the size it was drawn */
    int cache_page, cache_w, cache_h;
} lp_image_doc;

/* 0, or -ENOENT / -EISDIR / -EACCES, -ENOTSUP when nothing in this build decodes
 * the kind, -EINVAL when the file will not decode. The doc is usable (empty) either way. */
int lp_image_open(lp_image_doc *doc, const char *path);
void lp_image_close(lp_image_doc *doc);
/* A picture's size in px, a PDF page's in points; 0×0 out of range. */
lp_size lp_image_page_size(const lp_image_doc *doc, int page);
/* Draws the page into r, turned quarter_turns clockwise; r is the turned box. */
void lp_image_draw(cairo_t *cr, lp_image_doc *doc, int page, lp_rect r, int quarter_turns);

lp_size lp_image_rotated(lp_size s, int quarter_turns);
/* The scale that fits content, turned, inside viewport less padding on each side; never above 1. */
float lp_image_fit_scale(lp_size content, int quarter_turns, lp_size viewport, float padding);
/* The next fixed zoom step above (direction > 0) or below scale, from 1/8 to 16. */
float lp_image_zoom_step(float scale, int direction);
/* 1 for the kinds Preview opens: pictures and PDFs. */
int lp_image_kind_supported(enum lp_file_kind kind);
/* The picture or PDF delta places from path among its folder's, in Finder order,
 * wrapping and skipping hidden files. 1 with out filled; 0 when there is no other. */
int lp_image_sibling(const char *path, int delta, char *out, size_t n);

#endif
