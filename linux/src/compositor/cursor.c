/* The pointer images, drawn with Cairo at startup: no cursor theme package
 * is needed and the arrow matches the metal. */
#include <math.h>

#include "server.h"

/* The platinum arrow: a white blade with a dark rim and a soft drop shadow,
 * 24 px, hotspot at its tip. */
static struct mui_cursor_image draw_arrow(void) {
    struct mui_cursor_image image = { .buffer = lp_cairo_buffer_create(28, 28), .hotspot_x = 3, .hotspot_y = 3 };
    if (!image.buffer) return image;
    cairo_t *cr = cairo_create(image.buffer->surface);
    static const double pts[][2] = {
        { 3, 3 }, { 3, 21.5 }, { 8, 17 }, { 11.2, 24.2 }, { 14.4, 22.8 }, { 11.2, 15.8 }, { 18, 15.8 },
    };
    for (int pass = 0; pass < 2; pass++) {
        cairo_new_path(cr);
        for (size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); i++) {
            double x = pts[i][0], y = pts[i][1];
            if (pass == 0) { x += 1; y += 1.5; }
            if (i == 0) cairo_move_to(cr, x, y); else cairo_line_to(cr, x, y);
        }
        cairo_close_path(cr);
        if (pass == 0) {
            cairo_set_source_rgba(cr, 0, 0, 0, 0.28);
            cairo_fill(cr);
        } else {
            cairo_set_source_rgb(cr, 0.969, 0.969, 0.976); /* platinum.0 */
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.122, 0.133, 0.157, 0.9); /* ink.primary */
            cairo_set_line_width(cr, 1.2);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_stroke(cr);
        }
    }
    cairo_destroy(cr);
    cairo_surface_flush(image.buffer->surface);
    return image;
}

/* A double-headed resize arrow along `angle` (degrees, 0 = horizontal). */
static struct mui_cursor_image draw_resize(double angle_deg) {
    struct mui_cursor_image image = { .buffer = lp_cairo_buffer_create(32, 32), .hotspot_x = 16, .hotspot_y = 16 };
    if (!image.buffer) return image;
    cairo_t *cr = cairo_create(image.buffer->surface);
    cairo_translate(cr, 16, 16);
    cairo_rotate(cr, angle_deg * M_PI / 180.0);
    for (int pass = 0; pass < 2; pass++) {
        cairo_new_path(cr);
        cairo_move_to(cr, -12, 0); cairo_line_to(cr, -6, -5); cairo_line_to(cr, -6, -2); cairo_line_to(cr, 6, -2);
        cairo_line_to(cr, 6, -5); cairo_line_to(cr, 12, 0); cairo_line_to(cr, 6, 5); cairo_line_to(cr, 6, 2);
        cairo_line_to(cr, -6, 2); cairo_line_to(cr, -6, 5); cairo_close_path(cr);
        if (pass == 0) {
            cairo_set_source_rgba(cr, 0.122, 0.133, 0.157, 0.9);
            cairo_set_line_width(cr, 3);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_stroke(cr);
        } else {
            cairo_set_source_rgb(cr, 0.969, 0.969, 0.976);
            cairo_fill(cr);
        }
    }
    cairo_destroy(cr);
    cairo_surface_flush(image.buffer->surface);
    return image;
}

/* The I-beam over text: a platinum stem with serifs, ink rim, soft shadow; hotspot at its centre. */
static struct mui_cursor_image draw_ibeam(void) {
    struct mui_cursor_image image = { .buffer = lp_cairo_buffer_create(24, 24), .hotspot_x = 12, .hotspot_y = 12 };
    if (!image.buffer) return image;
    cairo_t *cr = cairo_create(image.buffer->surface);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int pass = 0; pass < 3; pass++) {
        double dx = pass == 0 ? 1 : 0, dy = pass == 0 ? 1.5 : 0;
        cairo_new_path(cr);
        cairo_move_to(cr, 12 + dx, 4 + dy); cairo_line_to(cr, 12 + dx, 20 + dy);
        cairo_move_to(cr, 9 + dx, 4 + dy); cairo_line_to(cr, 15 + dx, 4 + dy);
        cairo_move_to(cr, 9 + dx, 20 + dy); cairo_line_to(cr, 15 + dx, 20 + dy);
        if (pass == 0) { cairo_set_source_rgba(cr, 0, 0, 0, 0.28); cairo_set_line_width(cr, 3.4); }
        else if (pass == 1) { cairo_set_source_rgba(cr, 0.122, 0.133, 0.157, 0.9); cairo_set_line_width(cr, 3.2); }
        else { cairo_set_source_rgb(cr, 0.969, 0.969, 0.976); cairo_set_line_width(cr, 1.4); }
        cairo_stroke(cr);
    }
    cairo_destroy(cr);
    cairo_surface_flush(image.buffer->surface);
    return image;
}

void mui_cursor_init(struct mui_server *server) {
    server->cursors[MUI_CURSOR_ARROW] = draw_arrow();
    server->cursors[MUI_CURSOR_EW] = draw_resize(0);
    server->cursors[MUI_CURSOR_NS] = draw_resize(90);
    server->cursors[MUI_CURSOR_NWSE] = draw_resize(45);
    server->cursors[MUI_CURSOR_NESW] = draw_resize(-45);
    server->cursors[MUI_CURSOR_TEXT] = draw_ibeam();
    server->cursor_shape = MUI_CURSOR_COUNT;
    mui_cursor_set_shape(server, MUI_CURSOR_ARROW);
}

void mui_cursor_set_shape(struct mui_server *server, enum mui_cursor_shape shape) {
    if (shape >= MUI_CURSOR_COUNT) shape = MUI_CURSOR_ARROW;
    if (server->cursor_shape == shape) return;
    struct mui_cursor_image *image = &server->cursors[shape];
    if (!image->buffer) return;
    server->cursor_shape = shape;
    wlr_cursor_set_buffer(server->cursor, &image->buffer->base, image->hotspot_x, image->hotspot_y, 1.0f);
}

void mui_cursor_finish(struct mui_server *server) {
    for (int i = 0; i < MUI_CURSOR_COUNT; i++) {
        if (server->cursors[i].buffer) wlr_buffer_drop(&server->cursors[i].buffer->base);
        server->cursors[i].buffer = NULL;
    }
}
