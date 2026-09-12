/* Preview's documents: opening pictures (and PDFs, when poppler is in the
 * build), drawing a page in quarter turns, the fit and zoom arithmetic, and
 * stepping through a folder. Linux only (PARITY D15). */
#define _DARWIN_C_SOURCE 1   /* mkdtemp on macOS */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cairo.h>
#if CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif
#include "lp_test.h"
#include "maryui/lp_files.h"
#include "maryui/lp_image.h"

static char root[512];

static void path_of(const char *name, char *out, size_t n) { snprintf(out, n, "%s/%s", root, name); }

/* A w×h PNG, its left half red and its right half blue. */
static void write_picture(const char *name, int w, int h) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_rectangle(cr, 0, 0, w / 2.0, h);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0, 0, 1);
    cairo_rectangle(cr, w / 2.0, 0, w / 2.0, h);
    cairo_fill(cr);
    cairo_destroy(cr);
    char path[600];
    path_of(name, path, sizeof path);
    cairo_surface_write_to_png(s, path);
    cairo_surface_destroy(s);
}

static void write_bytes(const char *name, const char *text) {
    char path[600];
    path_of(name, path, sizeof path);
    FILE *f = fopen(path, "wb");
    fputs(text, f);
    fclose(f);
}

/* 'r' or 'b' for a red or blue pixel at (x, y), '?' otherwise. */
static char colour_at(cairo_surface_t *s, int x, int y) {
    cairo_surface_flush(s);
    uint32_t px;
    memcpy(&px, cairo_image_surface_get_data(s) + y * cairo_image_surface_get_stride(s) + 4 * x, 4);
    int r = px >> 16 & 255, b = px & 255;
    return r > 200 && b < 50 ? 'r' : b > 200 && r < 50 ? 'b' : '?';
}

LP_TEST(fits_inside_the_viewport_and_never_enlarges) {
    LP_ASSERT_NEAR(lp_image_fit_scale((lp_size){ 2000, 1000 }, 0, (lp_size){ 800, 600 }, 20), 0.38, 1e-6);
    LP_ASSERT_NEAR(lp_image_fit_scale((lp_size){ 2000, 1000 }, 1, (lp_size){ 800, 600 }, 20), 0.28, 1e-6);
    LP_ASSERT_NEAR(lp_image_fit_scale((lp_size){ 100, 50 }, 0, (lp_size){ 800, 600 }, 20), 1, 1e-6);
    LP_ASSERT_NEAR(lp_image_fit_scale((lp_size){ 0, 0 }, 0, (lp_size){ 800, 600 }, 20), 1, 1e-6);
    lp_size turned = lp_image_rotated((lp_size){ 40, 20 }, -1);
    LP_ASSERT_NEAR(turned.w, 20, 1e-6);
    LP_ASSERT_NEAR(turned.h, 40, 1e-6);
}

LP_TEST(steps_the_zoom_through_fixed_scales) {
    LP_ASSERT_NEAR(lp_image_zoom_step(1, 1), 1.5, 1e-6);
    LP_ASSERT_NEAR(lp_image_zoom_step(1, -1), 0.75, 1e-6);
    LP_ASSERT_NEAR(lp_image_zoom_step(0.3f, 1), 0.5, 1e-6);    /* from a fit's odd scale, to the next step */
    LP_ASSERT_NEAR(lp_image_zoom_step(0.3f, -1), 0.25, 1e-6);
    LP_ASSERT_NEAR(lp_image_zoom_step(16, 1), 16, 1e-6);
    LP_ASSERT_NEAR(lp_image_zoom_step(0.125f, -1), 0.125, 1e-6);
}

LP_TEST(opens_a_png_and_reports_its_size) {
    write_picture("wide.png", 40, 20);
    char path[600];
    path_of("wide.png", path, sizeof path);
    lp_image_doc doc;
    LP_ASSERT_EQ(lp_image_open(&doc, path), 0);
    LP_ASSERT_EQ(doc.type, LP_IMAGE_PICTURE);
    LP_ASSERT_EQ(doc.page_count, 1);
    lp_size s = lp_image_page_size(&doc, 0);
    LP_ASSERT_NEAR(s.w, 40, 1e-6);
    LP_ASSERT_NEAR(s.h, 20, 1e-6);
    s = lp_image_page_size(&doc, 1);
    LP_ASSERT_NEAR(s.w, 0, 1e-6);
    lp_image_close(&doc);
    LP_ASSERT_EQ(doc.type, LP_IMAGE_NONE);
}

LP_TEST(draws_a_page_turned_in_quarter_turns) {
    char path[600];
    path_of("wide.png", path, sizeof path);
    lp_image_doc doc;
    LP_ASSERT_EQ(lp_image_open(&doc, path), 0);
    cairo_surface_t *flat = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 40, 20);
    cairo_t *cr = cairo_create(flat);
    lp_image_draw(cr, &doc, 0, LP_RECT(0, 0, 40, 20), 0);
    LP_ASSERT_EQ(colour_at(flat, 5, 10), 'r');
    LP_ASSERT_EQ(colour_at(flat, 35, 10), 'b');
    cairo_surface_t *cached = doc.cache;
    lp_image_draw(cr, &doc, 0, LP_RECT(0, 0, 40, 20), 2);   /* same size: the cache is reused, turned */
    LP_ASSERT(doc.cache == cached);
    LP_ASSERT_EQ(colour_at(flat, 5, 10), 'b');
    LP_ASSERT_EQ(colour_at(flat, 35, 10), 'r');
    cairo_destroy(cr);
    cairo_surface_destroy(flat);

    cairo_surface_t *tall = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 20, 40);
    cr = cairo_create(tall);
    lp_image_draw(cr, &doc, 0, LP_RECT(0, 0, 20, 40), 1);   /* a clockwise turn puts the left edge on top */
    LP_ASSERT_EQ(colour_at(tall, 10, 5), 'r');
    LP_ASSERT_EQ(colour_at(tall, 10, 35), 'b');
    lp_image_draw(cr, &doc, 0, LP_RECT(0, 0, 20, 40), -1);
    LP_ASSERT_EQ(colour_at(tall, 10, 5), 'b');
    LP_ASSERT_EQ(colour_at(tall, 10, 35), 'r');
    cairo_destroy(cr);
    cairo_surface_destroy(tall);

    cairo_surface_t *big = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 80, 40);
    cr = cairo_create(big);
    lp_image_draw(cr, &doc, 0, LP_RECT(0, 0, 80, 40), 0);   /* scaled up, still split down the middle */
    LP_ASSERT_EQ(colour_at(big, 30, 20), 'r');
    LP_ASSERT_EQ(colour_at(big, 50, 20), 'b');
    LP_ASSERT_EQ(doc.cache_w, 80);
    cairo_destroy(cr);
    cairo_surface_destroy(big);
    lp_image_close(&doc);
}

LP_TEST(refuses_what_it_cannot_open) {
    lp_image_doc doc;
    char path[600];
    path_of("missing.png", path, sizeof path);
    LP_ASSERT_EQ(lp_image_open(&doc, path), -ENOENT);
    LP_ASSERT_EQ(lp_image_open(&doc, root), -EISDIR);
    write_bytes("note.txt", "hello");
    path_of("note.txt", path, sizeof path);
    LP_ASSERT_EQ(lp_image_open(&doc, path), -ENOTSUP);
    write_bytes("broken.png", "not a png at all");
    path_of("broken.png", path, sizeof path);
    LP_ASSERT_EQ(lp_image_open(&doc, path), -EINVAL);
    LP_ASSERT_EQ(doc.type, LP_IMAGE_NONE);
    write_bytes("broken.jpg", "not a jpeg either");
    path_of("broken.jpg", path, sizeof path);
    int rc = lp_image_open(&doc, path);    /* -ENOTSUP without gdk-pixbuf, -EINVAL with it */
    LP_ASSERT(rc == -ENOTSUP || rc == -EINVAL);
    LP_ASSERT_EQ(doc.page_count, 0);
    lp_image_close(&doc);
}

LP_TEST(steps_to_the_neighbouring_pictures_and_pdfs) {
    char dir[600];
    path_of("album", dir, sizeof dir);
    mkdir(dir, 0755);
    write_picture("album/a.png", 4, 4);
    write_bytes("album/b.pdf", "%PDF-1.4");
    write_bytes("album/c.txt", "words");
    write_bytes("album/d.jpg", "jpeg");
    write_picture("album/.hidden.png", 4, 4);
    char a[700], out[700];
    snprintf(a, sizeof a, "%s/a.png", dir);
    LP_ASSERT_EQ(lp_image_sibling(a, 1, out, sizeof out), 1);
    LP_ASSERT_STR(lp_files_basename(out), "b.pdf");
    LP_ASSERT_EQ(lp_image_sibling(out, 1, out, sizeof out), 1);
    LP_ASSERT_STR(lp_files_basename(out), "d.jpg");   /* the text file is skipped */
    LP_ASSERT_EQ(lp_image_sibling(out, 1, out, sizeof out), 1);
    LP_ASSERT_STR(lp_files_basename(out), "a.png");   /* and it wraps, past the hidden one */
    LP_ASSERT_EQ(lp_image_sibling(a, -1, out, sizeof out), 1);
    LP_ASSERT_STR(lp_files_basename(out), "d.jpg");
    char solo[600], only[700];
    path_of("solo", solo, sizeof solo);
    mkdir(solo, 0755);
    write_picture("solo/only.png", 4, 4);
    snprintf(only, sizeof only, "%s/only.png", solo);
    LP_ASSERT_EQ(lp_image_sibling(only, 1, out, sizeof out), 0);
    LP_ASSERT(lp_image_kind_supported(LP_FILE_PDF));
    LP_ASSERT(!lp_image_kind_supported(LP_FILE_VIDEO));
}

LP_TEST(opens_a_pdf_page_by_page) {
    char path[600];
    path_of("two-pages.pdf", path, sizeof path);
#if CAIRO_HAS_PDF_SURFACE
    cairo_surface_t *pdf = cairo_pdf_surface_create(path, 200, 100);
    cairo_t *cr = cairo_create(pdf);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_paint(cr);
    cairo_show_page(cr);
    cairo_pdf_surface_set_size(pdf, 100, 300);
    cairo_set_source_rgb(cr, 0, 0, 1);
    cairo_paint(cr);
    cairo_show_page(cr);
    cairo_destroy(cr);
    cairo_surface_finish(pdf);
    cairo_surface_destroy(pdf);
#else
    write_bytes("two-pages.pdf", "%PDF-1.4");
#endif
    lp_image_doc doc;
    int rc = lp_image_open(&doc, path);
#if defined(HAVE_POPPLER) && CAIRO_HAS_PDF_SURFACE
    LP_ASSERT_EQ(rc, 0);
    LP_ASSERT_EQ(doc.type, LP_IMAGE_PDF);
    LP_ASSERT_EQ(doc.page_count, 2);
    lp_size s = lp_image_page_size(&doc, 1);
    LP_ASSERT_NEAR(s.w, 100, 0.5);
    LP_ASSERT_NEAR(s.h, 300, 0.5);
    cairo_surface_t *out = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 10, 30);
    cr = cairo_create(out);
    lp_image_draw(cr, &doc, 1, LP_RECT(0, 0, 10, 30), 0);
    LP_ASSERT_EQ(colour_at(out, 5, 15), 'b');
    cairo_destroy(cr);
    cairo_surface_destroy(out);
#elif !defined(HAVE_POPPLER)
    LP_ASSERT_EQ(rc, -ENOTSUP);
#endif
    lp_image_close(&doc);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_image_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    LP_RUN(fits_inside_the_viewport_and_never_enlarges);
    LP_RUN(steps_the_zoom_through_fixed_scales);
    LP_RUN(opens_a_png_and_reports_its_size);
    LP_RUN(draws_a_page_turned_in_quarter_turns);
    LP_RUN(refuses_what_it_cannot_open);
    LP_RUN(steps_to_the_neighbouring_pictures_and_pdfs);
    LP_RUN(opens_a_pdf_page_by_page);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
