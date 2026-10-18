/* Chrono compositor — shared state. */
#ifndef CHRONO_SERVER_H
#define CHRONO_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#include "../common/theme.h"
#include "config.h"

#ifdef CHRONO_HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif

#define CHRONO_MAX_WORKSPACES 9

struct chrono_server;
struct chrono_view;

/* scene layers, bottom to top */
enum layer_index {
	L_DESKTOP,      /* fallback wallpaper painted from the theme */
	L_BACKGROUND,   /* layer-shell background */
	L_BOTTOM,
	L_VIEWS,
	L_TOP,          /* panels */
	L_FULLSCREEN,
	L_OVERLAY,
	L_UI,           /* snap preview, window switcher */
	L_LOCK,
	L_DRAG,
	L_COUNT
};

enum cursor_mode { CURSOR_PASSTHROUGH, CURSOR_MOVE, CURSOR_RESIZE, CURSOR_PRESS_BUTTON };

enum decor_part {
	PART_NONE = 0,
	PART_TITLE,
	PART_BUTTON,
	PART_EDGE,      /* resize: `edges` says which */
};

struct decor_hit {
	enum decor_part part;
	int button;             /* index into the geometry's buttons for PART_BUTTON */
	uint32_t edges;         /* WLR_EDGE_* for PART_EDGE */
};

/* Server-side decoration: shadow (9-slice of a shared template), title buffer, edges */
struct decor {
	struct wlr_scene_tree *tree;          /* child of the view tree, at (0,0) = frame top-left */
	struct wlr_scene_tree *shadow_tree;
	struct wlr_scene_buffer *shadow[8];
	struct wlr_scene_buffer *top, *left, *right, *bottom;
	/* what was painted last, to skip needless repaints */
	int painted_w, painted_h, painted_hover, painted_pressed;
	bool painted_active;
	char *painted_title;
	unsigned painted_theme_gen;
	int hover, pressed;     /* button index or -1 */
};

struct chrono_view {
	struct wl_list link;            /* server->views, front = topmost */
	struct chrono_server *server;
	uint32_t id;
	bool is_x11;
	struct wlr_xdg_toplevel *xdg;
#ifdef CHRONO_HAVE_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
#endif
	struct wlr_surface *surface;

	struct wlr_scene_tree *tree;    /* frame root, positioned at the frame's top-left */
	struct wlr_scene_tree *content; /* client surface tree */
	struct decor decor;
	bool ssd;                       /* we draw the frame */

	int x, y;                       /* client content top-left, layout coords */
	int w, h;                       /* client content size */
	struct wlr_box restore;         /* geometry before maximize / snap / fullscreen */
	bool mapped, minimized, maximized, fullscreen, active, urgent;
	int snap;                       /* 0, or WLR_EDGE_* combination when snapped */
	int workspace;
	char *title, *app_id;
	uint32_t pending_serial;
	bool placed;

	struct wlr_foreign_toplevel_handle_v1 *ftl;
	struct wlr_ext_foreign_toplevel_handle_v1 *ext_ftl;

	struct wl_listener map, unmap, destroy, commit;
	struct wl_listener request_move, request_resize, request_maximize, request_minimize, request_fullscreen;
	struct wl_listener set_title, set_app_id, new_popup;
	struct wl_listener ftl_request_activate, ftl_request_close, ftl_request_minimize, ftl_request_maximize,
		ftl_request_fullscreen;
#ifdef CHRONO_HAVE_XWAYLAND
	struct wl_listener associate, dissociate, request_configure, request_activate, set_class;
#endif
};

struct chrono_output {
	struct wl_list link;
	struct chrono_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_box full, usable;           /* layout coords */
	struct wlr_scene_buffer *desktop;      /* fallback wallpaper */
	int desktop_w, desktop_h;
	unsigned desktop_gen;
	struct wlr_scene_tree *layers[4];      /* layer-shell trees per layer, children of the global layer trees */
	struct wlr_scene_buffer *lock_buf;
	struct wl_listener frame, request_state, destroy;
};

struct chrono_keyboard {
	struct wl_list link;
	struct chrono_server *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener modifiers, key, destroy;
};

struct switcher {
	bool active;
	struct wl_list order;       /* not owned: snapshot of views by id */
	uint32_t ids[256];
	int count, index;
	struct wlr_scene_buffer *buf;
};

struct lock_state {
	bool locked;                /* our own lock screen */
	bool ext_locked;            /* an ext-session-lock client holds the lock */
	char password[256];
	int len;
	bool checking, failed;
	int auth_fd;
	pid_t auth_pid;
	struct wl_event_source *auth_source;
	struct wl_event_source *tick;
	struct wlr_session_lock_v1 *ext_lock;
	struct wl_listener ext_new_surface, ext_unlock, ext_destroy;
};

struct chrono_server {
	struct wl_display *display;
	struct wl_event_loop *loop;
	struct wlr_backend *backend;
	struct wlr_session *session;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_scene_tree *layers[L_COUNT];
	struct wlr_scene_tree *workspaces[CHRONO_MAX_WORKSPACES];
	struct wlr_scene_tree *unmanaged;      /* X11 menus / tooltips */
	struct wlr_scene_rect *snap_preview;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_layer_shell_v1 *layer_shell;
	struct wlr_output_layout *output_layout;
	struct wlr_output_manager_v1 *output_manager;
	struct wlr_xdg_activation_v1 *xdg_activation;
	struct wlr_foreign_toplevel_manager_v1 *ftl_manager;
	struct wlr_ext_foreign_toplevel_list_v1 *ext_ftl_list;
	struct wlr_idle_notifier_v1 *idle_notifier;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit;
	struct wlr_relative_pointer_manager_v1 *relative_pointer;
	struct wlr_pointer_constraints_v1 *constraints;
	struct wlr_pointer_constraint_v1 *active_constraint;
	struct wlr_session_lock_manager_v1 *lock_manager;
	struct wlr_output_power_manager_v1 *output_power;
	struct wlr_gamma_control_manager_v1 *gamma;
#ifdef CHRONO_HAVE_XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_ready, new_xwayland_surface;
#endif

	struct wl_list outputs;
	struct wl_list views;
	struct wl_list keyboards;
	uint32_t next_view_id;
	int workspace;                  /* current, 0-based */
	struct chrono_view *focused;

	/* input */
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor;
	struct wlr_seat *seat;
	enum cursor_mode cursor_mode;
	struct chrono_view *grab_view;
	double grab_x, grab_y;
	struct wlr_box grab_geo;
	uint32_t resize_edges;
	int pressed_button_view_id, pressed_button;
	uint32_t last_title_click;
	uint32_t last_title_click_view;
	int pending_snap;               /* edges the current move would snap to */
	const char *cursor_image;

	struct switcher switcher;
	struct lock_state lock;

	/* idle + night light */
	struct wl_event_source *idle_timer;
	bool screens_off;
	struct wl_event_source *night_timer;
	bool night_active;

	/* theme + config */
	struct chrono_theme theme;
	unsigned theme_gen;
	struct chrono_config config;
	struct wlr_buffer *shadow_tmpl[2];     /* inactive, active */
	int shadow_tmpl_size, shadow_corner;

	/* IPC */
	int ipc_fd;
	char ipc_path[256];
	struct wl_event_source *ipc_source;
	struct wl_list ipc_clients;

	pid_t shell_pid;
	char socket_name[64];

	struct wl_listener new_output, new_xdg_toplevel, new_xdg_popup, new_layer_surface, new_input,
		cursor_motion, cursor_motion_absolute, cursor_button, cursor_axis, cursor_frame, request_cursor,
		request_set_selection, request_set_primary_selection, request_start_drag, start_drag,
		new_decoration, output_manager_apply, output_manager_test, output_layout_change, xdg_activation_request,
		new_constraint, new_lock, set_output_power_mode, new_inhibitor, set_gamma, cursor_shape_request,
		kde_new_decoration;
};

/* main.c */
void server_spawn(struct chrono_server *s, const char *cmd);
void server_reload(struct chrono_server *s);
void server_apply_theme(struct chrono_server *s, const char *id);
void server_quit(struct chrono_server *s);

/* output.c */
void output_init(struct chrono_server *s);
struct chrono_output *output_at(struct chrono_server *s, double lx, double ly);
struct chrono_output *output_of_view(struct chrono_view *v);
void output_arrange_all(struct chrono_server *s);
void output_update_desktops(struct chrono_server *s);
void output_publish_config(struct chrono_server *s);
bool output_apply_settings(struct chrono_server *s, const char *name, const char *settings, char *err, size_t errlen);
void output_save(struct chrono_server *s, struct chrono_output *o);
void outputs_set_power(struct chrono_server *s, bool on);
void night_light_update(struct chrono_server *s);
void output_write_json(struct chrono_server *s, char **buf, size_t *len);

/* layer.c */
void layer_init(struct chrono_server *s);
void layers_arrange(struct chrono_output *o);
struct wlr_layer_surface_v1 *layer_exclusive_keyboard(struct chrono_server *s);

/* view.c */
void view_init(struct chrono_server *s);
void view_focus(struct chrono_view *v);
void view_focus_top(struct chrono_server *s);
void view_close(struct chrono_view *v);
void view_set_maximized(struct chrono_view *v, bool on);
void view_set_minimized(struct chrono_view *v, bool on);
void view_set_fullscreen(struct chrono_view *v, bool on);
void view_snap(struct chrono_view *v, uint32_t edges);
void view_move_to(struct chrono_view *v, int x, int y);
void view_resize(struct chrono_view *v, int x, int y, int w, int h);
void view_update_position(struct chrono_view *v);
void view_frame_box(struct chrono_view *v, struct wlr_box *box);
struct chrono_view *view_at(struct chrono_server *s, double lx, double ly, struct wlr_surface **surface,
			    double *sx, double *sy, struct decor_hit *hit);
struct chrono_view *view_by_id(struct chrono_server *s, uint32_t id);
void view_to_workspace(struct chrono_view *v, int ws);
void workspace_switch(struct chrono_server *s, int ws);
void views_redecorate(struct chrono_server *s);
void view_set_title(struct chrono_view *v, const char *title);
void view_set_app_id(struct chrono_view *v, const char *app_id);
void view_mapped(struct chrono_view *v);
void view_unmapped(struct chrono_view *v);
void view_destroyed(struct chrono_view *v);
void view_ftl_create(struct chrono_view *v);
void view_write_json(struct chrono_server *s, char **buf, size_t *len);

/* decor.c */
void decor_create(struct chrono_view *v);
void decor_destroy(struct chrono_view *v);
void decor_update(struct chrono_view *v);
void decor_offsets(struct chrono_view *v, int *left, int *top, int *right, int *bottom);
struct decor_hit decor_hit_test(struct chrono_view *v, double fx, double fy);
bool decor_node_is_frame(struct wlr_scene_node *node);
void decor_rebuild_shadow_templates(struct chrono_server *s);

/* buffer.c */
struct wlr_buffer *cairo_buffer_create(int w, int h, float scale, cairo_t **cr_out);
void cairo_buffer_done(struct wlr_buffer *buf);

/* input.c */
void input_init(struct chrono_server *s);
void input_apply_config(struct chrono_server *s);
void cursor_update_focus(struct chrono_server *s, uint32_t time);
void cursor_set_image(struct chrono_server *s, const char *name);
void begin_interactive(struct chrono_view *v, enum cursor_mode mode, uint32_t edges);
void keyboard_focus_surface(struct chrono_server *s, struct wlr_surface *surface);
void input_activity(struct chrono_server *s);
bool run_action(struct chrono_server *s, const char *action);

/* switcher.c */
void switcher_next(struct chrono_server *s, int dir);
void switcher_commit(struct chrono_server *s);
void switcher_cancel(struct chrono_server *s);

/* lock.c */
void lock_init(struct chrono_server *s);
void lock_screen(struct chrono_server *s);
bool lock_handle_key(struct chrono_server *s, xkb_keysym_t sym, uint32_t codepoint);
void lock_redraw(struct chrono_server *s);
bool lock_active(struct chrono_server *s);

/* idle.c */
void idle_init(struct chrono_server *s);
void idle_reset(struct chrono_server *s);

/* ipc.c */
void ipc_init(struct chrono_server *s);
void ipc_finish(struct chrono_server *s);
void ipc_event(struct chrono_server *s, const char *event, const char *json_payload);

/* xwayland.c */
void xwayland_init(struct chrono_server *s);

/* json helpers (ipc.c) */
void jbuf_append(char **buf, size_t *len, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void jbuf_string(char **buf, size_t *len, const char *s);

#endif
