/* XWayland: X11 apps and games get the same frames, focus and window management as Wayland ones.
 * Menus and tooltips (override-redirect) are placed where they ask, without a frame. */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#include "server.h"

#ifdef CHRONO_HAVE_XWAYLAND
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>

struct unmanaged {
	struct wlr_xwayland_surface *xs;
	struct chrono_server *server;
	struct wlr_scene_tree *tree;
	struct wl_listener associate, dissociate, map, unmap, destroy, request_configure, set_geometry;
};

/* ------------------------------------------------------------------------------- unmanaged */
static void um_map(struct wl_listener *l, void *data)
{
	struct unmanaged *u = wl_container_of(l, u, map);
	(void)data;
	u->tree = wlr_scene_subsurface_tree_create(u->server->unmanaged, u->xs->surface);
	wlr_scene_node_set_position(&u->tree->node, u->xs->x, u->xs->y);
	if (wlr_xwayland_surface_override_redirect_wants_focus(u->xs))
		keyboard_focus_surface(u->server, u->xs->surface);
}

static void um_unmap(struct wl_listener *l, void *data)
{
	struct unmanaged *u = wl_container_of(l, u, unmap);
	(void)data;
	if (u->tree) {
		wlr_scene_node_destroy(&u->tree->node);
		u->tree = NULL;
	}
	struct chrono_server *s = u->server;
	if (s->seat->keyboard_state.focused_surface == u->xs->surface && s->focused)
		keyboard_focus_surface(s, s->focused->surface);
}

static void um_set_geometry(struct wl_listener *l, void *data)
{
	struct unmanaged *u = wl_container_of(l, u, set_geometry);
	(void)data;
	if (u->tree)
		wlr_scene_node_set_position(&u->tree->node, u->xs->x, u->xs->y);
}

static void um_request_configure(struct wl_listener *l, void *data)
{
	struct unmanaged *u = wl_container_of(l, u, request_configure);
	struct wlr_xwayland_surface_configure_event *e = data;
	wlr_xwayland_surface_configure(u->xs, e->x, e->y, e->width, e->height);
}

static void um_associate(struct wl_listener *l, void *data)
{
	struct unmanaged *u = wl_container_of(l, u, associate);
	(void)data;
	u->map.notify = um_map;
	wl_signal_add(&u->xs->surface->events.map, &u->map);
	u->unmap.notify = um_unmap;
	wl_signal_add(&u->xs->surface->events.unmap, &u->unmap);
}

static void um_dissociate(struct wl_listener *l, void *data)
{
	struct unmanaged *u = wl_container_of(l, u, dissociate);
	(void)data;
	wl_list_remove(&u->map.link);
	wl_list_remove(&u->unmap.link);
}

static void um_destroy(struct wl_listener *l, void *data)
{
	struct unmanaged *u = wl_container_of(l, u, destroy);
	(void)data;
	wl_list_remove(&u->associate.link);
	wl_list_remove(&u->dissociate.link);
	wl_list_remove(&u->destroy.link);
	wl_list_remove(&u->request_configure.link);
	wl_list_remove(&u->set_geometry.link);
	free(u);
}

static void new_unmanaged(struct chrono_server *s, struct wlr_xwayland_surface *xs)
{
	struct unmanaged *u = calloc(1, sizeof *u);
	u->xs = xs;
	u->server = s;
	u->associate.notify = um_associate;
	wl_signal_add(&xs->events.associate, &u->associate);
	u->dissociate.notify = um_dissociate;
	wl_signal_add(&xs->events.dissociate, &u->dissociate);
	u->destroy.notify = um_destroy;
	wl_signal_add(&xs->events.destroy, &u->destroy);
	u->request_configure.notify = um_request_configure;
	wl_signal_add(&xs->events.request_configure, &u->request_configure);
	u->set_geometry.notify = um_set_geometry;
	wl_signal_add(&xs->events.set_geometry, &u->set_geometry);
	if (xs->surface)
		um_associate(&u->associate, NULL);
}

/* ------------------------------------------------------------------------------- managed windows */
static void x_map(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, map);
	(void)data;
	v->surface = v->xsurface->surface;
	if (!v->content) {
		v->content = wlr_scene_subsurface_tree_create(v->tree, v->surface);
	}
	v->w = v->xsurface->width;
	v->h = v->xsurface->height;
	v->x = v->xsurface->x;
	v->y = v->xsurface->y;
	/* windows that place themselves (splash screens, games) keep their spot */
	v->placed = v->xsurface->size_hints &&
		    (v->xsurface->size_hints->flags & (XCB_ICCCM_SIZE_HINT_US_POSITION | XCB_ICCCM_SIZE_HINT_P_POSITION));
	view_mapped(v);
}

static void x_unmap(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, unmap);
	(void)data;
	view_unmapped(v);
	if (v->content) {
		wlr_scene_node_destroy(&v->content->node);
		v->content = NULL;
	}
}

static void x_commit(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, commit);
	(void)data;
	if (!v->mapped)
		return;
	int w = v->surface->current.width, h = v->surface->current.height;
	if (w != v->w || h != v->h) {
		v->w = w;
		v->h = h;
		decor_update(v);
		view_update_position(v);
	}
}

static void x_associate(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, associate);
	(void)data;
	v->surface = v->xsurface->surface;
	v->map.notify = x_map;
	wl_signal_add(&v->surface->events.map, &v->map);
	v->unmap.notify = x_unmap;
	wl_signal_add(&v->surface->events.unmap, &v->unmap);
	v->commit.notify = x_commit;
	wl_signal_add(&v->surface->events.commit, &v->commit);
}

static void x_dissociate(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, dissociate);
	(void)data;
	wl_list_remove(&v->map.link);
	wl_list_remove(&v->unmap.link);
	wl_list_remove(&v->commit.link);
}

static void x_destroy(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, destroy);
	(void)data;
	wl_list_remove(&v->associate.link);
	wl_list_remove(&v->dissociate.link);
	wl_list_remove(&v->destroy.link);
	wl_list_remove(&v->request_configure.link);
	wl_list_remove(&v->request_activate.link);
	wl_list_remove(&v->request_move.link);
	wl_list_remove(&v->request_resize.link);
	wl_list_remove(&v->request_maximize.link);
	wl_list_remove(&v->request_minimize.link);
	wl_list_remove(&v->request_fullscreen.link);
	wl_list_remove(&v->set_title.link);
	wl_list_remove(&v->set_class.link);
	view_destroyed(v);
}

static void x_request_configure(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_configure);
	struct wlr_xwayland_surface_configure_event *e = data;
	if (!v->mapped) {
		wlr_xwayland_surface_configure(v->xsurface, e->x, e->y, e->width, e->height);
		return;
	}
	if (v->maximized || v->fullscreen || v->snap) {
		/* keep our tiling; just repeat it */
		wlr_xwayland_surface_configure(v->xsurface, (int16_t)v->x, (int16_t)v->y, (uint16_t)v->w, (uint16_t)v->h);
		return;
	}
	view_resize(v, e->x, e->y, e->width, e->height);
}

static void x_request_activate(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_activate);
	(void)data;
	if (v->mapped)
		view_focus(v);
}

static void x_request_move(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_move);
	(void)data;
	begin_interactive(v, CURSOR_MOVE, 0);
}

static void x_request_resize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_resize);
	struct wlr_xwayland_resize_event *e = data;
	begin_interactive(v, CURSOR_RESIZE, e->edges);
}

static void x_request_maximize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_maximize);
	(void)data;
	view_set_maximized(v, v->xsurface->maximized_horz || v->xsurface->maximized_vert);
}

static void x_request_minimize(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_minimize);
	struct wlr_xwayland_minimize_event *e = data;
	view_set_minimized(v, e->minimize);
}

static void x_request_fullscreen(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, request_fullscreen);
	(void)data;
	view_set_fullscreen(v, v->xsurface->fullscreen);
}

static void x_set_title(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, set_title);
	(void)data;
	view_set_title(v, v->xsurface->title);
}

static void x_set_class(struct wl_listener *l, void *data)
{
	struct chrono_view *v = wl_container_of(l, v, set_class);
	(void)data;
	view_set_app_id(v, v->xsurface->class);
}

static void new_xwayland_surface(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_xwayland_surface);
	struct wlr_xwayland_surface *xs = data;
	if (xs->override_redirect) {
		new_unmanaged(s, xs);
		return;
	}
	struct chrono_view *v = calloc(1, sizeof *v);
	v->server = s;
	v->id = ++s->next_view_id;
	v->is_x11 = true;
	v->xsurface = xs;
	v->ssd = !xs->decorations;     /* X11 apps that draw their own frame say so via _MOTIF_WM_HINTS */
	v->workspace = s->workspace;
	v->tree = wlr_scene_tree_create(s->layers[L_VIEWS]);
	v->tree->node.data = v;
	wlr_scene_node_set_enabled(&v->tree->node, false);
	decor_create(v);
	v->title = strdup(xs->title ? xs->title : "");
	v->app_id = strdup(xs->class ? xs->class : "");
	wl_list_insert(&s->views, &v->link);

	v->associate.notify = x_associate;
	wl_signal_add(&xs->events.associate, &v->associate);
	v->dissociate.notify = x_dissociate;
	wl_signal_add(&xs->events.dissociate, &v->dissociate);
	v->destroy.notify = x_destroy;
	wl_signal_add(&xs->events.destroy, &v->destroy);
	v->request_configure.notify = x_request_configure;
	wl_signal_add(&xs->events.request_configure, &v->request_configure);
	v->request_activate.notify = x_request_activate;
	wl_signal_add(&xs->events.request_activate, &v->request_activate);
	v->request_move.notify = x_request_move;
	wl_signal_add(&xs->events.request_move, &v->request_move);
	v->request_resize.notify = x_request_resize;
	wl_signal_add(&xs->events.request_resize, &v->request_resize);
	v->request_maximize.notify = x_request_maximize;
	wl_signal_add(&xs->events.request_maximize, &v->request_maximize);
	v->request_minimize.notify = x_request_minimize;
	wl_signal_add(&xs->events.request_minimize, &v->request_minimize);
	v->request_fullscreen.notify = x_request_fullscreen;
	wl_signal_add(&xs->events.request_fullscreen, &v->request_fullscreen);
	v->set_title.notify = x_set_title;
	wl_signal_add(&xs->events.set_title, &v->set_title);
	v->set_class.notify = x_set_class;
	wl_signal_add(&xs->events.set_class, &v->set_class);
	if (xs->surface)
		x_associate(&v->associate, NULL);
}

static void xwayland_ready(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, xwayland_ready);
	(void)data;
	wlr_xwayland_set_seat(s->xwayland, s->seat);
	struct wlr_xcursor *xc = wlr_xcursor_manager_get_xcursor(s->xcursor, "default", 1);
	if (xc) {
		struct wlr_xcursor_image *img = xc->images[0];
		struct wlr_buffer *buf = wlr_xcursor_image_get_buffer(img);
		if (buf)
			wlr_xwayland_set_cursor(s->xwayland, buf, (int32_t)img->hotspot_x, (int32_t)img->hotspot_y);
	}
}

void xwayland_init(struct chrono_server *s)
{
	s->unmanaged = wlr_scene_tree_create(s->layers[L_OVERLAY]);
	s->xwayland = wlr_xwayland_create(s->display, s->compositor, true);
	if (!s->xwayland) {
		wlr_log(WLR_ERROR, "XWayland isn't available; X11 apps won't start");
		return;
	}
	s->xwayland_ready.notify = xwayland_ready;
	wl_signal_add(&s->xwayland->events.ready, &s->xwayland_ready);
	s->new_xwayland_surface.notify = new_xwayland_surface;
	wl_signal_add(&s->xwayland->events.new_surface, &s->new_xwayland_surface);
	setenv("DISPLAY", s->xwayland->display_name, 1);
}

#else
void xwayland_init(struct chrono_server *s)
{
	(void)s;
	wlr_log(WLR_INFO, "built without XWayland");
}
#endif
