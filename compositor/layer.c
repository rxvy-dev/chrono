/* wlr-layer-shell: panels, docks, wallpapers, launchers, notifications (ours or anyone's). */
#define _GNU_SOURCE
#include <stdlib.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>

#include "server.h"

struct layer_surf {
	struct wl_list link;
	struct chrono_server *server;
	struct wlr_layer_surface_v1 *ls;
	struct wlr_scene_layer_surface_v1 *scene;
	struct wl_listener map, unmap, commit, destroy;
	bool mapped;
};

static struct wl_list layer_surfaces;   /* all layer_surf */
static struct chrono_server *g_srv;

static enum layer_index tree_for(enum zwlr_layer_shell_v1_layer layer)
{
	switch (layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND: return L_BACKGROUND;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM: return L_BOTTOM;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP: return L_TOP;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY: return L_OVERLAY;
	}
	return L_TOP;
}

void layers_arrange(struct chrono_output *o)
{
	struct chrono_server *s = o->server;
	struct wlr_box full = o->full, usable = o->full;
	if (full.width <= 0)
		return;
	/* exclusive zones first, top layers before lower ones, then everything else */
	for (int pass = 0; pass < 2; pass++) {
		for (int layer = 3; layer >= 0; layer--) {
			struct layer_surf *l;
			wl_list_for_each(l, &layer_surfaces, link) {
				struct wlr_layer_surface_v1 *ls = l->ls;
				if (ls->output != o->wlr_output || !ls->initialized || (int)ls->current.layer != layer)
					continue;
				bool excl = ls->current.exclusive_zone > 0;
				if ((pass == 0) != excl)
					continue;
				wlr_scene_layer_surface_v1_configure(l->scene, &full, &usable);
			}
		}
	}
	o->usable = usable;
	(void)s;
}

struct wlr_layer_surface_v1 *layer_exclusive_keyboard(struct chrono_server *s)
{
	(void)s;
	struct layer_surf *l;
	wl_list_for_each(l, &layer_surfaces, link) {
		if (l->mapped && l->ls->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE &&
		    l->ls->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP)
			return l->ls;
	}
	return NULL;
}

static struct chrono_output *output_for(struct chrono_server *s, struct wlr_output *wo)
{
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link)
		if (o->wlr_output == wo)
			return o;
	return NULL;
}

static void rearrange(struct layer_surf *l)
{
	struct chrono_output *o = output_for(l->server, l->ls->output);
	if (!o)
		return;
	struct wlr_box before = o->usable;
	layers_arrange(o);
	if (before.x != o->usable.x || before.y != o->usable.y || before.width != o->usable.width ||
	    before.height != o->usable.height)
		output_arrange_all(l->server);
}

static void layer_commit(struct wl_listener *li, void *data)
{
	struct layer_surf *l = wl_container_of(li, l, commit);
	(void)data;
	struct wlr_layer_surface_v1 *ls = l->ls;
	if (!ls->output)
		return;
	if (ls->initial_commit || ls->current.committed) {
		/* the layer may have changed: move the scene node to the right tree */
		struct wlr_scene_tree *want = l->server->layers[tree_for(ls->current.layer)];
		if (l->scene->tree->node.parent != want)
			wlr_scene_node_reparent(&l->scene->tree->node, want);
		rearrange(l);
	}
}

static void layer_map(struct wl_listener *li, void *data)
{
	struct layer_surf *l = wl_container_of(li, l, map);
	(void)data;
	l->mapped = true;
	rearrange(l);
	if (l->ls->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE &&
	    l->ls->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP && !lock_active(l->server))
		keyboard_focus_surface(l->server, l->ls->surface);
}

static void layer_unmap(struct wl_listener *li, void *data)
{
	struct layer_surf *l = wl_container_of(li, l, unmap);
	(void)data;
	l->mapped = false;
	rearrange(l);
	struct chrono_server *s = l->server;
	if (s->seat->keyboard_state.focused_surface == l->ls->surface && !lock_active(s)) {
		if (s->focused)
			keyboard_focus_surface(s, s->focused->surface);
		else
			view_focus_top(s);
	}
}

static void layer_destroy(struct wl_listener *li, void *data)
{
	struct layer_surf *l = wl_container_of(li, l, destroy);
	(void)data;
	wl_list_remove(&l->map.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->commit.link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->link);
	struct chrono_output *o = output_for(l->server, l->ls->output);
	free(l);
	if (o)
		output_arrange_all(o->server);
}

static void new_layer_surface(struct wl_listener *li, void *data)
{
	struct chrono_server *s = wl_container_of(li, s, new_layer_surface);
	struct wlr_layer_surface_v1 *ls = data;
	if (!ls->output) {
		struct chrono_output *o = output_at(s, s->cursor->x, s->cursor->y);
		if (!o) {
			wlr_layer_surface_v1_destroy(ls);
			return;
		}
		ls->output = o->wlr_output;
	}
	struct layer_surf *l = calloc(1, sizeof *l);
	l->server = s;
	l->ls = ls;
	l->scene = wlr_scene_layer_surface_v1_create(s->layers[tree_for(ls->pending.layer)], ls);
	ls->data = l->scene->tree;
	l->map.notify = layer_map;
	wl_signal_add(&ls->surface->events.map, &l->map);
	l->unmap.notify = layer_unmap;
	wl_signal_add(&ls->surface->events.unmap, &l->unmap);
	l->commit.notify = layer_commit;
	wl_signal_add(&ls->surface->events.commit, &l->commit);
	l->destroy.notify = layer_destroy;
	wl_signal_add(&ls->events.destroy, &l->destroy);
	wl_list_insert(&layer_surfaces, &l->link);
}

void layer_init(struct chrono_server *s)
{
	g_srv = s;
	wl_list_init(&layer_surfaces);
	s->layer_shell = wlr_layer_shell_v1_create(s->display, 4);
	s->new_layer_surface.notify = new_layer_surface;
	wl_signal_add(&s->layer_shell->events.new_surface, &s->new_layer_surface);
}
