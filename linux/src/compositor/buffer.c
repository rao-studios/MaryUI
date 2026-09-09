/* Cairo image surfaces as wlr_buffers. begin_data_ptr_access hands the
 * renderer the surface's pixels: cairo's ARGB32 is premultiplied and, on a
 * little-endian machine, laid out exactly like DRM_FORMAT_ARGB8888. */
#include <stdlib.h>
#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>

#include "server.h"

static void buffer_destroy(struct wlr_buffer *wlr_buffer) {
    struct lp_cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
    cairo_surface_destroy(buffer->surface);
    free(buffer);
}

static bool buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer, uint32_t flags,
        void **data, uint32_t *format, size_t *stride) {
    struct lp_cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
    cairo_surface_flush(buffer->surface);
    *data = cairo_image_surface_get_data(buffer->surface);
    *format = DRM_FORMAT_ARGB8888;
    *stride = (size_t)cairo_image_surface_get_stride(buffer->surface);
    return *data != NULL;
}

static void buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
    struct lp_cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
    cairo_surface_mark_dirty(buffer->surface);
}

static const struct wlr_buffer_impl cairo_buffer_impl = {
    .destroy = buffer_destroy,
    .begin_data_ptr_access = buffer_begin_data_ptr_access,
    .end_data_ptr_access = buffer_end_data_ptr_access,
};

struct lp_cairo_buffer *lp_cairo_buffer_from_surface(cairo_surface_t *surface) {
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS
            || cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE
            || cairo_image_surface_get_format(surface) != CAIRO_FORMAT_ARGB32) {
        wlr_log(WLR_ERROR, "lp_cairo_buffer: not an ARGB32 image surface");
        return NULL;
    }
    struct lp_cairo_buffer *buffer = calloc(1, sizeof(*buffer));
    if (!buffer) return NULL;
    buffer->surface = cairo_surface_reference(surface);
    wlr_buffer_init(&buffer->base, &cairo_buffer_impl,
        cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
    return buffer;
}

struct lp_cairo_buffer *lp_cairo_buffer_create(int width, int height) {
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    struct lp_cairo_buffer *buffer = lp_cairo_buffer_from_surface(surface);
    cairo_surface_destroy(surface);
    return buffer;
}
