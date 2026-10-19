/* Chrono — a Wayland desktop whose themes span every era of computing.
 * Compositor entry point: sets everything up, starts the shell and autostart programs, runs. */
#define _GNU_SOURCE
#include <execinfo.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/util/log.h>

#include "../common/ini.h"
#include "server.h"

static struct chrono_server server;
static int shell_restarts;
static time_t shell_restart_window;

/* ------------------------------------------------------------------------------- processes */
static pid_t spawn_argv_shell(const char *cmd)
{
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);
		signal(SIGCHLD, SIG_DFL);
		signal(SIGPIPE, SIG_DFL);
		execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
		_exit(127);
	}
	return pid;
}

void server_spawn(struct chrono_server *s, const char *cmd)
{
	(void)s;
	if (cmd && *cmd)
		spawn_argv_shell(cmd);
}

static void start_shell(struct chrono_server *s)
{
	if (!s->config.start_shell)
		return;
	s->shell_pid = spawn_argv_shell("exec chrono-shell");
}

static int handle_sigchld(int sig, void *data)
{
	struct chrono_server *s = data;
	(void)sig;
	int status;
	pid_t pid;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if (pid != s->shell_pid || pid <= 0)
			continue;
		s->shell_pid = 0;
		/* the panel crashed: bring it back, but don't loop forever on a broken install */
		time_t now = time(NULL);
		if (now - shell_restart_window > 60) {
			shell_restart_window = now;
			shell_restarts = 0;
		}
		if (++shell_restarts <= 5) {
			wlr_log(WLR_INFO, "chrono-shell exited (status %d), restarting", status);
			start_shell(s);
		} else {
			wlr_log(WLR_ERROR, "chrono-shell keeps exiting; not restarting it again");
		}
	}
	return 0;
}

static int handle_terminate(int sig, void *data)
{
	(void)sig;
	server_quit(data);
	return 0;
}

static int handle_sighup(int sig, void *data)
{
	(void)sig;
	server_reload(data);
	return 0;
}

void server_quit(struct chrono_server *s)
{
	if (s->shell_pid > 0)
		kill(s->shell_pid, SIGTERM);
	s->config.start_shell = false;
	wl_display_terminate(s->display);
}

/* ------------------------------------------------------------------------------- theme + config */
static void reload_cursor_theme(struct chrono_server *s)
{
	const char *ct = s->config.cursor_theme[0] ? s->config.cursor_theme : s->theme.cursor_theme;
	int cs = s->config.cursor_size > 0 ? s->config.cursor_size : s->theme.cursor_size;
	if (s->xcursor)
		wlr_xcursor_manager_destroy(s->xcursor);
	s->xcursor = wlr_xcursor_manager_create(ct, (uint32_t)cs);
	wlr_xcursor_manager_load(s->xcursor, 1);
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link)
		wlr_xcursor_manager_load(s->xcursor, o->wlr_output->scale);
	char size[16];
	snprintf(size, sizeof size, "%d", cs);
	setenv("XCURSOR_THEME", ct, 1);
	setenv("XCURSOR_SIZE", size, 1);
	s->cursor_image = NULL;
	cursor_set_image(s, "default");
}

static void load_theme(struct chrono_server *s, const char *id)
{
	if (!chrono_theme_load_prefs(&s->theme, id, s->config.appearance_mode, s->config.accent, s->config.path))
		wlr_log(WLR_ERROR, "theme '%s' not found, using the built-in default", id);
	s->theme_gen++;
	decor_rebuild_shadow_templates(s);
}

void server_refresh_theme(struct chrono_server *s)
{
	load_theme(s, s->config.theme);
	reload_cursor_theme(s);
	views_redecorate(s);
	output_update_desktops(s);
	if (lock_active(s))
		lock_redraw(s);
	char *b = NULL;
	size_t n = 0;
	jbuf_append(&b, &n, "{\"id\":");
	jbuf_string(&b, &n, s->theme.id);
	jbuf_append(&b, &n, "}");
	ipc_event(s, "theme", b);
	free(b);
}

void server_apply_theme(struct chrono_server *s, const char *id)
{
	load_theme(s, id);
	/* remember what was picked, not the light/dark pair we may be showing instead */
	if (strcmp(s->config.theme, id) && s->config.theme != id)
		snprintf(s->config.theme, sizeof s->config.theme, "%s", id);
	chrono_ini_set_in_file(s->config.path, "general", "theme", s->config.theme);
	reload_cursor_theme(s);
	views_redecorate(s);
	output_update_desktops(s);
	if (lock_active(s))
		lock_redraw(s);
	char *b = NULL;
	size_t n = 0;
	jbuf_append(&b, &n, "{\"id\":");
	jbuf_string(&b, &n, s->theme.id);
	jbuf_append(&b, &n, ",\"path\":");
	jbuf_string(&b, &n, s->theme.path);
	jbuf_append(&b, &n, "}");
	ipc_event(s, "theme", b);
	free(b);
}

void server_reload(struct chrono_server *s)
{
	char old_theme[64];
	snprintf(old_theme, sizeof old_theme, "%s", s->config.theme);
	int old_ws = s->config.workspaces;
	config_load(&s->config);
	if (s->workspace >= s->config.workspaces)
		workspace_switch(s, s->config.workspaces - 1);
	if (old_ws > s->config.workspaces) {
		struct chrono_view *v;
		wl_list_for_each(v, &s->views, link)
			if (v->workspace >= s->config.workspaces)
				view_to_workspace(v, s->config.workspaces - 1);
	}
	if (strcmp(old_theme, s->config.theme))
		server_apply_theme(s, s->config.theme);
	else
		server_refresh_theme(s);
	input_apply_config(s);
	night_light_update(s);
	idle_reset(s);
	ipc_event(s, "config", NULL);
}

/* ------------------------------------------------------------------------------- activation */
static void xdg_activation_request(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, xdg_activation_request);
	struct wlr_xdg_activation_v1_request_activate_event *e = data;
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link) {
		if (v->surface == e->surface && v->mapped) {
			view_focus(v);
			return;
		}
	}
}

/* ------------------------------------------------------------------------------- environment */
static void export_environment(struct chrono_server *s)
{
	setenv("WAYLAND_DISPLAY", s->socket_name, 1);
	setenv("XDG_CURRENT_DESKTOP", "Chrono", 1);
	setenv("XDG_SESSION_DESKTOP", "Chrono", 1);
	setenv("XDG_SESSION_TYPE", "wayland", 1);
	setenv("MOZ_ENABLE_WAYLAND", "1", 0);
	setenv("QT_QPA_PLATFORM", "wayland;xcb", 0);
	setenv("SDL_VIDEODRIVER", "wayland,x11", 0);
	setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 0);
	setenv("ELECTRON_OZONE_PLATFORM_HINT", "auto", 0);
	/* let D-Bus activated services (portals, notifications) and systemd user units see the session */
	server_spawn(s, "dbus-update-activation-environment --systemd WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP "
			"XDG_SESSION_TYPE XDG_SESSION_DESKTOP CHRONO_SOCK XCURSOR_THEME XCURSOR_SIZE 2>/dev/null || "
			"dbus-update-activation-environment WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP 2>/dev/null");
}

static void run_autostart(struct chrono_server *s)
{
	char *list = strdup(s->config.autostart), *save = NULL;
	for (char *cmd = strtok_r(list, ";", &save); cmd; cmd = strtok_r(NULL, ";", &save)) {
		while (*cmd == ' ')
			cmd++;
		if (*cmd)
			server_spawn(s, cmd);
	}
	free(list);
	/* XDG autostart entries (~/.config/autostart, /etc/xdg/autostart) */
	server_spawn(s, "command -v chrono-autostart >/dev/null && exec chrono-autostart");
}

static void usage(void)
{
	printf("chrono %s — a Wayland desktop with themes from every era\n\n"
	       "usage: chrono [options]\n"
	       "  -s, --startup CMD   run CMD once the desktop is up\n"
	       "      --no-shell      don't start chrono-shell (panel, dock, launcher)\n"
	       "  -d, --debug         verbose logging\n"
	       "  -v, --version       print the version\n\n"
	       "Config: ~/.config/chrono/chrono.conf · Themes: ~/.local/share/chrono/themes\n"
	       "Control a running desktop with chronoctl.\n",
	       CHRONO_VERSION);
}

/* If we ever crash, put a backtrace in the session log (~/.local/state/chrono/chrono.log) so it
 * can be reported; then die the normal way so a core dump is still written. */
static void crash_handler(int sig)
{
	static const char msg[] = "\nchrono: crashed — backtrace follows (please report it with this log)\n";
	ssize_t r = write(STDERR_FILENO, msg, sizeof msg - 1);
	(void)r;
	void *frames[64];
	int n = backtrace(frames, 64);
	backtrace_symbols_fd(frames, n, STDERR_FILENO);
	signal(sig, SIG_DFL);
	raise(sig);
}

int main(int argc, char **argv)
{
	signal(SIGSEGV, crash_handler);
	signal(SIGABRT, crash_handler);
	signal(SIGBUS, crash_handler);
	signal(SIGFPE, crash_handler);
	const char *startup = NULL;
	bool no_shell = false, debug = false;
	static const struct option opts[] = {
		{"startup", required_argument, NULL, 's'}, {"no-shell", no_argument, NULL, 'n'},
		{"debug", no_argument, NULL, 'd'}, {"version", no_argument, NULL, 'v'}, {"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0}};
	int c;
	while ((c = getopt_long(argc, argv, "s:dvh", opts, NULL)) != -1) {
		switch (c) {
		case 's': startup = optarg; break;
		case 'n': no_shell = true; break;
		case 'd': debug = true; break;
		case 'v': printf("chrono %s\n", CHRONO_VERSION); return 0;
		default: usage(); return c == 'h' ? 0 : 1;
		}
	}
	wlr_log_init(debug ? WLR_DEBUG : WLR_INFO, NULL);

	struct chrono_server *s = &server;
	config_load(&s->config);
	if (no_shell)
		s->config.start_shell = false;
	chrono_theme_load_prefs(&s->theme, s->config.theme, s->config.appearance_mode, s->config.accent, s->config.path);
	s->theme_gen = 1;
	s->ipc_fd = -1;
	wl_list_init(&s->views);

	s->display = wl_display_create();
	s->loop = wl_display_get_event_loop(s->display);
	s->backend = wlr_backend_autocreate(s->loop, &s->session);
	if (!s->backend) {
		wlr_log(WLR_ERROR, "can't start a backend (run from a TTY, or inside another Wayland/X11 session)");
		return 1;
	}
	s->renderer = wlr_renderer_autocreate(s->backend);
	if (!s->renderer) {
		wlr_log(WLR_ERROR, "can't create a renderer");
		return 1;
	}
	wlr_renderer_init_wl_display(s->renderer, s->display);
	s->allocator = wlr_allocator_autocreate(s->backend, s->renderer);

	s->compositor = wlr_compositor_create(s->display, 6, s->renderer);
	wlr_subcompositor_create(s->display);
	wlr_data_device_manager_create(s->display);
	wlr_primary_selection_v1_device_manager_create(s->display);
	wlr_data_control_manager_v1_create(s->display);
	wlr_ext_data_control_manager_v1_create(s->display, 1);
	wlr_viewporter_create(s->display);
	wlr_single_pixel_buffer_manager_v1_create(s->display);
	wlr_fractional_scale_manager_v1_create(s->display, 1);
	wlr_presentation_create(s->display, s->backend, 2);
	wlr_alpha_modifier_v1_create(s->display);
	wlr_content_type_manager_v1_create(s->display, 1);
	wlr_screencopy_manager_v1_create(s->display);
	wlr_export_dmabuf_manager_v1_create(s->display);
	wlr_ext_image_copy_capture_manager_v1_create(s->display, 1);
	wlr_ext_output_image_capture_source_manager_v1_create(s->display, 1);
	if (wlr_renderer_get_texture_formats(s->renderer, WLR_BUFFER_CAP_DMABUF))
		wlr_linux_dmabuf_v1_create_with_renderer(s->display, 4, s->renderer);

	s->scene = wlr_scene_create();
	for (int i = 0; i < L_COUNT; i++)
		s->layers[i] = wlr_scene_tree_create(&s->scene->tree);

	output_init(s);
	view_init(s);
	layer_init(s);
	input_init(s);
	decor_rebuild_shadow_templates(s);
	lock_init(s);
	idle_init(s);

	s->xdg_activation = wlr_xdg_activation_v1_create(s->display);
	s->xdg_activation_request.notify = xdg_activation_request;
	wl_signal_add(&s->xdg_activation->events.request_activate, &s->xdg_activation_request);

	const char *sock = wl_display_add_socket_auto(s->display);
	if (!sock) {
		wlr_log(WLR_ERROR, "can't create a Wayland socket");
		wlr_backend_destroy(s->backend);
		return 1;
	}
	snprintf(s->socket_name, sizeof s->socket_name, "%s", sock);

	xwayland_init(s);
	ipc_init(s);
	themewatch_init(s);

	if (!wlr_backend_start(s->backend)) {
		wlr_log(WLR_ERROR, "can't start the backend");
		wlr_backend_destroy(s->backend);
		wl_display_destroy(s->display);
		return 1;
	}
	export_environment(s);
	cursor_set_image(s, "default");

	wl_event_loop_add_signal(s->loop, SIGCHLD, handle_sigchld, s);
	wl_event_loop_add_signal(s->loop, SIGTERM, handle_terminate, s);
	wl_event_loop_add_signal(s->loop, SIGINT, handle_terminate, s);
	wl_event_loop_add_signal(s->loop, SIGHUP, handle_sighup, s);
	signal(SIGPIPE, SIG_IGN);

	start_shell(s);
	run_autostart(s);
	if (startup)
		server_spawn(s, startup);

	wlr_log(WLR_INFO, "Chrono %s running on WAYLAND_DISPLAY=%s (theme: %s)", CHRONO_VERSION, sock, s->theme.name);
	wl_display_run(s->display);

	ipc_finish(s);
	themewatch_finish();
	wl_display_destroy_clients(s->display);
#ifdef CHRONO_HAVE_XWAYLAND
	if (s->xwayland) {
		wl_list_remove(&s->xwayland_ready.link);
		wl_list_remove(&s->new_xwayland_surface.link);
		wlr_xwayland_destroy(s->xwayland);
	}
#endif
	/* wlroots 0.20 asserts that nothing still listens to an object being destroyed */
	struct wl_listener *global_listeners[] = {
		&s->xdg_activation_request, &s->output_layout_change, &s->new_output, &s->output_manager_apply,
		&s->output_manager_test, &s->set_output_power_mode, &s->set_gamma, &s->cursor_motion,
		&s->cursor_motion_absolute, &s->cursor_button, &s->cursor_axis, &s->cursor_frame, &s->new_input,
		&s->request_cursor, &s->request_set_selection, &s->request_set_primary_selection, &s->request_start_drag,
		&s->start_drag, &s->cursor_shape_request, &s->new_constraint, &s->new_lock, &s->new_layer_surface,
		&s->new_xdg_toplevel, &s->new_xdg_popup, &s->new_decoration, &s->kde_new_decoration, &s->new_inhibitor,
	};
	for (size_t i = 0; i < sizeof global_listeners / sizeof *global_listeners; i++)
		if (global_listeners[i]->link.next)
			wl_list_remove(&global_listeners[i]->link);
	/* outputs and input devices go first: their destroy handlers still use the scene and cursor */
	wlr_backend_destroy(s->backend);
	wlr_scene_node_destroy(&s->scene->tree.node);
	wlr_xcursor_manager_destroy(s->xcursor);
	wlr_cursor_destroy(s->cursor);
	wlr_allocator_destroy(s->allocator);
	wlr_renderer_destroy(s->renderer);
	wl_display_destroy(s->display);
	return 0;
}
