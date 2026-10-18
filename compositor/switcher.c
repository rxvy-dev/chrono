/* Alt+Tab: a themed window list in the middle of the screen. */
#define _GNU_SOURCE
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

#include "../common/paint.h"
#include "server.h"

#define ROW_H 40
#define SW_W 520

static void draw(struct chrono_server *s)
{
	struct switcher *sw = &s->switcher;
	struct chrono_output *o = output_at(s, s->cursor->x, s->cursor->y);
	if (!o || sw->count == 0)
		return;
	const struct chrono_theme *t = &s->theme;
	int rows = sw->count > 12 ? 12 : sw->count;
	int first = sw->index >= rows ? sw->index - rows + 1 : 0;
	int H = rows * ROW_H + 24, W = SW_W;
	if (W > o->full.width - 40)
		W = o->full.width - 40;
	cairo_t *cr;
	struct wlr_buffer *buf = cairo_buffer_create(W, H, o->wlr_output->scale, &cr);
	if (!buf)
		return;
	bool bevel = !strcmp(t->panel_style, "bevel") || !strcmp(t->panel_style, "pixel");
	double r = bevel ? 0 : 14;
	cairo_new_sub_path(cr);
	if (r > 0) {
		cairo_arc(cr, W - r, r, r, -M_PI / 2, 0);
		cairo_arc(cr, W - r, H - r, r, 0, M_PI / 2);
		cairo_arc(cr, r, H - r, r, M_PI / 2, M_PI);
		cairo_arc(cr, r, r, r, M_PI, 1.5 * M_PI);
		cairo_close_path(cr);
	} else {
		cairo_rectangle(cr, 0, 0, W, H);
	}
	struct chrono_rgba bg = t->panel_bg;
	if (bg.a < 0.92)
		bg.a = 0.92;
	cairo_set_source_rgba(cr, bg.r, bg.g, bg.b, bg.a);
	cairo_fill(cr);
	if (bevel)
		chrono_paint_bevel(cr, 0, 0, W, H, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, false);

	PangoLayout *l = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(t->font_ui);
	pango_layout_set_font_description(l, fd);
	pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_END);
	pango_layout_set_width(l, (W - 60) * PANGO_SCALE);
	for (int i = 0; i < rows; i++) {
		int idx = first + i;
		struct chrono_view *v = view_by_id(s, sw->ids[idx]);
		if (!v)
			continue;
		double y = 12 + i * ROW_H;
		bool sel = idx == sw->index;
		if (sel) {
			cairo_new_sub_path(cr);
			double rr = bevel ? 0 : 8, x0 = 10, w0 = W - 20, h0 = ROW_H - 4;
			if (rr > 0) {
				cairo_arc(cr, x0 + w0 - rr, y + rr, rr, -M_PI / 2, 0);
				cairo_arc(cr, x0 + w0 - rr, y + h0 - rr, rr, 0, M_PI / 2);
				cairo_arc(cr, x0 + rr, y + h0 - rr, rr, M_PI / 2, M_PI);
				cairo_arc(cr, x0 + rr, y + rr, rr, M_PI, 1.5 * M_PI);
				cairo_close_path(cr);
			} else {
				cairo_rectangle(cr, x0, y, w0, h0);
			}
			cairo_set_source_rgba(cr, t->accent.r, t->accent.g, t->accent.b, 1);
			cairo_fill(cr);
		}
		/* a coloured dot standing in for the app icon */
		unsigned hsh = 5381;
		for (const char *p = v->app_id ? v->app_id : ""; *p; p++)
			hsh = hsh * 33 + (unsigned char)*p;
		cairo_arc(cr, 30, y + (ROW_H - 4) / 2.0, 9, 0, 2 * M_PI);
		cairo_set_source_rgba(cr, ((hsh >> 16) & 255) / 255.0 * 0.6 + 0.3, ((hsh >> 8) & 255) / 255.0 * 0.6 + 0.3,
				      (hsh & 255) / 255.0 * 0.6 + 0.3, 1);
		cairo_fill(cr);
		char text[600];
		snprintf(text, sizeof text, "%s%s", v->minimized ? "▾ " : "", v->title && *v->title ? v->title : v->app_id);
		pango_layout_set_text(l, text, -1);
		int tw, th;
		pango_layout_get_pixel_size(l, &tw, &th);
		struct chrono_rgba fg = sel ? t->accent_fg : t->panel_fg;
		cairo_set_source_rgba(cr, fg.r, fg.g, fg.b, 1);
		cairo_move_to(cr, 50, y + (ROW_H - 4 - th) / 2.0);
		pango_cairo_show_layout(cr, l);
		cairo_new_path(cr);
	}
	pango_font_description_free(fd);
	g_object_unref(l);
	cairo_buffer_done(buf);

	if (!sw->buf)
		sw->buf = wlr_scene_buffer_create(s->layers[L_UI], NULL);
	wlr_scene_buffer_set_buffer(sw->buf, buf);
	wlr_scene_buffer_set_dest_size(sw->buf, W, H);
	wlr_buffer_drop(buf);
	wlr_scene_node_set_position(&sw->buf->node, o->full.x + (o->full.width - W) / 2, o->full.y + (o->full.height - H) / 2);
	wlr_scene_node_set_enabled(&sw->buf->node, true);
}

void switcher_next(struct chrono_server *s, int dir)
{
	struct switcher *sw = &s->switcher;
	if (lock_active(s))
		return;
	if (!sw->active) {
		sw->count = 0;
		struct chrono_view *v;
		wl_list_for_each(v, &s->views, link)
			if (v->mapped && v->workspace == s->workspace && sw->count < 256)
				sw->ids[sw->count++] = v->id;
		if (sw->count == 0)
			return;
		sw->active = true;
		sw->index = 0;
	}
	sw->index = (sw->index + dir + sw->count) % sw->count;
	draw(s);
}

static void hide(struct chrono_server *s)
{
	s->switcher.active = false;
	if (s->switcher.buf)
		wlr_scene_node_set_enabled(&s->switcher.buf->node, false);
}

void switcher_commit(struct chrono_server *s)
{
	struct switcher *sw = &s->switcher;
	if (!sw->active)
		return;
	struct chrono_view *v = view_by_id(s, sw->ids[sw->index]);
	hide(s);
	if (v)
		view_focus(v);
}

void switcher_cancel(struct chrono_server *s)
{
	hide(s);
}
