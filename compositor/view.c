/* Windows ("views"): xdg-shell toplevels and popups, focus, stacking, maximize / snap / fullscreen /
 * minimize, workspaces and the foreign-toplevel handles that taskbars use. XWayland windows share all of
 * this through the is_x11 branches (see xwayland.c). */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "server.h"

bool decor_node_is_frame(struct wlr_scene_node *node);

/* ================================================================================ small helpers */
struct chrono_view *view_by_id(struct chrono_server *s, uint32_t id)
{
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link)
		if (v->id == id)
			return v;
	return NULL;
}

static struct chrono_view *view_from_tree(struct chrono_server *s, struct wlr_scene_tree *t)
{
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link)
		if (v->tree == t)
			return v;
	return NULL;
}

static struct wlr_box xdg_geometry(struct chrono_view *v)
{
	struct wlr_box g = {0};
	if (!v->is_x11 && v->xdg)
		g = v->xdg->base->geometry;
	return g;
}

void view_frame_box(struct chrono_view *v, struct wlr_box *box)
{
	int l, t, r, b;
	decor_offsets(v, &l, &t, &r, &b);
	*box = (struct wlr_box){v->x - l, v->y - t, v->w + l + r, v->h + t + b};
}

static bool view_visible(struct chrono_view *v)
{
	return v->mapped && !v->minimized && v->workspace == v->server->workspace;
}

static void view_update_visibility(struct chrono_view *v)
{
	wlr_scene_node_set_enabled(&v->tree->node, view_visible(v));
}

void view_update_position(struct chrono_view *v)
{
	int l, t, r, b;
	decor_offsets(v, &l, &t, &r, &b);
	wlr_scene_node_set_position(&v->tree->node, v->x - l, v->y - t);
	struct wlr_box g = xdg_geometry(v);
	wlr_scene_node_set_position(&v->content->node, l - g.x, t - g.y);
}

/* tell the client its new size (and, for X11, position) */
static void view_configure(struct chrono_view *v, int x, int y, int w, int h)
{
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	v->x = x;
	v->y = y;
	if (v->is_x11) {
#ifdef CHRONO_HAVE_XWAYLAND
		wlr_xwayland_surface_configure(v->xsurface, (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h);
		v->w = w;
		v->h = h;
#endif
	} else if (v->xdg && v->xdg->base->initialized) {
		v->pending_serial = wlr_xdg_toplevel_set_size(v->xdg, w, h);
	}
	view_update_position(v);
	decor_update(v);
}

static void view_set_activated(struct chrono_view *v, bool on)
{
	v->active = on;
	if (v->is_x11) {
#ifdef CHRONO_HAVE_XWAYLAND
		wlr_xwayland_surface_activate(v->xsurface, on);
#endif
	} else if (v->xdg && v->xdg->base->initialized) {
		wlr_xdg_toplevel_set_activated(v->xdg, on);
	}
	if (v->ftl)
		wlr_foreign_toplevel_handle_v1_set_activated(v->ftl, on);
	decor_update(v);
}

static void ftl_update_states(struct chrono_view *v)
{
	if (!v->ftl)
		return;
	wlr_foreign_toplevel_handle_v1_set_maximized(v->ftl, v->maximized);
	wlr_foreign_toplevel_handle_v1_set_minimized(v->ftl, v->minimized);
	wlr_foreign_toplevel_handle_v1_set_fullscreen(v->ftl, v->fullscreen);
}

static void windows_changed(struct chrono_server *s)
{
	ipc_event(s, "windows", NULL);
}

/* the usable area of the view's output (or the one under the cursor) */
static struct chrono_output *target_output(struct chrono_view *v)
{
	struct chrono_output *o = output_of_view(v);
	if (!o)
		o = output_at(v->server, v->server->cursor->x, v->server->cursor->y);
	return o;
}

/* ================================================================================ focus + stacking */
void view_focus(struct chrono_view *v)
{
	if (!v)
		return;
	struct chrono_server *s = v->server;
	if (lock_active(s))
		return;
	if (v->minimized)
		view_set_minimized(v, false);
	if (v->workspace != s->workspace)
		workspace_switch(s, v->workspace);

	/* raise */
	wl_list_remove(&v->link);
	wl_list_insert(&s->views, &v->link);
	wlr_scene_node_raise_to_top(&v->tree->node);

	if (s->focused != v) {
		if (s->focused)
			view_set_activated(s->focused, false);
		s->focused = v;
		view_set_activated(v, true);
	}
	/* a layer surface that grabbed the keyboard (launcher, power menu) keeps it */
	if (!layer_exclusive_keyboard(s))
		keyboard_focus_surface(s, v->surface);
	windows_changed(s);
}

void view_focus_top(struct chrono_server *s)
{
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link) {
		if (view_visible(v)) {
			view_focus(v);
			return;
		}
	}
	if (s->focused)
		view_set_activated(s->focused, false);
	s->focused = NULL;
	if (!layer_exclusive_keyboard(s))
		wlr_seat_keyboard_notify_clear_focus(s->seat);
	windows_changed(s);
}

/* ================================================================================ hit testing */
struct chrono_view *view_at(struct chrono_server *s, double lx, double ly, struct wlr_surface **surface,
			    double *sx, double *sy, struct decor_hit *hit)
{
	*surface = NULL;
	hit->part = PART_NONE;
	struct wlr_scene_node *node = wlr_scene_node_at(&s->scene->tree.node, lx, ly, sx, sy);
	struct chrono_view *v = NULL;
	if (node) {
		if (node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_buffer *sb = wlr_scene_buffer_from_node(node);
			struct wlr_scene_surface *ss = wlr_scene_surface_try_from_buffer(sb);
			if (ss)
				*surface = ss->surface;
		}
		for (struct wlr_scene_tree *t = node->parent; t && !v; t = t->node.parent)
			v = view_from_tree(s, t);
		if (v && decor_node_is_frame(node)) {
			*surface = NULL;
			struct wlr_box fb;
			view_frame_box(v, &fb);
			*hit = decor_hit_test(v, lx - fb.x, ly - fb.y);
		}
		if (*surface || v)
			return v;
	}
	/* nothing there: maybe the invisible resize grip just outside a frame */
	wl_list_for_each(v, &s->views, link) {
		if (!view_visible(v) || !v->ssd || v->fullscreen || v->maximized)
			continue;
		struct wlr_box fb;
		view_frame_box(v, &fb);
		const int grip = 6;
		if (lx >= fb.x - grip && lx < fb.x + fb.width + grip && ly >= fb.y - grip && ly < fb.y + fb.height + grip) {
			struct decor_hit h = decor_hit_test(v, lx - fb.x, ly - fb.y);
			if (h.part == PART_EDGE) {
				*hit = h;
				return v;
			}
		}
	}
	return NULL;
}

/* ================================================================================ geometry ops */
void view_move_to(struct chrono_view *v, int x, int y)
{
	v->x = x;
	v->y = y;
	if (v->is_x11)
		view_configure(v, x, y, v->w, v->h);
	else
		view_update_position(v);
}

void view_resize(struct chrono_view *v, int x, int y, int w, int h)
{
	view_configure(v, x, y, w, h);
}

static void save_restore(struct chrono_view *v)
{
	if (!v->maximized && !v->snap && !v->fullscreen)
		v->restore = (struct wlr_box){v->x, v->y, v->w, v->h};
}

static void clear_tiling(struct chrono_view *v)
{
	if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
		wlr_xdg_toplevel_set_tiled(v->xdg, WLR_EDGE_NONE);
}

/* fit the content box of `v` into `area` (layout coords), accounting for its frame */
static void place_in(struct chrono_view *v, struct wlr_box area)
{
	int l, t, r, b;
	decor_offsets(v, &l, &t, &r, &b);
	int gap = v->server->config.gaps;
	area.x += gap;
	area.y += gap;
	area.width -= 2 * gap;
	area.height -= 2 * gap;
	view_configure(v, area.x + l, area.y + t, area.width - l - r, area.height - t - b);
}

void view_set_maximized(struct chrono_view *v, bool on)
{
	if (v->fullscreen || (on == v->maximized && !v->snap))
		goto out;
	struct chrono_output *o = target_output(v);
	if (on) {
		save_restore(v);
		v->maximized = true;
		v->snap = 0;
		if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
			wlr_xdg_toplevel_set_maximized(v->xdg, true);
#ifdef CHRONO_HAVE_XWAYLAND
		if (v->is_x11)
			wlr_xwayland_surface_set_maximized(v->xsurface, true, true);
#endif
		if (o)
			place_in(v, o->usable);
	} else {
		v->maximized = false;
		v->snap = 0;
		clear_tiling(v);
		if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
			wlr_xdg_toplevel_set_maximized(v->xdg, false);
#ifdef CHRONO_HAVE_XWAYLAND
		if (v->is_x11)
			wlr_xwayland_surface_set_maximized(v->xsurface, false, false);
#endif
		if (v->restore.width > 0)
			view_configure(v, v->restore.x, v->restore.y, v->restore.width, v->restore.height);
	}
	ftl_update_states(v);
out:
	if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
		wlr_xdg_surface_schedule_configure(v->xdg->base);
	windows_changed(v->server);
}

void view_snap(struct chrono_view *v, uint32_t edges)
{
	struct chrono_output *o = target_output(v);
	if (!o || v->fullscreen)
		return;
	if (edges == WLR_EDGE_TOP) {
		view_set_maximized(v, true);
		return;
	}
	if (edges == WLR_EDGE_NONE) {
		if (v->maximized)
			view_set_maximized(v, false);
		else if (v->snap) {
			v->snap = 0;
			clear_tiling(v);
			view_configure(v, v->restore.x, v->restore.y, v->restore.width, v->restore.height);
		}
		windows_changed(v->server);
		return;
	}
	save_restore(v);
	if (v->maximized) {
		v->maximized = false;
		if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
			wlr_xdg_toplevel_set_maximized(v->xdg, false);
	}
	struct wlr_box a = o->usable;
	if (edges & WLR_EDGE_LEFT)
		a.width /= 2;
	if (edges & WLR_EDGE_RIGHT) {
		a.x += a.width / 2;
		a.width -= a.width / 2;
	}
	if (edges & WLR_EDGE_TOP)
		a.height /= 2;
	if (edges & WLR_EDGE_BOTTOM) {
		a.y += a.height / 2;
		a.height -= a.height / 2;
	}
	v->snap = (int)edges;
	if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
		wlr_xdg_toplevel_set_tiled(v->xdg, edges);
	place_in(v, a);
	ftl_update_states(v);
	windows_changed(v->server);
}

void view_set_fullscreen(struct chrono_view *v, bool on)
{
	struct chrono_server *s = v->server;
	if (on == v->fullscreen)
		goto out;
	struct chrono_output *o = target_output(v);
	if (on) {
		save_restore(v);
		v->fullscreen = true;
		wlr_scene_node_reparent(&v->tree->node, s->layers[L_FULLSCREEN]);
		if (o)
			view_configure(v, o->full.x, o->full.y, o->full.width, o->full.height);
	} else {
		v->fullscreen = false;
		wlr_scene_node_reparent(&v->tree->node, s->layers[L_VIEWS]);
		if (v->maximized && o) {
			place_in(v, o->usable);
		} else if (v->snap) {
			int snap = v->snap;
			v->snap = 0;
			view_snap(v, (uint32_t)snap);
		} else if (v->restore.width > 0) {
			view_configure(v, v->restore.x, v->restore.y, v->restore.width, v->restore.height);
		}
	}
	if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
		wlr_xdg_toplevel_set_fullscreen(v->xdg, on);
#ifdef CHRONO_HAVE_XWAYLAND
	if (v->is_x11)
		wlr_xwayland_surface_set_fullscreen(v->xsurface, on);
#endif
	decor_update(v);
	ftl_update_states(v);
out:
	if (!v->is_x11 && v->xdg && v->xdg->base->initialized)
		wlr_xdg_surface_schedule_configure(v->xdg->base);
	windows_changed(s);
}

void view_set_minimized(struct chrono_view *v, bool on)
{
	if (on == v->minimized)
		return;
	v->minimized = on;
#ifdef CHRONO_HAVE_XWAYLAND
	if (v->is_x11)
		wlr_xwayland_surface_set_minimized(v->xsurface, on);
#endif
	view_update_visibility(v);
	ftl_update_states(v);
	if (on && v->server->focused == v) {
		view_set_activated(v, false);
		v->server->focused = NULL;
		view_focus_top(v->server);
	}
	windows_changed(v->server);
}

void view_close(struct chrono_view *v)
{
	if (v->is_x11) {
#ifdef CHRONO_HAVE_XWAYLAND
		wlr_xwayland_surface_close(v->xsurface);
#endif
	} else if (v->xdg) {
		wlr_xdg_toplevel_send_close(v->xdg);
	}
}

/* ================================================================================ workspaces */
void workspace_switch(struct chrono_server *s, int ws)
{
	if (ws < 0 || ws >= s->config.workspaces || ws == s->workspace)
		return;
	s->workspace = ws;
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link)
		view_update_visibility(v);
	if (s->focused && s->focused->workspace != ws) {
		view_set_activated(s->focused, false);
		s->focused = NULL;
	}
	view_focus_top(s);
	char payload[32];
	snprintf(payload, sizeof payload, "{\"workspace\":%d}", ws + 1);
	ipc_event(s, "workspace", payload);
}

void view_to_workspace(struct chrono_view *v, int ws)
{
	struct chrono_server *s = v->server;
	if (ws < 0 || ws >= s->config.workspaces)
		return;
	v->workspace = ws;
	view_update_visibility(v);
	if (s->focused == v && ws != s->workspace) {
		view_set_activated(v, false);
		s->focused = NULL;
		view_focus_top(s);
	}
	windows_changed(s);
}

/* ================================================================================ placement */
static void place_new(struct chrono_view *v)
{
	struct chrono_server *s = v->server;
	struct chrono_output *o = output_at(s, s->cursor->x, s->cursor->y);
	if (!o)
		return;
	struct wlr_box a = o->usable;
	int l, t, r, b;
	decor_offsets(v, &l, &t, &r, &b);
	int w = v->w, h = v->h;
	int maxw = a.width - l - r, maxh = a.height - t - b;
	bool too_big = w > maxw || h > maxh;
	if (w <= 1 || h <= 1) {
		w = maxw * 3 / 5;
		h = maxh * 3 / 5;
	}
	if (w > maxw) w = maxw;
	if (h > maxh) h = maxh;

	int x, y;
	struct chrono_view *parent = NULL;
	if (!v->is_x11 && v->xdg && v->xdg->parent) {
		struct chrono_view *p;
		wl_list_for_each(p, &s->views, link)
			if (!p->is_x11 && p->xdg == v->xdg->parent)
				parent = p;
	}
	if (parent) {
		x = parent->x + (parent->w - w) / 2;
		y = parent->y + (parent->h - h) / 2;
	} else if (!strcmp(s->config.placement, "cursor")) {
		x = (int)s->cursor->x - w / 2;
		y = (int)s->cursor->y - h / 2;
	} else {
		x = a.x + l + (maxw - w) / 2;
		y = a.y + t + (maxh - h) / 2;
		if (!strcmp(s->config.placement, "cascade")) {
			int n = 0;
			struct chrono_view *p;
			wl_list_for_each(p, &s->views, link)
				if (p != v && p->mapped && p->workspace == s->workspace)
					n++;
			x = a.x + l + 40 + (n % 8) * 32;
			y = a.y + t + 30 + (n % 8) * 32;
		}
		/* never land exactly on top of another window: step down-right until the spot is free */
		for (int tries = 0; tries < 12; tries++) {
			bool taken = false;
			struct chrono_view *p;
			wl_list_for_each(p, &s->views, link)
				if (p != v && p->mapped && !p->minimized && p->workspace == s->workspace &&
				    abs(p->x - x) < 12 && abs(p->y - y) < 12)
					taken = true;
			if (!taken)
				break;
			x += 32;
			y += 32;
		}
	}
	/* keep the title bar reachable */
	if (x < a.x + l) x = a.x + l;
	if (y < a.y + t) y = a.y + t;
	if (x + w > a.x + a.width - r) x = a.x + a.width - r - w;
	if (y + h > a.y + a.height - b) y = a.y + a.height - b - h;
	v->placed = true;
	if (w != v->w || h != v->h || v->is_x11)
		view_configure(v, x, y, w, h);
	else
		view_move_to(v, x, y);
	if (too_big && !parent)
		view_set_maximized(v, true);
}

/* ================================================================================ lifecycle (shared) */
void view_mapped(struct chrono_view *v)
{
	struct chrono_server *s = v->server;
	v->mapped = true;
	v->workspace = s->workspace;
	if (!v->is_x11) {
		struct wlr_box g = v->xdg->base->geometry;
		if (g.width > 0) {
			v->w = g.width;
			v->h = g.height;
		} else {
			v->w = v->surface->current.width;
			v->h = v->surface->current.height;
		}
	}
	if (!v->placed)
		place_new(v);
	view_update_position(v);
	view_update_visibility(v);
	view_ftl_create(v);
	decor_update(v);
	view_focus(v);
}

void view_unmapped(struct chrono_view *v)
{
	struct chrono_server *s = v->server;
	v->mapped = false;
	view_update_visibility(v);
	if (s->grab_view == v) {
		s->cursor_mode = CURSOR_PASSTHROUGH;
		s->grab_view = NULL;
		if (s->snap_preview)
			wlr_scene_node_set_enabled(&s->snap_preview->node, false);
	}
	if (v->ftl) {
		/* wlroots 0.20 asserts that nobody still listens when a handle is destroyed */
		wl_list_remove(&v->ftl_request_activate.link);
		wl_list_remove(&v->ftl_request_close.link);
		wl_list_remove(&v->ftl_request_minimize.link);
		wl_list_remove(&v->ftl_request_maximize.link);
		wl_list_remove(&v->ftl_request_fullscreen.link);
		wlr_foreign_toplevel_handle_v1_destroy(v->ftl);
		v->ftl = NULL;
	}
	if (v->ext_ftl) {
		wlr_ext_foreign_toplevel_handle_v1_destroy(v->ext_ftl);
		v->ext_ftl = NULL;
	}
	if (s->focused == v) {
		s->focused = NULL;
		view_focus_top(s);
	}
	windows_changed(s);
}

void view_destroyed(struct chrono_view *v)
{
	struct chrono_server *s = v->server;
	if (v->mapped)
		view_unmapped(v);
	if (s->focused == v)
		s->focused = NULL;
	wl_list_remove(&v->link);
	decor_destroy(v);
	wlr_scene_node_destroy(&v->tree->node);
	free(v->title);
	free(v->app_id);
	free(v);
	windows_changed(s);
}

void view_set_title(struct chrono_view *v, const char *title)
{
	free(v->title);
	v->title = strdup(title ? title : "");
	if (v->ftl)
		wlr_foreign_toplevel_handle_v1_set_title(v->ftl, v->title);
	if (v->ext_ftl)
		wlr_ext_foreign_toplevel_handle_v1_update_state(
			v->ext_ftl, &(struct wlr_ext_foreign_toplevel_handle_v1_state){.title = v->title, .app_id = v->app_id});
	decor_update(v);
	if (v->mapped)
		windows_changed(v->server);
}

void view_set_app_id(struct chrono_view *v, const char *app_id)
{
	free(v->app_id);
	v->app_id = strdup(app_id ? app_id : "");
	if (v->ftl)
		wlr_foreign_toplevel_handle_v1_set_app_id(v->ftl, v->app_id);
	if (v->ext_ftl)
		wlr_ext_foreign_toplevel_handle_v1_update_state(
			v->ext_ftl, &(struct wlr_ext_foreign_toplevel_handle_v1_state){.title = v->title, .app_id = v->app_id});
	if (v->mapped)
		windows_changed(v->server);
}

/* ================================================================================ foreign toplevel */
static void ftl_activate(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, ftl_request_activate);
	(void)data;
	view_focus(v);
}
static void ftl_close(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, ftl_request_close);
	(void)data;
	view_close(v);
}
static void ftl_minimize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, ftl_request_minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *e = data;
	view_set_minimized(v, e->minimized);
}
static void ftl_maximize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, ftl_request_maximize);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *e = data;
	view_set_maximized(v, e->maximized);
}
static void ftl_fullscreen(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, ftl_request_fullscreen);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *e = data;
	view_set_fullscreen(v, e->fullscreen);
}

void view_ftl_create(struct chrono_view *v)
{
	struct chrono_server *s = v->server;
	if (!v->ftl && s->ftl_manager) {
		v->ftl = wlr_foreign_toplevel_handle_v1_create(s->ftl_manager);
		if (v->ftl) {
			wlr_foreign_toplevel_handle_v1_set_title(v->ftl, v->title ? v->title : "");
			wlr_foreign_toplevel_handle_v1_set_app_id(v->ftl, v->app_id ? v->app_id : "");
			struct chrono_output *o = output_of_view(v);
			if (o)
				wlr_foreign_toplevel_handle_v1_output_enter(v->ftl, o->wlr_output);
			v->ftl_request_activate.notify = ftl_activate;
			wl_signal_add(&v->ftl->events.request_activate, &v->ftl_request_activate);
			v->ftl_request_close.notify = ftl_close;
			wl_signal_add(&v->ftl->events.request_close, &v->ftl_request_close);
			v->ftl_request_minimize.notify = ftl_minimize;
			wl_signal_add(&v->ftl->events.request_minimize, &v->ftl_request_minimize);
			v->ftl_request_maximize.notify = ftl_maximize;
			wl_signal_add(&v->ftl->events.request_maximize, &v->ftl_request_maximize);
			v->ftl_request_fullscreen.notify = ftl_fullscreen;
			wl_signal_add(&v->ftl->events.request_fullscreen, &v->ftl_request_fullscreen);
			ftl_update_states(v);
		}
	}
	if (!v->ext_ftl && s->ext_ftl_list) {
		v->ext_ftl = wlr_ext_foreign_toplevel_handle_v1_create(
			s->ext_ftl_list,
			&(struct wlr_ext_foreign_toplevel_handle_v1_state){.title = v->title ? v->title : "",
									   .app_id = v->app_id ? v->app_id : ""});
	}
}

/* ================================================================================ xdg-shell */
static void xdg_map(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, map);
	(void)data;
	view_mapped(v);
}

static void xdg_unmap(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, unmap);
	(void)data;
	view_unmapped(v);
}

static void xdg_commit(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, commit);
	struct chrono_server *s = v->server;
	(void)data;
	struct wlr_xdg_surface *base = v->xdg->base;
	if (base->initial_commit) {
		wlr_xdg_toplevel_set_wm_capabilities(v->xdg, WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
							     WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
							     WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE);
		/* let the client pick its size; we place it when it maps */
		wlr_xdg_toplevel_set_size(v->xdg, 0, 0);
		if (base->client && v->xdg->requested.maximized)
			wlr_xdg_toplevel_set_maximized(v->xdg, false);
		wlr_xdg_surface_schedule_configure(base);
		return;
	}
	if (!v->mapped)
		return;
	struct wlr_box g = base->geometry;
	int w = g.width > 0 ? g.width : v->surface->current.width;
	int h = g.height > 0 ? g.height : v->surface->current.height;
	if (w != v->w || h != v->h) {
		if (s->cursor_mode == CURSOR_RESIZE && s->grab_view == v) {
			/* keep the opposite edge fixed while resizing from the left / top */
			if (s->resize_edges & WLR_EDGE_LEFT)
				v->x = s->grab_geo.x + s->grab_geo.width - w;
			if (s->resize_edges & WLR_EDGE_TOP)
				v->y = s->grab_geo.y + s->grab_geo.height - h;
		}
		v->w = w;
		v->h = h;
		decor_update(v);
	}
	view_update_position(v);
}

static void xdg_destroy(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, destroy);
	(void)data;
	wl_list_remove(&v->map.link);
	wl_list_remove(&v->unmap.link);
	wl_list_remove(&v->commit.link);
	wl_list_remove(&v->destroy.link);
	wl_list_remove(&v->request_move.link);
	wl_list_remove(&v->request_resize.link);
	wl_list_remove(&v->request_maximize.link);
	wl_list_remove(&v->request_minimize.link);
	wl_list_remove(&v->request_fullscreen.link);
	wl_list_remove(&v->set_title.link);
	wl_list_remove(&v->set_app_id.link);
	view_destroyed(v);
}

static void xdg_request_move(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_move);
	(void)data;
	begin_interactive(v, CURSOR_MOVE, 0);
}

static void xdg_request_resize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_resize);
	struct wlr_xdg_toplevel_resize_event *e = data;
	begin_interactive(v, CURSOR_RESIZE, e->edges);
}

static void xdg_request_maximize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_maximize);
	(void)data;
	if (!v->xdg->base->initialized)
		return;
	view_set_maximized(v, v->xdg->requested.maximized);
}

static void xdg_request_fullscreen(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_fullscreen);
	(void)data;
	if (!v->xdg->base->initialized)
		return;
	view_set_fullscreen(v, v->xdg->requested.fullscreen);
}

static void xdg_request_minimize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_minimize);
	(void)data;
	if (v->mapped)
		view_set_minimized(v, true);
}

static void xdg_set_title(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, set_title);
	(void)data;
	view_set_title(v, v->xdg->title);
}

static void xdg_set_app_id(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, set_app_id);
	(void)data;
	view_set_app_id(v, v->xdg->app_id);
}

static void new_xdg_toplevel(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_xdg_toplevel);
	struct wlr_xdg_toplevel *toplevel = data;
	struct chrono_view *v = calloc(1, sizeof *v);
	v->server = s;
	v->id = ++s->next_view_id;
	v->xdg = toplevel;
	v->surface = toplevel->base->surface;
	v->ssd = false;      /* a frame only for clients that negotiate server-side decorations (below) */
	v->workspace = s->workspace;
	v->tree = wlr_scene_tree_create(s->layers[L_VIEWS]);
	v->tree->node.data = v;
	wlr_scene_node_set_enabled(&v->tree->node, false);
	v->content = wlr_scene_xdg_surface_create(v->tree, toplevel->base);
	toplevel->base->data = v->content;
	decor_create(v);
	v->title = strdup(toplevel->title ? toplevel->title : "");
	v->app_id = strdup(toplevel->app_id ? toplevel->app_id : "");
	wl_list_insert(&s->views, &v->link);

	v->map.notify = xdg_map;
	wl_signal_add(&toplevel->base->surface->events.map, &v->map);
	v->unmap.notify = xdg_unmap;
	wl_signal_add(&toplevel->base->surface->events.unmap, &v->unmap);
	v->commit.notify = xdg_commit;
	wl_signal_add(&toplevel->base->surface->events.commit, &v->commit);
	v->destroy.notify = xdg_destroy;
	wl_signal_add(&toplevel->events.destroy, &v->destroy);
	v->request_move.notify = xdg_request_move;
	wl_signal_add(&toplevel->events.request_move, &v->request_move);
	v->request_resize.notify = xdg_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &v->request_resize);
	v->request_maximize.notify = xdg_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &v->request_maximize);
	v->request_minimize.notify = xdg_request_minimize;
	wl_signal_add(&toplevel->events.request_minimize, &v->request_minimize);
	v->request_fullscreen.notify = xdg_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &v->request_fullscreen);
	v->set_title.notify = xdg_set_title;
	wl_signal_add(&toplevel->events.set_title, &v->set_title);
	v->set_app_id.notify = xdg_set_app_id;
	wl_signal_add(&toplevel->events.set_app_id, &v->set_app_id);
}

/* ---------------------------------------------------------------------------- popups */
struct popup {
	struct wlr_xdg_popup *xdg;
	struct wl_listener commit, destroy;
};

static void popup_commit(struct wl_listener *l, void *data)
{
	struct popup *p = wl_container_of(l, p, commit);
	(void)data;
	if (p->xdg->base->initial_commit)
		wlr_xdg_surface_schedule_configure(p->xdg->base);
}

static void popup_destroy(struct wl_listener *l, void *data)
{
	struct popup *p = wl_container_of(l, p, destroy);
	(void)data;
	wl_list_remove(&p->commit.link);
	wl_list_remove(&p->destroy.link);
	free(p);
}

static void new_xdg_popup(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_xdg_popup);
	struct wlr_xdg_popup *xp = data;
	struct wlr_scene_tree *parent_tree = NULL;
	struct wlr_xdg_surface *parent = xp->parent ? wlr_xdg_surface_try_from_wlr_surface(xp->parent) : NULL;
	if (parent) {
		parent_tree = parent->data;
	} else if (xp->parent) {
		struct wlr_layer_surface_v1 *ls = wlr_layer_surface_v1_try_from_wlr_surface(xp->parent);
		if (ls)
			parent_tree = ls->data;
	}
	if (!parent_tree) {
		wlr_log(WLR_ERROR, "popup without a known parent");
		return;
	}
	struct wlr_scene_tree *tree = wlr_scene_xdg_surface_create(parent_tree, xp->base);
	xp->base->data = tree;

	/* keep it inside the output its parent is on */
	int lx = 0, ly = 0;
	wlr_scene_node_coords(&parent_tree->node, &lx, &ly);
	struct chrono_output *o = output_at(s, lx, ly);
	if (o) {
		struct wlr_box box = o->full;
		box.x -= lx;
		box.y -= ly;
		wlr_xdg_popup_unconstrain_from_box(xp, &box);
	}

	struct popup *p = calloc(1, sizeof *p);
	p->xdg = xp;
	p->commit.notify = popup_commit;
	wl_signal_add(&xp->base->surface->events.commit, &p->commit);
	p->destroy.notify = popup_destroy;
	wl_signal_add(&xp->events.destroy, &p->destroy);
}

/* ---------------------------------------------------------------------------- decorations */
struct deco {
	struct wlr_xdg_toplevel_decoration_v1 *wlr;
	struct chrono_server *server;
	struct wl_listener request_mode, destroy, commit;
};

static struct chrono_view *view_from_toplevel(struct chrono_server *s, struct wlr_xdg_toplevel *t)
{
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link)
		if (!v->is_x11 && v->xdg == t)
			return v;
	return NULL;
}

static void deco_apply(struct deco *d)
{
	if (!d->wlr->toplevel->base->initialized)
		return;
	/* We always want to draw the frame: that's where the theme lives. Clients may still insist on their own. */
	wlr_xdg_toplevel_decoration_v1_set_mode(d->wlr, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	struct chrono_view *v = view_from_toplevel(d->server, d->wlr->toplevel);
	if (v) {
		v->ssd = true;
		view_update_position(v);
		decor_update(v);
	}
}

static void deco_request_mode(struct wl_listener *l, void *data)
{
	struct deco *d = wl_container_of(l, d, request_mode);
	(void)data;
	deco_apply(d);
}

static void deco_commit(struct wl_listener *l, void *data)
{
	struct deco *d = wl_container_of(l, d, commit);
	(void)data;
	if (d->wlr->toplevel->base->initial_commit)
		deco_apply(d);
}

static void deco_destroy(struct wl_listener *l, void *data)
{
	struct deco *d = wl_container_of(l, d, destroy);
	(void)data;
	wl_list_remove(&d->request_mode.link);
	wl_list_remove(&d->destroy.link);
	wl_list_remove(&d->commit.link);
	free(d);
}

static void new_decoration(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_decoration);
	struct wlr_xdg_toplevel_decoration_v1 *wd = data;
	struct deco *d = calloc(1, sizeof *d);
	d->wlr = wd;
	d->server = s;
	d->request_mode.notify = deco_request_mode;
	wl_signal_add(&wd->events.request_mode, &d->request_mode);
	d->destroy.notify = deco_destroy;
	wl_signal_add(&wd->events.destroy, &d->destroy);
	d->commit.notify = deco_commit;
	wl_signal_add(&wd->toplevel->base->surface->events.commit, &d->commit);
	deco_apply(d);
}

/* org_kde_kwin_server_decoration: GTK3 and older Qt ask through this one */
struct kde_deco {
	struct wlr_server_decoration *wlr;
	struct chrono_server *server;
	struct wl_listener mode, destroy;
};

static void kde_apply(struct kde_deco *d)
{
	struct chrono_view *v;
	wl_list_for_each(v, &d->server->views, link) {
		if (v->surface == d->wlr->surface) {
			v->ssd = d->wlr->mode == WLR_SERVER_DECORATION_MANAGER_MODE_SERVER;
			view_update_position(v);
			decor_update(v);
		}
	}
}

static void kde_mode(struct wl_listener *l, void *data)
{
	struct kde_deco *d = wl_container_of(l, d, mode);
	(void)data;
	kde_apply(d);
}

static void kde_destroy(struct wl_listener *l, void *data)
{
	struct kde_deco *d = wl_container_of(l, d, destroy);
	(void)data;
	wl_list_remove(&d->mode.link);
	wl_list_remove(&d->destroy.link);
	free(d);
}

static void kde_new_decoration(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, kde_new_decoration);
	struct kde_deco *d = calloc(1, sizeof *d);
	d->wlr = data;
	d->server = s;
	d->mode.notify = kde_mode;
	wl_signal_add(&d->wlr->events.mode, &d->mode);
	d->destroy.notify = kde_destroy;
	wl_signal_add(&d->wlr->events.destroy, &d->destroy);
	kde_apply(d);
}

/* A client that never asks for decorations draws its own (GTK4 / libadwaita): we notice that from the
 * absence of the protocol object at map time — see xdg_map → view_mapped. Clients known to draw their
 * own title bar get no frame. */
static void maybe_csd(struct chrono_view *v)
{
	(void)v;
}

void views_redecorate(struct chrono_server *s)
{
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link) {
		v->decor.painted_theme_gen = 0;
		if (!v->mapped)
			continue;
		if (v->maximized) {
			struct chrono_output *o = target_output(v);
			if (o)
				place_in(v, o->usable);
		} else if (v->snap) {
			int e = v->snap;
			v->snap = 0;
			view_snap(v, (uint32_t)e);
		} else {
			view_update_position(v);
		}
		decor_update(v);
	}
}

/* ================================================================================ IPC */
void view_write_json(struct chrono_server *s, char **buf, size_t *len)
{
	/* creation order, so taskbars stay stable */
	struct chrono_view *list[512];
	int n = 0;
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link)
		if (v->mapped && n < 512)
			list[n++] = v;
	for (int i = 1; i < n; i++)
		for (int j = i; j > 0 && list[j - 1]->id > list[j]->id; j--) {
			struct chrono_view *t = list[j];
			list[j] = list[j - 1];
			list[j - 1] = t;
		}
	jbuf_append(buf, len, "[");
	for (int i = 0; i < n; i++) {
		v = list[i];
		jbuf_append(buf, len, "%s{\"id\":%u,\"title\":", i ? "," : "", v->id);
		jbuf_string(buf, len, v->title ? v->title : "");
		jbuf_append(buf, len, ",\"app_id\":");
		jbuf_string(buf, len, v->app_id ? v->app_id : "");
		jbuf_append(buf, len,
			    ",\"focused\":%s,\"minimized\":%s,\"maximized\":%s,\"fullscreen\":%s,\"workspace\":%d,"
			    "\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"x11\":%s}",
			    s->focused == v ? "true" : "false", v->minimized ? "true" : "false",
			    v->maximized ? "true" : "false", v->fullscreen ? "true" : "false", v->workspace + 1, v->x, v->y,
			    v->w, v->h, v->is_x11 ? "true" : "false");
	}
	jbuf_append(buf, len, "]");
}

/* ================================================================================ init */
void view_init(struct chrono_server *s)
{
	(void)maybe_csd;
	s->xdg_shell = wlr_xdg_shell_create(s->display, 6);
	s->new_xdg_toplevel.notify = new_xdg_toplevel;
	wl_signal_add(&s->xdg_shell->events.new_toplevel, &s->new_xdg_toplevel);
	s->new_xdg_popup.notify = new_xdg_popup;
	wl_signal_add(&s->xdg_shell->events.new_popup, &s->new_xdg_popup);

	struct wlr_xdg_decoration_manager_v1 *dm = wlr_xdg_decoration_manager_v1_create(s->display);
	s->new_decoration.notify = new_decoration;
	wl_signal_add(&dm->events.new_toplevel_decoration, &s->new_decoration);

	struct wlr_server_decoration_manager *kde = wlr_server_decoration_manager_create(s->display);
	wlr_server_decoration_manager_set_default_mode(kde, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	s->kde_new_decoration.notify = kde_new_decoration;
	wl_signal_add(&kde->events.new_decoration, &s->kde_new_decoration);

	s->ftl_manager = wlr_foreign_toplevel_manager_v1_create(s->display);
	s->ext_ftl_list = wlr_ext_foreign_toplevel_list_v1_create(s->display, 1);
}
