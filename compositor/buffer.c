/* A wlr_buffer backed by a cairo image surface, for everything the compositor paints itself
 * (decorations, shadows, the fallback wallpaper, the switcher, the lock screen). */
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <math.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>

#include "server.h"

struct cairo_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
	cairo_t *cr;
};

static void cb_destroy(struct wlr_buffer *wb)
{
	struct cairo_buffer *b = wl_container_of(wb, b, base);
	if (b->cr)
		cairo_destroy(b->cr);
	cairo_surface_destroy(b->surface);
	wlr_buffer_finish(wb);
	free(b);
}

static bool cb_begin(struct wlr_buffer *wb, uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
	struct cairo_buffer *b = wl_container_of(wb, b, base);
	(void)flags;
	cairo_surface_flush(b->surface);
	*data = cairo_image_surface_get_data(b->surface);
	*format = DRM_FORMAT_ARGB8888;
	*stride = (size_t)cairo_image_surface_get_stride(b->surface);
	return true;
}

static void cb_end(struct wlr_buffer *wb)
{
	(void)wb;
}

static const struct wlr_buffer_impl cairo_buffer_impl = {
	.destroy = cb_destroy,
	.begin_data_ptr_access = cb_begin,
	.end_data_ptr_access = cb_end,
};

/* Returns a buffer of w x h logical pixels rendered at `scale`; draw with *cr_out in logical units,
 * then call cairo_buffer_done(). */
struct wlr_buffer *cairo_buffer_create(int w, int h, float scale, cairo_t **cr_out)
{
	if (w <= 0 || h <= 0)
		return NULL;
	if (scale <= 0)
		scale = 1;
	int pw = (int)ceil(w * scale), ph = (int)ceil(h * scale);
	struct cairo_buffer *b = calloc(1, sizeof *b);
	b->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
	if (cairo_surface_status(b->surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(b->surface);
		free(b);
		return NULL;
	}
	cairo_surface_set_device_scale(b->surface, scale, scale);
	wlr_buffer_init(&b->base, &cairo_buffer_impl, pw, ph);
	b->cr = cairo_create(b->surface);
	if (cr_out)
		*cr_out = b->cr;
	return &b->base;
}

void cairo_buffer_done(struct wlr_buffer *wb)
{
	struct cairo_buffer *b = wl_container_of(wb, b, base);
	if (b->cr) {
		cairo_destroy(b->cr);
		b->cr = NULL;
	}
	cairo_surface_flush(b->surface);
}
