/* Server-side window decorations, painted by common/paint.c in the active theme. */
#define _GNU_SOURCE
#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>

#include "../common/paint.h"
#include "server.h"

static char decor_marker;     /* node->data of decoration trees */

static float view_scale(struct chrono_view *v)
{
	struct chrono_output *o = output_of_view(v);
	return o ? o->wlr_output->scale : 1.0f;
}

static bool show_frame(struct chrono_view *v)
{
	return v->ssd && !v->fullscreen && v->server->theme.title_height + v->server->theme.border_width > 0;
}

void decor_offsets(struct chrono_view *v, int *left, int *top, int *right, int *bottom)
{
	*left = *top = *right = *bottom = 0;
	if (!show_frame(v))
		return;
	struct chrono_decor_geom g;
	chrono_decor_geometry(&v->server->theme, v->w > 0 ? v->w : 200, v->title, &g);
	*left = *right = *bottom = g.border;
	*top = g.top;
}

/* ---------------------------------------------------------------------------------- shadow template */
void decor_rebuild_shadow_templates(struct chrono_server *s)
{
	for (int i = 0; i < 2; i++) {
		if (s->shadow_tmpl[i]) {
			wlr_buffer_drop(s->shadow_tmpl[i]);
			s->shadow_tmpl[i] = NULL;
		}
	}
	const struct chrono_theme *t = &s->theme;
	if (!t->shadow || t->shadow_size <= 0)
		return;
	int sh = t->shadow_size, R = t->corner_radius;
	int f = 2 * (sh + R) + 2;          /* a small frame; its middle columns/rows get stretched */
	int B = f + 2 * sh;
	s->shadow_tmpl_size = B;
	s->shadow_corner = 2 * sh + R;
	for (int i = 0; i < 2; i++) {
		cairo_t *cr;
		struct wlr_buffer *buf = cairo_buffer_create(B, B, 1, &cr);
		if (!buf)
			continue;
		chrono_paint_shadow(cr, t, f, f, i == 1);
		/* hollow it out: translucent windows must not show the shadow through themselves */
		cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
		double r = fmin(R, f / 2.0);
		if (r > 0) {
			cairo_new_sub_path(cr);
			cairo_arc(cr, sh + f - r, sh + r, r, -M_PI / 2, 0);
			cairo_arc(cr, sh + f - r, sh + f - r, r, 0, M_PI / 2);
			cairo_arc(cr, sh + r, sh + f - r, r, M_PI / 2, M_PI);
			cairo_arc(cr, sh + r, sh + r, r, M_PI, 1.5 * M_PI);
			cairo_close_path(cr);
		} else {
			cairo_rectangle(cr, sh, sh, f, f);
		}
		cairo_fill(cr);
		cairo_buffer_done(buf);
		s->shadow_tmpl[i] = buf;
	}
}

/* ---------------------------------------------------------------------------------- lifecycle */
void decor_create(struct chrono_view *v)
{
	struct decor *d = &v->decor;
	memset(d, 0, sizeof *d);
	d->hover = d->pressed = -1;
	d->painted_hover = d->painted_pressed = -2;
	d->tree = wlr_scene_tree_create(v->tree);
	d->tree->node.data = &decor_marker;
	wlr_scene_node_lower_to_bottom(&d->tree->node);
	d->shadow_tree = wlr_scene_tree_create(d->tree);
	for (int i = 0; i < 8; i++)
		d->shadow[i] = wlr_scene_buffer_create(d->shadow_tree, NULL);
	d->top = wlr_scene_buffer_create(d->tree, NULL);
	d->left = wlr_scene_buffer_create(d->tree, NULL);
	d->right = wlr_scene_buffer_create(d->tree, NULL);
	d->bottom = wlr_scene_buffer_create(d->tree, NULL);
}

void decor_destroy(struct chrono_view *v)
{
	free(v->decor.painted_title);
	v->decor.painted_title = NULL;
	/* scene nodes go away with the view tree */
}

bool decor_node_is_frame(struct wlr_scene_node *node);
bool decor_node_is_frame(struct wlr_scene_node *node)
{
	for (struct wlr_scene_tree *t = node->parent; t; t = t->node.parent)
		if (t->node.data == &decor_marker)
			return true;
	return false;
}

static void set_buffer(struct wlr_scene_buffer *sb, struct wlr_buffer *buf, int w, int h)
{
	wlr_scene_buffer_set_buffer(sb, buf);
	wlr_scene_buffer_set_dest_size(sb, w, h);
	if (buf)
		wlr_buffer_drop(buf);
}

static void place_shadow(struct chrono_view *v, int W, int H)
{
	struct chrono_server *s = v->server;
	struct decor *d = &v->decor;
	bool show = s->theme.shadow && s->shadow_tmpl[0] && !v->maximized && !v->snap && !v->fullscreen;
	wlr_scene_node_set_enabled(&d->shadow_tree->node, show);
	if (!show)
		return;
	struct wlr_buffer *tmpl = s->shadow_tmpl[v->active ? 1 : 0];
	int sh = s->theme.shadow_size, B = s->shadow_tmpl_size, c = s->shadow_corner;
	if (c * 2 > B)
		c = B / 2;
	int mid = B - 2 * c;
	int ew = W + 2 * sh - 2 * c, eh = H + 2 * sh - 2 * c;
	struct { double sx, sy, sw, sh; int x, y, w, h; } p[8] = {
		{0, 0, c, c, -sh, -sh, c, c},
		{B - c, 0, c, c, W + sh - c, -sh, c, c},
		{0, B - c, c, c, -sh, H + sh - c, c, c},
		{B - c, B - c, c, c, W + sh - c, H + sh - c, c, c},
		{c, 0, mid, c, -sh + c, -sh, ew, c},
		{c, B - c, mid, c, -sh + c, H + sh - c, ew, c},
		{0, c, c, mid, -sh, -sh + c, c, eh},
		{B - c, c, c, mid, W + sh - c, -sh + c, c, eh},
	};
	for (int i = 0; i < 8; i++) {
		struct wlr_scene_buffer *sb = d->shadow[i];
		if (p[i].w <= 0 || p[i].h <= 0 || mid <= 0) {
			wlr_scene_node_set_enabled(&sb->node, false);
			continue;
		}
		wlr_scene_node_set_enabled(&sb->node, true);
		wlr_scene_buffer_set_buffer(sb, tmpl);
		wlr_scene_buffer_set_source_box(sb, &(struct wlr_fbox){p[i].sx, p[i].sy, p[i].sw, p[i].sh});
		wlr_scene_buffer_set_dest_size(sb, p[i].w, p[i].h);
		wlr_scene_node_set_position(&sb->node, p[i].x, p[i].y);
	}
}

/* ---------------------------------------------------------------------------------- painting */
void decor_update(struct chrono_view *v)
{
	struct chrono_server *s = v->server;
	struct decor *d = &v->decor;
	if (!d->tree)
		return;
	bool show = show_frame(v) && v->w > 0 && v->h > 0;
	wlr_scene_node_set_enabled(&d->tree->node, show);
	if (!show)
		return;
	const struct chrono_theme *t = &s->theme;
	struct chrono_decor_geom g;
	chrono_decor_geometry(t, v->w, v->title, &g);
	int b = g.border, W = v->w + 2 * b, H = g.top + v->h + b;
	float scale = view_scale(v);
	const char *title = v->title ? v->title : "";

	bool same_title = d->painted_title && !strcmp(d->painted_title, title);
	bool theme_changed = d->painted_theme_gen != s->theme_gen;
	bool size_changed = d->painted_w != v->w;
	bool state_changed = d->painted_active != v->active;

	if (g.top > 0 && (theme_changed || size_changed || state_changed || !same_title || d->painted_hover != d->hover ||
			  d->painted_pressed != d->pressed)) {
		cairo_t *cr;
		struct wlr_buffer *buf = cairo_buffer_create(W, g.top, scale, &cr);
		if (buf) {
			chrono_decor_paint_top(cr, t, v->w, title, v->active, d->hover, d->pressed);
			cairo_buffer_done(buf);
			set_buffer(d->top, buf, W, g.top);
		}
		wlr_scene_node_set_position(&d->top->node, 0, 0);
	}
	wlr_scene_node_set_enabled(&d->top->node, g.top > 0);

	if (b > 0 && (theme_changed || state_changed || size_changed || d->painted_h != v->h)) {
		struct { struct wlr_scene_buffer *sb; enum chrono_edge e; int x, y, w, h; } e[3] = {
			{d->left, EDGE_LEFT, 0, g.top, b, v->h},
			{d->right, EDGE_RIGHT, W - b, g.top, b, v->h},
			{d->bottom, EDGE_BOTTOM, 0, g.top + v->h, W, b},
		};
		for (int i = 0; i < 3; i++) {
			cairo_t *cr;
			struct wlr_buffer *buf = cairo_buffer_create(e[i].w, e[i].h, scale, &cr);
			if (!buf)
				continue;
			chrono_decor_paint_edge(cr, t, e[i].e, e[i].w, e[i].h, v->active);
			cairo_buffer_done(buf);
			set_buffer(e[i].sb, buf, e[i].w, e[i].h);
			wlr_scene_node_set_position(&e[i].sb->node, e[i].x, e[i].y);
		}
	}
	wlr_scene_node_set_enabled(&d->left->node, b > 0);
	wlr_scene_node_set_enabled(&d->right->node, b > 0);
	wlr_scene_node_set_enabled(&d->bottom->node, b > 0);

	place_shadow(v, W, H);

	d->painted_w = v->w;
	d->painted_h = v->h;
	d->painted_active = v->active;
	d->painted_hover = d->hover;
	d->painted_pressed = d->pressed;
	d->painted_theme_gen = s->theme_gen;
	if (!same_title) {
		free(d->painted_title);
		d->painted_title = strdup(title);
	}
}

/* ---------------------------------------------------------------------------------- hit testing */
/* fx, fy relative to the frame's top-left (may be slightly outside for the resize grip) */
struct decor_hit decor_hit_test(struct chrono_view *v, double fx, double fy)
{
	struct decor_hit hit = {PART_NONE, -1, 0};
	if (!show_frame(v))
		return hit;
	const struct chrono_theme *t = &v->server->theme;
	struct chrono_decor_geom g;
	chrono_decor_geometry(t, v->w, v->title, &g);
	int W = v->w + 2 * g.border, H = g.top + v->h + g.border;
	bool resizable = !v->maximized && !v->fullscreen;

	/* buttons */
	if (fy >= 0 && fy < g.top) {
		for (int i = 0; i < g.nbuttons; i++) {
			struct chrono_button_rect *bt = &g.buttons[i];
			if (fx >= bt->x - 2 && fx < bt->x + bt->w + 2 && fy >= bt->y - 2 && fy < bt->y + bt->h + 2) {
				hit.part = PART_BUTTON;
				hit.button = i;
				return hit;
			}
		}
	}
	/* resize edges: a generous grip, wider at the corners */
	if (resizable) {
		int grip = g.border > 6 ? g.border : 6, corner = 18;
		uint32_t e = 0;
		bool near_l = fx < grip, near_r = fx >= W - grip, near_t = fy < (g.title_y > 0 ? g.border : 3),
		     near_b = fy >= H - grip;
		if (near_l || near_r) {
			if (fy < corner) e |= WLR_EDGE_TOP;
			if (fy >= H - corner) e |= WLR_EDGE_BOTTOM;
		}
		if (near_t || near_b) {
			if (fx < corner) e |= WLR_EDGE_LEFT;
			if (fx >= W - corner) e |= WLR_EDGE_RIGHT;
		}
		if (near_l) e |= WLR_EDGE_LEFT;
		if (near_r) e |= WLR_EDGE_RIGHT;
		if (near_t) e |= WLR_EDGE_TOP;
		if (near_b) e |= WLR_EDGE_BOTTOM;
		if (fx < 0 || fy < 0 || fx >= W || fy >= H || e) {
			if (e) {
				hit.part = PART_EDGE;
				hit.edges = e;
				return hit;
			}
		}
	}
	if (fy >= 0 && fy < g.top && fx >= 0 && fx < (g.tab_w ? g.tab_w : W)) {
		hit.part = PART_TITLE;
		return hit;
	}
	return hit;
}
