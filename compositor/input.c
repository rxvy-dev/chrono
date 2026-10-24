/* Keyboard, mouse and touchpad: keybinds and actions, moving / resizing / snapping windows, title bar
 * buttons, drag and drop, the clipboard, pointer constraints for games, and VT switching. */
#define _GNU_SOURCE
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wlr/backend/libinput.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "../common/paint.h"
#include "server.h"

#define BIND_MODS (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO | WLR_MODIFIER_MOD5)

struct chrono_pointer {
	struct wl_list link;
	struct wlr_input_device *dev;
	struct wl_listener destroy;
};

static struct wl_list pointers;
static uint32_t consumed_keys[32];
static int nconsumed;

/* ================================================================================ helpers */
void cursor_set_image(struct chrono_server *s, const char *name)
{
	if (s->cursor_image == name)
		return;
	s->cursor_image = name;
	wlr_cursor_set_xcursor(s->cursor, s->xcursor, name);
}

void input_activity(struct chrono_server *s)
{
	wlr_idle_notifier_v1_notify_activity(s->idle_notifier, s->seat);
	idle_reset(s);
}

void keyboard_focus_surface(struct chrono_server *s, struct wlr_surface *surface)
{
	if (!surface) {
		wlr_seat_keyboard_notify_clear_focus(s->seat);
		return;
	}
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
	if (kb)
		wlr_seat_keyboard_notify_enter(s->seat, surface, kb->keycodes, kb->num_keycodes, &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(s->seat, surface, NULL, 0, NULL);
}

static const char *edge_cursor(uint32_t e)
{
	switch (e) {
	case WLR_EDGE_TOP: return "n-resize";
	case WLR_EDGE_BOTTOM: return "s-resize";
	case WLR_EDGE_LEFT: return "w-resize";
	case WLR_EDGE_RIGHT: return "e-resize";
	case WLR_EDGE_TOP | WLR_EDGE_LEFT: return "nw-resize";
	case WLR_EDGE_TOP | WLR_EDGE_RIGHT: return "ne-resize";
	case WLR_EDGE_BOTTOM | WLR_EDGE_LEFT: return "sw-resize";
	case WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT: return "se-resize";
	}
	return "default";
}

/* ================================================================================ actions */
static void spawn_named(struct chrono_server *s, const char *what)
{
	if (!strcmp(what, "terminal"))
		server_spawn(s, s->config.terminal);
	else if (!strcmp(what, "files"))
		server_spawn(s, s->config.file_manager);
	else if (!strcmp(what, "browser"))
		server_spawn(s, s->config.browser);
	else
		server_spawn(s, what);
}

bool run_action(struct chrono_server *s, const char *action)
{
	char verb[64] = "", arg[256] = "";
	sscanf(action, "%63s %255[^\n]", verb, arg);
	struct chrono_view *v = s->focused;

	if (!strcmp(verb, "exec")) {
		spawn_named(s, arg);
	} else if (!strcmp(verb, "launcher") || !strcmp(verb, "power-menu") || !strcmp(verb, "notifications")) {
		ipc_event(s, verb, NULL);
	} else if (!strcmp(verb, "settings")) {
		server_spawn(s, "chrono-settings");
	} else if (!strcmp(verb, "close")) {
		if (v) view_close(v);
	} else if (!strcmp(verb, "maximize")) {
		if (v) view_set_maximized(v, !(v->maximized));
	} else if (!strcmp(verb, "minimize")) {
		if (v) view_set_minimized(v, true);
	} else if (!strcmp(verb, "fullscreen")) {
		if (v) view_set_fullscreen(v, !v->fullscreen);
	} else if (!strcmp(verb, "snap")) {
		if (!v)
			return true;
		uint32_t e = !strcmp(arg, "left") ? WLR_EDGE_LEFT : !strcmp(arg, "right") ? WLR_EDGE_RIGHT
			   : !strcmp(arg, "up") ? WLR_EDGE_TOP : WLR_EDGE_NONE;
		/* Super+Left on a right-snapped window un-snaps it, like other desktops */
		if ((e == WLR_EDGE_LEFT && v->snap == WLR_EDGE_RIGHT) || (e == WLR_EDGE_RIGHT && v->snap == WLR_EDGE_LEFT))
			e = WLR_EDGE_NONE;
		view_snap(v, e);
	} else if (!strcmp(verb, "switcher")) {
		switcher_next(s, !strcmp(arg, "prev") ? -1 : 1);
	} else if (!strcmp(verb, "workspace") || !strcmp(verb, "move-to-workspace")) {
		int ws;
		if (!strcmp(arg, "next"))
			ws = (s->workspace + 1) % s->config.workspaces;
		else if (!strcmp(arg, "prev"))
			ws = (s->workspace + s->config.workspaces - 1) % s->config.workspaces;
		else
			ws = atoi(arg) - 1;
		if (ws < 0 || ws >= s->config.workspaces)
			return true;
		if (verb[0] == 'm') {
			if (v) {
				view_to_workspace(v, ws);
				workspace_switch(s, ws);
				view_focus(v);
			}
		} else {
			workspace_switch(s, ws);
		}
	} else if (!strcmp(verb, "lock")) {
		lock_screen(s);
	} else if (!strcmp(verb, "reload")) {
		server_reload(s);
	} else if (!strcmp(verb, "quit") || !strcmp(verb, "exit")) {
		server_quit(s);
	} else if (!strcmp(verb, "theme")) {
		server_apply_theme(s, arg);
	} else if (!strcmp(verb, "screenshot")) {
		char cmd[300];
		if (arg[0]) {
			snprintf(cmd, sizeof cmd, "chrono-screenshot %s", arg);
		} else {
			/* the monitor under the pointer, saved and copied to the clipboard */
			struct chrono_output *o = output_at(s, s->cursor->x, s->cursor->y);
			snprintf(cmd, sizeof cmd, "chrono-screenshot output '%s'", o ? o->wlr_output->name : "");
		}
		server_spawn(s, cmd);
	} else if (!strcmp(verb, "volume")) {
		const char *c = !strcmp(arg, "up") ? "wpctl set-volume -l 1.5 @DEFAULT_AUDIO_SINK@ 5%+"
			      : !strcmp(arg, "down") ? "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"
			      : "wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle";
		server_spawn(s, c);
		ipc_event(s, "osd", "{\"kind\":\"volume\"}");
	} else if (!strcmp(verb, "mic")) {
		server_spawn(s, "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle");
		ipc_event(s, "osd", "{\"kind\":\"mic\"}");
	} else if (!strcmp(verb, "brightness")) {
		server_spawn(s, !strcmp(arg, "up") ? "brightnessctl set 5%+" : "brightnessctl set 5%-");
		ipc_event(s, "osd", "{\"kind\":\"brightness\"}");
	} else if (!strcmp(verb, "media")) {
		char cmd[128];
		snprintf(cmd, sizeof cmd, "playerctl %s", arg[0] ? arg : "play-pause");
		server_spawn(s, cmd);
	} else {
		wlr_log(WLR_ERROR, "unknown action: %s", action);
		return false;
	}
	return true;
}

/* ================================================================================ keyboard */
static bool handle_bind(struct chrono_server *s, uint32_t mods, const xkb_keysym_t *syms, int n)
{
	mods &= BIND_MODS;
	if (s->binds_inhibited)
		return false;             /* the settings app is recording a shortcut */
	for (int i = 0; i < n; i++) {
		xkb_keysym_t sym = xkb_keysym_to_lower(syms[i]);
		for (int b = 0; b < s->config.nbinds; b++) {
			struct chrono_bind *bd = &s->config.binds[b];
			if (bd->sym == sym && bd->mods == mods) {
				run_action(s, bd->action);
				return true;
			}
		}
	}
	return false;
}

static void kb_modifiers(struct wl_listener *l, void *data)
{
	struct chrono_keyboard *k = wl_container_of(l, k, modifiers);
	(void)data;
	struct chrono_server *s = k->server;
	wlr_seat_set_keyboard(s->seat, k->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(s->seat, &k->wlr_keyboard->modifiers);
	/* the window switcher commits when Alt / Super is let go */
	uint32_t m = wlr_keyboard_get_modifiers(k->wlr_keyboard);
	if (s->switcher.active && !(m & (WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO)))
		switcher_commit(s);
}

static void kb_key(struct wl_listener *l, void *data)
{
	struct chrono_keyboard *k = wl_container_of(l, k, key);
	struct chrono_server *s = k->server;
	struct wlr_keyboard_key_event *e = data;
	uint32_t keycode = e->keycode + 8;
	struct xkb_state *state = k->wlr_keyboard->xkb_state;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(state, keycode, &syms);
	const xkb_keysym_t *raw;
	xkb_layout_index_t layout = xkb_state_key_get_layout(state, keycode);
	int nraw = xkb_keymap_key_get_syms_by_level(k->wlr_keyboard->keymap, keycode, layout, 0, &raw);
	uint32_t mods = wlr_keyboard_get_modifiers(k->wlr_keyboard);
	bool pressed = e->state == WL_KEYBOARD_KEY_STATE_PRESSED;

	input_activity(s);
	if (s->screens_off)
		outputs_set_power(s, true);

	/* releases of keys we swallowed stay swallowed */
	if (!pressed) {
		for (int i = 0; i < nconsumed; i++) {
			if (consumed_keys[i] == e->keycode) {
				consumed_keys[i] = consumed_keys[--nconsumed];
				return;
			}
		}
	}

	if (pressed) {
		/* VT switching works everywhere, even on the lock screen */
		for (int i = 0; i < nsyms; i++) {
			if (syms[i] >= XKB_KEY_XF86Switch_VT_1 && syms[i] <= XKB_KEY_XF86Switch_VT_12) {
				if (s->session)
					wlr_session_change_vt(s->session, syms[i] - XKB_KEY_XF86Switch_VT_1 + 1);
				return;
			}
		}
	}

	if (lock_active(s)) {
		if (pressed && !s->lock.ext_locked) {
			uint32_t cp = xkb_state_key_get_utf32(state, keycode);
			for (int i = 0; i < nsyms; i++)
				lock_handle_key(s, syms[i], i == 0 ? cp : 0);
			return;
		}
		if (!s->lock.ext_locked)
			return;
		/* an external locker gets its keys */
		wlr_seat_set_keyboard(s->seat, k->wlr_keyboard);
		wlr_seat_keyboard_notify_key(s->seat, e->time_msec, e->keycode, e->state);
		return;
	}

	if (pressed && s->switcher.active) {
		for (int i = 0; i < nsyms; i++) {
			if (syms[i] == XKB_KEY_Escape) {
				switcher_cancel(s);
				goto consumed;
			}
			if (syms[i] == XKB_KEY_Return || syms[i] == XKB_KEY_KP_Enter) {
				switcher_commit(s);
				goto consumed;
			}
		}
	}

	/* tapping Super on its own (no other key or click while it's held) opens the app menu */
	bool is_super = false;
	for (int i = 0; i < nraw; i++)
		if (raw[i] == XKB_KEY_Super_L || raw[i] == XKB_KEY_Super_R)
			is_super = true;
	if (pressed) {
		s->super_tap = is_super && !(mods & BIND_MODS & ~WLR_MODIFIER_LOGO);
	} else if (is_super) {
		bool tap = s->super_tap;
		s->super_tap = false;
		if (tap && s->config.super_opens_launcher && !s->binds_inhibited && !s->switcher.active)
			run_action(s, "launcher");
	}

	if (pressed) {
		/* match the unshifted key first (Super+Shift+1), then the translated one */
		if (handle_bind(s, mods, raw, nraw) || handle_bind(s, mods, syms, nsyms))
			goto consumed;
	}

	wlr_seat_set_keyboard(s->seat, k->wlr_keyboard);
	wlr_seat_keyboard_notify_key(s->seat, e->time_msec, e->keycode, e->state);
	return;

consumed:
	if (nconsumed < 32)
		consumed_keys[nconsumed++] = e->keycode;
}

static void kb_destroy(struct wl_listener *l, void *data)
{
	struct chrono_keyboard *k = wl_container_of(l, k, destroy);
	(void)data;
	wl_list_remove(&k->modifiers.link);
	wl_list_remove(&k->key.link);
	wl_list_remove(&k->destroy.link);
	wl_list_remove(&k->link);
	free(k);
}

static void keyboard_configure(struct chrono_server *s, struct wlr_keyboard *kb)
{
	struct xkb_rule_names rules = {0};
	struct chrono_config *c = &s->config;
	rules.rules = "evdev";
	rules.model = c->kb_model[0] ? c->kb_model : NULL;
	rules.layout = c->kb_layout[0] ? c->kb_layout : NULL;
	rules.variant = c->kb_variant[0] ? c->kb_variant : NULL;
	rules.options = c->kb_options[0] ? c->kb_options : NULL;
	struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *km = xkb_keymap_new_from_names(ctx, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!km) {
		wlr_log(WLR_ERROR, "bad keyboard layout '%s', using the default", c->kb_layout);
		km = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}
	if (km) {
		wlr_keyboard_set_keymap(kb, km);
		if (c->numlock) {
			xkb_mod_index_t num = xkb_keymap_mod_get_index(km, XKB_MOD_NAME_NUM);
			if (num != XKB_MOD_INVALID)
				wlr_keyboard_notify_modifiers(kb, kb->modifiers.depressed, kb->modifiers.latched,
							      kb->modifiers.locked | (1u << num), kb->modifiers.group);
		}
		xkb_keymap_unref(km);
	}
	xkb_context_unref(ctx);
	wlr_keyboard_set_repeat_info(kb, c->repeat_rate, c->repeat_delay);
}

static void new_keyboard(struct chrono_server *s, struct wlr_input_device *dev)
{
	struct wlr_keyboard *kb = wlr_keyboard_from_input_device(dev);
	struct chrono_keyboard *k = calloc(1, sizeof *k);
	k->server = s;
	k->wlr_keyboard = kb;
	keyboard_configure(s, kb);
	k->modifiers.notify = kb_modifiers;
	wl_signal_add(&kb->events.modifiers, &k->modifiers);
	k->key.notify = kb_key;
	wl_signal_add(&kb->events.key, &k->key);
	k->destroy.notify = kb_destroy;
	wl_signal_add(&dev->events.destroy, &k->destroy);
	wlr_seat_set_keyboard(s->seat, kb);
	wl_list_insert(&s->keyboards, &k->link);
}

/* ================================================================================ pointer devices */
static void pointer_configure(struct chrono_server *s, struct wlr_input_device *dev)
{
	if (!wlr_input_device_is_libinput(dev))
		return;
	struct libinput_device *li = wlr_libinput_get_device_handle(dev);
	struct chrono_config *c = &s->config;
	bool touchpad = libinput_device_config_tap_get_finger_count(li) > 0;
	if (libinput_device_config_accel_is_available(li)) {
		libinput_device_config_accel_set_speed(li, touchpad ? c->tp_accel : c->accel);
		libinput_device_config_accel_set_profile(li, c->flat_accel ? LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
									   : LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
	}
	if (libinput_device_config_scroll_has_natural_scroll(li))
		libinput_device_config_scroll_set_natural_scroll_enabled(li, touchpad ? c->tp_natural_scroll
										     : c->natural_scroll);
	if (libinput_device_config_left_handed_is_available(li))
		libinput_device_config_left_handed_set(li, c->left_handed);
	if (touchpad) {
		libinput_device_config_tap_set_enabled(li, c->tap ? LIBINPUT_CONFIG_TAP_ENABLED : LIBINPUT_CONFIG_TAP_DISABLED);
		if (libinput_device_config_dwt_is_available(li))
			libinput_device_config_dwt_set_enabled(li, c->dwt ? LIBINPUT_CONFIG_DWT_ENABLED
									   : LIBINPUT_CONFIG_DWT_DISABLED);
	}
}

static void pointer_destroy(struct wl_listener *l, void *data)
{
	struct chrono_pointer *p = wl_container_of(l, p, destroy);
	(void)data;
	wl_list_remove(&p->destroy.link);
	wl_list_remove(&p->link);
	free(p);
}

static void new_input(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_input);
	struct wlr_input_device *dev = data;
	switch (dev->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		new_keyboard(s, dev);
		break;
	case WLR_INPUT_DEVICE_POINTER: {
		pointer_configure(s, dev);
		wlr_cursor_attach_input_device(s->cursor, dev);
		struct chrono_pointer *p = calloc(1, sizeof *p);
		p->dev = dev;
		p->destroy.notify = pointer_destroy;
		wl_signal_add(&dev->events.destroy, &p->destroy);
		wl_list_insert(&pointers, &p->link);
		break;
	}
	default:
		break;
	}
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&s->keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(s->seat, caps);
}

void input_apply_config(struct chrono_server *s)
{
	struct chrono_keyboard *k;
	wl_list_for_each(k, &s->keyboards, link)
		keyboard_configure(s, k->wlr_keyboard);
	struct chrono_pointer *p;
	wl_list_for_each(p, &pointers, link)
		pointer_configure(s, p->dev);
}

/* ================================================================================ interactive grabs */
void begin_interactive(struct chrono_view *v, enum cursor_mode mode, uint32_t edges)
{
	struct chrono_server *s = v->server;
	if (lock_active(s) || v->fullscreen)
		return;
	s->grab_view = v;
	s->cursor_mode = mode;
	s->grab_x = s->cursor->x;
	s->grab_y = s->cursor->y;
	s->grab_geo = (struct wlr_box){v->x, v->y, v->w, v->h};
	s->resize_edges = edges;
	s->pending_snap = 0;
	if (mode == CURSOR_RESIZE) {
		if (v->maximized || v->snap) {
			/* resizing a tiled window detaches it from the tiling */
			v->maximized = false;
			v->snap = 0;
		}
		cursor_set_image(s, edge_cursor(edges));
	} else if (mode == CURSOR_MOVE) {
		cursor_set_image(s, "grabbing");
	}
	wlr_seat_pointer_notify_clear_focus(s->seat);
}

static uint32_t snap_edges_at(struct chrono_server *s, double x, double y, struct chrono_output **out)
{
	struct chrono_output *o = output_at(s, x, y);
	*out = o;
	if (!o || !s->config.snap)
		return 0;
	int th = s->config.snap_threshold;
	struct wlr_box f = o->full;
	uint32_t e = 0;
	if (x <= f.x + th) e |= WLR_EDGE_LEFT;
	if (x >= f.x + f.width - 1 - th) e |= WLR_EDGE_RIGHT;
	if (y <= f.y + th) e |= WLR_EDGE_TOP;
	if (y >= f.y + f.height - 1 - th) e |= WLR_EDGE_BOTTOM;
	/* corners give quarters; plain top maximizes; plain bottom does nothing */
	if (e == WLR_EDGE_BOTTOM)
		e = 0;
	return e;
}

static void show_snap_preview(struct chrono_server *s, uint32_t e, struct chrono_output *o)
{
	if (!e || !o) {
		wlr_scene_node_set_enabled(&s->snap_preview->node, false);
		return;
	}
	struct wlr_box a = o->usable;
	if (e != WLR_EDGE_TOP) {
		if (e & WLR_EDGE_LEFT) a.width /= 2;
		if (e & WLR_EDGE_RIGHT) { a.x += a.width / 2; a.width -= a.width / 2; }
		if ((e & WLR_EDGE_TOP) && (e & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT))) a.height /= 2;
		if (e & WLR_EDGE_BOTTOM) { a.y += a.height / 2; a.height -= a.height / 2; }
	}
	struct chrono_rgba c = s->theme.accent;
	float col[4] = {(float)(c.r * 0.3), (float)(c.g * 0.3), (float)(c.b * 0.3), 0.3f};
	wlr_scene_rect_set_color(s->snap_preview, col);
	wlr_scene_rect_set_size(s->snap_preview, a.width - 16, a.height - 16);
	wlr_scene_node_set_position(&s->snap_preview->node, a.x + 8, a.y + 8);
	wlr_scene_node_set_enabled(&s->snap_preview->node, true);
}

static void process_move(struct chrono_server *s)
{
	struct chrono_view *v = s->grab_view;
	double cx = s->cursor->x, cy = s->cursor->y;
	if ((v->maximized || v->snap) && (fabs(cx - s->grab_x) > 8 || fabs(cy - s->grab_y) > 8)) {
		/* drag a tiled window off its tile: restore its size under the cursor */
		double fx = (s->grab_x - v->x) / (v->w > 0 ? v->w : 1);
		v->maximized = false;
		v->snap = 0;
		if (!v->is_x11 && v->xdg && v->xdg->base->initialized) {
			wlr_xdg_toplevel_set_maximized(v->xdg, false);
			wlr_xdg_toplevel_set_tiled(v->xdg, WLR_EDGE_NONE);
		}
		int w = v->restore.width > 0 ? v->restore.width : v->w * 2 / 3;
		int h = v->restore.height > 0 ? v->restore.height : v->h * 2 / 3;
		int nx = (int)(cx - fx * w);
		view_resize(v, nx, v->y, w, h);
		s->grab_geo = (struct wlr_box){nx, v->y, w, h};
		s->grab_x = cx;
		s->grab_y = cy;
		ipc_event(s, "windows", NULL);
	}
	view_move_to(v, s->grab_geo.x + (int)(cx - s->grab_x), s->grab_geo.y + (int)(cy - s->grab_y));
	struct chrono_output *o;
	uint32_t e = snap_edges_at(s, cx, cy, &o);
	s->pending_snap = (int)e;
	show_snap_preview(s, e, o);
}

static void process_resize(struct chrono_server *s)
{
	struct chrono_view *v = s->grab_view;
	double dx = s->cursor->x - s->grab_x, dy = s->cursor->y - s->grab_y;
	struct wlr_box g = s->grab_geo;
	int x = g.x, y = g.y, w = g.width, h = g.height;
	int minw = 120, minh = 60;
	if (!v->is_x11 && v->xdg) {
		if (v->xdg->current.min_width > minw) minw = v->xdg->current.min_width;
		if (v->xdg->current.min_height > minh) minh = v->xdg->current.min_height;
	}
	if (s->resize_edges & WLR_EDGE_LEFT) {
		w = g.width - (int)dx;
		if (w < minw) w = minw;
		x = g.x + g.width - w;
	} else if (s->resize_edges & WLR_EDGE_RIGHT) {
		w = g.width + (int)dx;
		if (w < minw) w = minw;
	}
	if (s->resize_edges & WLR_EDGE_TOP) {
		h = g.height - (int)dy;
		if (h < minh) h = minh;
		y = g.y + g.height - h;
	} else if (s->resize_edges & WLR_EDGE_BOTTOM) {
		h = g.height + (int)dy;
		if (h < minh) h = minh;
	}
	if (v->is_x11) {
		view_resize(v, x, y, w, h);
	} else if (v->xdg && v->xdg->base->initialized) {
		/* xdg clients resize on their own schedule; the commit handler keeps the far edge fixed */
		wlr_xdg_toplevel_set_size(v->xdg, w, h);
	}
}

static void update_decor_hover(struct chrono_server *s, struct chrono_view *v, struct decor_hit *hit)
{
	struct chrono_view *it;
	wl_list_for_each(it, &s->views, link) {
		int want = (it == v && hit->part == PART_BUTTON) ? hit->button : -1;
		if (it->decor.hover != want) {
			it->decor.hover = want;
			decor_update(it);
		}
	}
}

void cursor_update_focus(struct chrono_server *s, uint32_t time)
{
	if (s->cursor_mode == CURSOR_MOVE) {
		process_move(s);
		return;
	}
	if (s->cursor_mode == CURSOR_RESIZE) {
		process_resize(s);
		return;
	}
	double sx, sy;
	struct wlr_surface *surface;
	struct decor_hit hit;
	struct chrono_view *v = view_at(s, s->cursor->x, s->cursor->y, &surface, &sx, &sy, &hit);
	if (s->cursor_mode == CURSOR_PRESS_BUTTON)
		return;
	update_decor_hover(s, v, &hit);
	if (lock_active(s) && !s->lock.ext_locked) {
		cursor_set_image(s, "default");
		wlr_seat_pointer_notify_clear_focus(s->seat);
		return;
	}
	if (!surface) {
		if (hit.part == PART_EDGE)
			cursor_set_image(s, edge_cursor(hit.edges));
		else
			cursor_set_image(s, "default");
		wlr_seat_pointer_notify_clear_focus(s->seat);
		return;
	}
	s->cursor_image = NULL;   /* the client sets its own */
	wlr_seat_pointer_notify_enter(s->seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(s->seat, time, sx, sy);
	if (v && s->config.focus_follows_mouse && v != s->focused && !s->switcher.active)
		view_focus(v);
}

/* ================================================================================ cursor events */
static bool constrained_motion(struct chrono_server *s, double dx, double dy)
{
	struct wlr_pointer_constraint_v1 *c = s->active_constraint;
	if (!c || !s->seat->pointer_state.focused_surface || c->surface != s->seat->pointer_state.focused_surface)
		return false;
	if (c->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
		return true;
	/* confined: stay inside the surface's region */
	double sx = s->seat->pointer_state.sx + dx, sy = s->seat->pointer_state.sy + dy;
	if (!pixman_region32_contains_point(&c->region, (int)sx, (int)sy, NULL))
		return true;
	return false;
}

static void cursor_motion(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, cursor_motion);
	struct wlr_pointer_motion_event *e = data;
	input_activity(s);
	if (s->screens_off)
		outputs_set_power(s, true);
	wlr_relative_pointer_manager_v1_send_relative_motion(s->relative_pointer, s->seat, (uint64_t)e->time_msec * 1000,
							     e->delta_x, e->delta_y, e->unaccel_dx, e->unaccel_dy);
	if (!constrained_motion(s, e->delta_x, e->delta_y))
		wlr_cursor_move(s->cursor, &e->pointer->base, e->delta_x, e->delta_y);
	cursor_update_focus(s, e->time_msec);
}

static void cursor_motion_absolute(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *e = data;
	input_activity(s);
	wlr_cursor_warp_absolute(s->cursor, &e->pointer->base, e->x, e->y);
	cursor_update_focus(s, e->time_msec);
}

static void press_decor_button(struct chrono_server *s, struct chrono_view *v, int idx)
{
	struct chrono_decor_geom g;
	chrono_decor_geometry(&s->theme, v->w, v->title, &g);
	if (idx < 0 || idx >= g.nbuttons)
		return;
	switch (g.buttons[idx].kind) {
	case BTN_CLOSE: view_close(v); break;
	case BTN_MAXIMIZE: view_set_maximized(v, !v->maximized); break;
	case BTN_MINIMIZE: view_set_minimized(v, true); break;
	case BTN_MENU: {
		char payload[96];
		snprintf(payload, sizeof payload, "{\"id\":%u,\"x\":%d,\"y\":%d}", v->id, (int)s->cursor->x, (int)s->cursor->y);
		ipc_event(s, "window-menu", payload);
		break;
	}
	default: break;
	}
}

static void cursor_button(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, cursor_button);
	struct wlr_pointer_button_event *e = data;
	input_activity(s);
	bool pressed = e->state == WL_POINTER_BUTTON_STATE_PRESSED;
	s->super_tap = false;   /* Super+click / Super+drag is not a tap */

	if (lock_active(s) && !s->lock.ext_locked)
		return;

	if (!pressed) {
		if (s->cursor_mode == CURSOR_MOVE) {
			struct chrono_view *v = s->grab_view;
			wlr_scene_node_set_enabled(&s->snap_preview->node, false);
			if (v && s->pending_snap)
				view_snap(v, (uint32_t)s->pending_snap);
			s->cursor_mode = CURSOR_PASSTHROUGH;
			s->grab_view = NULL;
			s->cursor_image = NULL;
			cursor_update_focus(s, e->time_msec);
			ipc_event(s, "windows", NULL);
			return;
		}
		if (s->cursor_mode == CURSOR_RESIZE) {
			s->cursor_mode = CURSOR_PASSTHROUGH;
			s->grab_view = NULL;
			s->cursor_image = NULL;
			cursor_update_focus(s, e->time_msec);
			ipc_event(s, "windows", NULL);
			return;
		}
		if (s->cursor_mode == CURSOR_PRESS_BUTTON) {
			s->cursor_mode = CURSOR_PASSTHROUGH;
			struct chrono_view *v = view_by_id(s, (uint32_t)s->pressed_button_view_id);
			if (v) {
				int idx = v->decor.pressed;
				v->decor.pressed = -1;
				decor_update(v);
				double sx, sy;
				struct wlr_surface *surf;
				struct decor_hit hit;
				struct chrono_view *under = view_at(s, s->cursor->x, s->cursor->y, &surf, &sx, &sy, &hit);
				if (under == v && hit.part == PART_BUTTON && hit.button == idx)
					press_decor_button(s, v, idx);
			}
			cursor_update_focus(s, e->time_msec);
			return;
		}
		wlr_seat_pointer_notify_button(s->seat, e->time_msec, e->button, e->state);
		return;
	}

	double sx, sy;
	struct wlr_surface *surface;
	struct decor_hit hit;
	struct chrono_view *v = view_at(s, s->cursor->x, s->cursor->y, &surface, &sx, &sy, &hit);

	/* clicking a panel / launcher that wants the keyboard gives it the keyboard */
	if (surface && !v) {
		struct wlr_layer_surface_v1 *ls = wlr_layer_surface_v1_try_from_wlr_surface(surface);
		if (ls && ls->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
			keyboard_focus_surface(s, surface);
	}
	if (v) {
		if (v != s->focused || s->seat->keyboard_state.focused_surface != v->surface)
			view_focus(v);
		struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
		uint32_t mods = kb ? wlr_keyboard_get_modifiers(kb) : 0;
		/* Super + drag moves, Super + right-drag resizes, from anywhere in the window */
		if (mods & WLR_MODIFIER_LOGO) {
			if (e->button == BTN_LEFT) {
				begin_interactive(v, CURSOR_MOVE, 0);
				return;
			}
			if (e->button == BTN_RIGHT) {
				struct wlr_box fb;
				view_frame_box(v, &fb);
				uint32_t edges = (s->cursor->x < fb.x + fb.width / 2.0 ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT) |
						 (s->cursor->y < fb.y + fb.height / 2.0 ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM);
				begin_interactive(v, CURSOR_RESIZE, edges);
				return;
			}
		}
		if (hit.part == PART_BUTTON && e->button == BTN_LEFT) {
			s->cursor_mode = CURSOR_PRESS_BUTTON;
			s->pressed_button_view_id = (int)v->id;
			v->decor.pressed = hit.button;
			decor_update(v);
			return;
		}
		if (hit.part == PART_EDGE && e->button == BTN_LEFT) {
			begin_interactive(v, CURSOR_RESIZE, hit.edges);
			return;
		}
		if (hit.part == PART_TITLE) {
			if (e->button == BTN_LEFT) {
				if (s->last_title_click_view == v->id && e->time_msec - s->last_title_click < 400) {
					s->last_title_click_view = 0;
					const char *dc = s->config.double_click_title;
					if (!strcmp(dc, "maximize"))
						view_set_maximized(v, !v->maximized);
					else if (!strcmp(dc, "minimize"))
						view_set_minimized(v, true);
					return;
				}
				s->last_title_click = e->time_msec;
				s->last_title_click_view = v->id;
				begin_interactive(v, CURSOR_MOVE, 0);
			} else if (e->button == BTN_MIDDLE) {
				/* middle-click a title bar to send the window behind the others */
				wlr_scene_node_lower_to_bottom(&v->tree->node);
				wl_list_remove(&v->link);
				wl_list_insert(v->server->views.prev, &v->link);
				view_focus_top(s);
			} else if (e->button == BTN_RIGHT) {
				char payload[96];
				snprintf(payload, sizeof payload, "{\"id\":%u,\"x\":%d,\"y\":%d}", v->id, (int)s->cursor->x,
					 (int)s->cursor->y);
				ipc_event(s, "window-menu", payload);
			}
			return;
		}
	}
	wlr_seat_pointer_notify_button(s->seat, e->time_msec, e->button, e->state);
}

static void cursor_axis(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, cursor_axis);
	struct wlr_pointer_axis_event *e = data;
	input_activity(s);
	double f = s->config.scroll_factor > 0 ? s->config.scroll_factor : 1.0;
	wlr_seat_pointer_notify_axis(s->seat, e->time_msec, e->orientation, f * e->delta,
				     (int32_t)(f * e->delta_discrete), e->source, e->relative_direction);
}

static void cursor_frame(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, cursor_frame);
	(void)data;
	wlr_seat_pointer_notify_frame(s->seat);
}

static void request_cursor(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *e = data;
	if (s->cursor_mode != CURSOR_PASSTHROUGH)
		return;
	if (s->seat->pointer_state.focused_client == e->seat_client) {
		s->cursor_image = NULL;
		wlr_cursor_set_surface(s->cursor, e->surface, e->hotspot_x, e->hotspot_y);
	}
}

static void cursor_shape_request(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, cursor_shape_request);
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *e = data;
	if (s->cursor_mode != CURSOR_PASSTHROUGH)
		return;
	if (s->seat->pointer_state.focused_client == e->seat_client) {
		s->cursor_image = NULL;
		wlr_cursor_set_xcursor(s->cursor, s->xcursor, wlr_cursor_shape_v1_name(e->shape));
	}
}

static void request_set_selection(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, request_set_selection);
	struct wlr_seat_request_set_selection_event *e = data;
	wlr_seat_set_selection(s->seat, e->source, e->serial);
}

static void request_set_primary_selection(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *e = data;
	wlr_seat_set_primary_selection(s->seat, e->source, e->serial);
}

static void request_start_drag(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, request_start_drag);
	struct wlr_seat_request_start_drag_event *e = data;
	if (wlr_seat_validate_pointer_grab_serial(s->seat, e->origin, e->serial))
		wlr_seat_start_pointer_drag(s->seat, e->drag, e->serial);
	else
		wlr_data_source_destroy(e->drag->source);
}

static void start_drag(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, start_drag);
	struct wlr_drag *drag = data;
	if (drag->icon)
		wlr_scene_drag_icon_create(s->layers[L_DRAG], drag->icon);
}

/* ------------------------------------------------------------------------------- pointer constraints */
struct constraint {
	struct wlr_pointer_constraint_v1 *wlr;
	struct chrono_server *server;
	struct wl_listener destroy;
};

static void constraint_destroy(struct wl_listener *l, void *data)
{
	struct constraint *c = wl_container_of(l, c, destroy);
	(void)data;
	if (c->server->active_constraint == c->wlr)
		c->server->active_constraint = NULL;
	wl_list_remove(&c->destroy.link);
	free(c);
}

static void new_constraint(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_constraint);
	struct wlr_pointer_constraint_v1 *wc = data;
	struct constraint *c = calloc(1, sizeof *c);
	c->wlr = wc;
	c->server = s;
	c->destroy.notify = constraint_destroy;
	wl_signal_add(&wc->events.destroy, &c->destroy);
	/* games lock the pointer on the focused window */
	if (s->seat->keyboard_state.focused_surface == wc->surface) {
		if (s->active_constraint && s->active_constraint != wc)
			wlr_pointer_constraint_v1_send_deactivated(s->active_constraint);
		s->active_constraint = wc;
		wlr_pointer_constraint_v1_send_activated(wc);
	}
}

/* ================================================================================ init */
void input_init(struct chrono_server *s)
{
	wl_list_init(&pointers);
	wl_list_init(&s->keyboards);
	s->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(s->cursor, s->output_layout);
	const char *ct = s->config.cursor_theme[0] ? s->config.cursor_theme : s->theme.cursor_theme;
	int cs = s->config.cursor_size > 0 ? s->config.cursor_size : s->theme.cursor_size;
	s->xcursor = wlr_xcursor_manager_create(ct, (uint32_t)cs);
	char size[16];
	snprintf(size, sizeof size, "%d", cs);
	setenv("XCURSOR_THEME", ct, 1);
	setenv("XCURSOR_SIZE", size, 1);
	wlr_xcursor_manager_load(s->xcursor, 1);

	s->cursor_motion.notify = cursor_motion;
	wl_signal_add(&s->cursor->events.motion, &s->cursor_motion);
	s->cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&s->cursor->events.motion_absolute, &s->cursor_motion_absolute);
	s->cursor_button.notify = cursor_button;
	wl_signal_add(&s->cursor->events.button, &s->cursor_button);
	s->cursor_axis.notify = cursor_axis;
	wl_signal_add(&s->cursor->events.axis, &s->cursor_axis);
	s->cursor_frame.notify = cursor_frame;
	wl_signal_add(&s->cursor->events.frame, &s->cursor_frame);

	s->new_input.notify = new_input;
	wl_signal_add(&s->backend->events.new_input, &s->new_input);

	s->seat = wlr_seat_create(s->display, "seat0");
	s->request_cursor.notify = request_cursor;
	wl_signal_add(&s->seat->events.request_set_cursor, &s->request_cursor);
	s->request_set_selection.notify = request_set_selection;
	wl_signal_add(&s->seat->events.request_set_selection, &s->request_set_selection);
	s->request_set_primary_selection.notify = request_set_primary_selection;
	wl_signal_add(&s->seat->events.request_set_primary_selection, &s->request_set_primary_selection);
	s->request_start_drag.notify = request_start_drag;
	wl_signal_add(&s->seat->events.request_start_drag, &s->request_start_drag);
	s->start_drag.notify = start_drag;
	wl_signal_add(&s->seat->events.start_drag, &s->start_drag);

	struct wlr_cursor_shape_manager_v1 *shape = wlr_cursor_shape_manager_v1_create(s->display, 1);
	s->cursor_shape_request.notify = cursor_shape_request;
	wl_signal_add(&shape->events.request_set_shape, &s->cursor_shape_request);

	s->relative_pointer = wlr_relative_pointer_manager_v1_create(s->display);
	s->constraints = wlr_pointer_constraints_v1_create(s->display);
	s->new_constraint.notify = new_constraint;
	wl_signal_add(&s->constraints->events.new_constraint, &s->new_constraint);

	s->snap_preview = wlr_scene_rect_create(s->layers[L_UI], 10, 10, (float[4]){0.2f, 0.4f, 0.8f, 0.3f});
	wlr_scene_node_set_enabled(&s->snap_preview->node, false);
}

/* For automated tests (chronoctl pointer X Y [left|right|middle]): move the pointer and click,
 * exactly as if a mouse had done it. */
void input_debug_pointer(struct chrono_server *s, double x, double y, const char *button)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	uint32_t ms = (uint32_t)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
	/* a headless test machine has no mouse, so clients never asked for wl_pointer: advertise one */
	if (!(s->seat->capabilities & WL_SEAT_CAPABILITY_POINTER))
		wlr_seat_set_capabilities(s->seat, s->seat->capabilities | WL_SEAT_CAPABILITY_POINTER);
	wlr_cursor_warp_closest(s->cursor, NULL, x, y);
	cursor_update_focus(s, ms);
	wlr_seat_pointer_notify_frame(s->seat);
	uint32_t code = !strcmp(button, "left") ? BTN_LEFT : !strcmp(button, "right") ? BTN_RIGHT
		      : !strcmp(button, "middle") ? BTN_MIDDLE : 0;
	if (!code)
		return;
	struct wlr_pointer_button_event e = {.time_msec = ms, .button = code, .state = WL_POINTER_BUTTON_STATE_PRESSED};
	s->cursor_button.notify(&s->cursor_button, &e);
	wlr_seat_pointer_notify_frame(s->seat);
	e.state = WL_POINTER_BUTTON_STATE_RELEASED;
	e.time_msec = ms + 30;
	s->cursor_button.notify(&s->cursor_button, &e);
	wlr_seat_pointer_notify_frame(s->seat);
}
