/* Locking: Chrono's own themed lock screen (PAM, checked in a child process so the desktop never
 * blocks), plus ext-session-lock-v1 for external lockers such as swaylock or hyprlock.
 * While locked, no window gets input or is shown: the lock layer covers every output. */
#define _GNU_SOURCE
#include <cairo.h>
#include <errno.h>
#include <fcntl.h>
#include <pango/pangocairo.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/util/log.h>

#include "../common/paint.h"
#include "server.h"

bool lock_active(struct chrono_server *s)
{
	return s->lock.locked || s->lock.ext_locked;
}

static const char *user_name(void)
{
	struct passwd *pw = getpwuid(getuid());
	return pw ? pw->pw_name : "user";
}

static const char *real_name(void)
{
	static char buf[128];
	struct passwd *pw = getpwuid(getuid());
	if (pw && pw->pw_gecos && *pw->pw_gecos) {
		snprintf(buf, sizeof buf, "%.*s", (int)strcspn(pw->pw_gecos, ","), pw->pw_gecos);
		if (*buf)
			return buf;
	}
	return user_name();
}

static void text(cairo_t *cr, const char *font, double size, bool bold, const char *s, double cx, double y,
		 const struct chrono_rgba *c)
{
	PangoLayout *l = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(font);
	pango_font_description_set_absolute_size(fd, size * PANGO_SCALE);
	if (bold)
		pango_font_description_set_weight(fd, PANGO_WEIGHT_BOLD);
	pango_layout_set_font_description(l, fd);
	pango_font_description_free(fd);
	pango_layout_set_text(l, s, -1);
	int w, h;
	pango_layout_get_pixel_size(l, &w, &h);
	cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
	cairo_move_to(cr, cx - w / 2.0, y);
	pango_cairo_show_layout(cr, l);
	cairo_new_path(cr);
	g_object_unref(l);
}

static void paint_lock(struct chrono_server *s, cairo_t *cr, int w, int h, bool primary)
{
	const struct chrono_theme *t = &s->theme;
	chrono_paint_desktop(cr, t, w, h);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
	cairo_paint(cr);
	if (!primary)
		return;
	struct chrono_rgba white = {1, 1, 1, 1}, dim = {1, 1, 1, 0.75};
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char clock[16], date[64];
	strftime(clock, sizeof clock, "%H:%M", &tm);
	strftime(date, sizeof date, "%A, %B %e", &tm);
	text(cr, t->font_ui, h * 0.11, true, clock, w / 2.0, h * 0.18, &white);
	text(cr, t->font_ui, h * 0.025, false, date, w / 2.0, h * 0.18 + h * 0.14, &dim);

	/* user + password field */
	double cy = h * 0.6, fw = 300, fh = 40;
	cairo_arc(cr, w / 2.0, cy - 70, 34, 0, 2 * M_PI);
	cairo_set_source_rgba(cr, t->accent.r, t->accent.g, t->accent.b, 1);
	cairo_fill(cr);
	char initial[8];
	snprintf(initial, sizeof initial, "%c", real_name()[0] ? real_name()[0] - (real_name()[0] >= 'a' && real_name()[0] <= 'z' ? 32 : 0) : '?');
	text(cr, t->font_ui, 30, true, initial, w / 2.0, cy - 70 - 20, &t->accent_fg);
	text(cr, t->font_ui, 18, true, real_name(), w / 2.0, cy - 26, &white);

	double fx = w / 2.0 - fw / 2;
	double r = !strcmp(t->panel_style, "bevel") || !strcmp(t->panel_style, "pixel") ? 0 : fh / 2;
	cairo_new_sub_path(cr);
	if (r > 0) {
		cairo_arc(cr, fx + fw - r, cy + 10 + r, r, -M_PI / 2, M_PI / 2);
		cairo_arc(cr, fx + r, cy + 10 + r, r, M_PI / 2, 1.5 * M_PI);
		cairo_close_path(cr);
	} else {
		cairo_rectangle(cr, fx, cy + 10, fw, fh);
	}
	cairo_set_source_rgba(cr, 1, 1, 1, 0.16);
	cairo_fill_preserve(cr);
	if (s->lock.failed)
		cairo_set_source_rgba(cr, 1, 0.35, 0.35, 0.9);
	else
		cairo_set_source_rgba(cr, 1, 1, 1, 0.35);
	cairo_set_line_width(cr, 1.5);
	cairo_stroke(cr);
	int n = s->lock.len > 24 ? 24 : s->lock.len;
	for (int i = 0; i < n; i++) {
		cairo_arc(cr, w / 2.0 - (n - 1) * 8 + i * 16, cy + 10 + fh / 2, 4.5, 0, 2 * M_PI);
		cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
		cairo_fill(cr);
	}
	const char *hint = s->lock.checking ? "Checking…"
			 : s->lock.failed ? "That password didn't work"
			 : n == 0 ? "Type your password to unlock" : "";
	if (*hint)
		text(cr, t->font_ui, 14, false, hint, w / 2.0, cy + 10 + fh + 14, s->lock.failed ? &white : &dim);
}

void lock_redraw(struct chrono_server *s)
{
	struct chrono_output *o;
	bool primary_done = false;
	wl_list_for_each(o, &s->outputs, link) {
		if (!s->lock.locked || s->lock.ext_locked || o->full.width <= 0) {
			if (o->lock_buf)
				wlr_scene_node_set_enabled(&o->lock_buf->node, false);
			continue;
		}
		/* the clock and password box go on the output under the cursor */
		bool primary = !primary_done && output_at(s, s->cursor->x, s->cursor->y) == o;
		if (primary)
			primary_done = true;
		cairo_t *cr;
		struct wlr_buffer *buf = cairo_buffer_create(o->full.width, o->full.height, o->wlr_output->scale, &cr);
		if (!buf)
			continue;
		paint_lock(s, cr, o->full.width, o->full.height, primary);
		cairo_buffer_done(buf);
		if (!o->lock_buf)
			o->lock_buf = wlr_scene_buffer_create(s->layers[L_LOCK], NULL);
		wlr_scene_buffer_set_buffer(o->lock_buf, buf);
		wlr_scene_buffer_set_dest_size(o->lock_buf, o->full.width, o->full.height);
		wlr_buffer_drop(buf);
		wlr_scene_node_set_position(&o->lock_buf->node, o->full.x, o->full.y);
		wlr_scene_node_set_enabled(&o->lock_buf->node, true);
	}
}

static int lock_tick(void *data)
{
	struct chrono_server *s = data;
	if (s->lock.locked && !s->lock.ext_locked)
		lock_redraw(s);
	wl_event_source_timer_update(s->lock.tick, 20 * 1000);
	return 0;
}

static void unlock(struct chrono_server *s)
{
	s->lock.locked = false;
	s->lock.failed = false;
	explicit_bzero(s->lock.password, sizeof s->lock.password);
	s->lock.len = 0;
	lock_redraw(s);
	wlr_scene_node_set_enabled(&s->layers[L_LOCK]->node, true);
	if (s->focused)
		keyboard_focus_surface(s, s->focused->surface);
	else
		view_focus_top(s);
	cursor_update_focus(s, 0);
	ipc_event(s, "unlocked", NULL);
}

void lock_screen(struct chrono_server *s)
{
	if (lock_active(s))
		return;
	s->lock.locked = true;
	s->lock.failed = false;
	s->lock.len = 0;
	wlr_seat_keyboard_notify_clear_focus(s->seat);
	wlr_seat_pointer_notify_clear_focus(s->seat);
	switcher_cancel(s);
	lock_redraw(s);
	ipc_event(s, "locked", NULL);
}

/* ------------------------------------------------------------------------------- PAM, in a child */
static const char *pam_pw;

static int conv(int n, const struct pam_message **msg, struct pam_response **resp, void *data)
{
	(void)data;
	struct pam_response *r = calloc((size_t)n, sizeof *r);
	if (!r)
		return PAM_BUF_ERR;
	for (int i = 0; i < n; i++) {
		if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF || msg[i]->msg_style == PAM_PROMPT_ECHO_ON)
			r[i].resp = strdup(pam_pw);
	}
	*resp = r;
	return PAM_SUCCESS;
}

static int auth_done(int fd, uint32_t mask, void *data)
{
	struct chrono_server *s = data;
	(void)mask;
	char c = 0;
	ssize_t n = read(fd, &c, 1);
	wl_event_source_remove(s->lock.auth_source);
	s->lock.auth_source = NULL;
	close(fd);
	waitpid(s->lock.auth_pid, NULL, 0);
	s->lock.checking = false;
	if (n == 1 && c == 'y') {
		unlock(s);
	} else {
		s->lock.failed = true;
		explicit_bzero(s->lock.password, sizeof s->lock.password);
		s->lock.len = 0;
		lock_redraw(s);
	}
	return 0;
}

static void check_password(struct chrono_server *s)
{
	if (s->lock.checking)
		return;
	int p[2];
	if (pipe2(p, O_CLOEXEC) < 0)
		return;
	s->lock.checking = true;
	s->lock.failed = false;
	lock_redraw(s);
	pid_t pid = fork();
	if (pid == 0) {
		close(p[0]);
		pam_pw = s->lock.password;
		struct pam_conv pc = {conv, NULL};
		pam_handle_t *ph = NULL;
		int r = pam_start("chrono", user_name(), &pc, &ph);
		if (r == PAM_SUCCESS)
			r = pam_authenticate(ph, 0);
		if (r == PAM_SUCCESS)
			r = pam_acct_mgmt(ph, 0);
		if (ph)
			pam_end(ph, r);
		char c = r == PAM_SUCCESS ? 'y' : 'n';
		if (write(p[1], &c, 1) != 1) { /* nothing */ }
		_exit(0);
	}
	close(p[1]);
	explicit_bzero(s->lock.password, sizeof s->lock.password);
	if (pid < 0) {
		close(p[0]);
		s->lock.checking = false;
		s->lock.failed = true;
		lock_redraw(s);
		return;
	}
	s->lock.auth_pid = pid;
	s->lock.auth_source = wl_event_loop_add_fd(s->loop, p[0], WL_EVENT_READABLE, auth_done, s);
}

bool lock_handle_key(struct chrono_server *s, xkb_keysym_t sym, uint32_t cp)
{
	if (!s->lock.locked || s->lock.checking)
		return true;
	if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
		if (s->lock.len > 0)
			check_password(s);
		s->lock.len = 0;
		return true;
	}
	if (sym == XKB_KEY_Escape) {
		explicit_bzero(s->lock.password, sizeof s->lock.password);
		s->lock.len = 0;
	} else if (sym == XKB_KEY_BackSpace) {
		/* drop the last UTF-8 character */
		while (s->lock.len > 0 && (s->lock.password[s->lock.len - 1] & 0xC0) == 0x80)
			s->lock.password[--s->lock.len] = 0;
		if (s->lock.len > 0)
			s->lock.password[--s->lock.len] = 0;
	} else if (cp >= 0x20 && cp != 0x7f) {
		char u[8];
		int n = 0;
		if (cp < 0x80) u[n++] = (char)cp;
		else if (cp < 0x800) { u[n++] = (char)(0xC0 | (cp >> 6)); u[n++] = (char)(0x80 | (cp & 0x3F)); }
		else if (cp < 0x10000) { u[n++] = (char)(0xE0 | (cp >> 12)); u[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[n++] = (char)(0x80 | (cp & 0x3F)); }
		else { u[n++] = (char)(0xF0 | (cp >> 18)); u[n++] = (char)(0x80 | ((cp >> 12) & 0x3F)); u[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[n++] = (char)(0x80 | (cp & 0x3F)); }
		if (s->lock.len + n < (int)sizeof s->lock.password - 1) {
			memcpy(s->lock.password + s->lock.len, u, (size_t)n);
			s->lock.len += n;
			s->lock.password[s->lock.len] = 0;
		}
	} else {
		return true;
	}
	s->lock.failed = false;
	lock_redraw(s);
	return true;
}

/* ------------------------------------------------------------------------------- ext-session-lock */
struct lock_surf {
	struct wlr_session_lock_surface_v1 *wlr;
	struct wlr_scene_tree *tree;
	struct wl_listener destroy;
};

static void lock_surf_destroy(struct wl_listener *l, void *data)
{
	struct lock_surf *ls = wl_container_of(l, ls, destroy);
	(void)data;
	wl_list_remove(&ls->destroy.link);
	free(ls);
}

static void ext_new_surface(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, lock.ext_new_surface);
	struct wlr_session_lock_surface_v1 *surf = data;
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		if (o->wlr_output != surf->output)
			continue;
		struct lock_surf *ls = calloc(1, sizeof *ls);
		ls->wlr = surf;
		ls->tree = wlr_scene_subsurface_tree_create(s->layers[L_LOCK], surf->surface);
		wlr_scene_node_set_position(&ls->tree->node, o->full.x, o->full.y);
		ls->destroy.notify = lock_surf_destroy;
		wl_signal_add(&surf->events.destroy, &ls->destroy);
		wlr_session_lock_surface_v1_configure(surf, (uint32_t)o->full.width, (uint32_t)o->full.height);
		if (output_at(s, s->cursor->x, s->cursor->y) == o)
			keyboard_focus_surface(s, surf->surface);
	}
}

static void ext_unlock(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, lock.ext_unlock);
	(void)data;
	s->lock.ext_locked = false;
	wl_list_remove(&s->lock.ext_new_surface.link);
	wl_list_remove(&s->lock.ext_unlock.link);
	wl_list_remove(&s->lock.ext_destroy.link);
	s->lock.ext_lock = NULL;
	unlock(s);
}

static void ext_destroy(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, lock.ext_destroy);
	(void)data;
	/* the locker went away without unlocking (crashed?): stay locked with our own screen */
	wl_list_remove(&s->lock.ext_new_surface.link);
	wl_list_remove(&s->lock.ext_unlock.link);
	wl_list_remove(&s->lock.ext_destroy.link);
	s->lock.ext_lock = NULL;
	s->lock.ext_locked = false;
	s->lock.locked = true;
	lock_redraw(s);
}

static void new_lock(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_lock);
	struct wlr_session_lock_v1 *lk = data;
	if (s->lock.ext_lock) {
		wlr_session_lock_v1_destroy(lk);
		return;
	}
	s->lock.ext_lock = lk;
	s->lock.ext_locked = true;
	s->lock.locked = false;
	lock_redraw(s);           /* hides ours if it was up */
	wlr_seat_keyboard_notify_clear_focus(s->seat);
	s->lock.ext_new_surface.notify = ext_new_surface;
	wl_signal_add(&lk->events.new_surface, &s->lock.ext_new_surface);
	s->lock.ext_unlock.notify = ext_unlock;
	wl_signal_add(&lk->events.unlock, &s->lock.ext_unlock);
	s->lock.ext_destroy.notify = ext_destroy;
	wl_signal_add(&lk->events.destroy, &s->lock.ext_destroy);
	wlr_session_lock_v1_send_locked(lk);
	ipc_event(s, "locked", NULL);
}

void lock_init(struct chrono_server *s)
{
	s->lock_manager = wlr_session_lock_manager_v1_create(s->display);
	s->new_lock.notify = new_lock;
	wl_signal_add(&s->lock_manager->events.new_lock, &s->new_lock);
	s->lock.tick = wl_event_loop_add_timer(s->loop, lock_tick, s);
	wl_event_source_timer_update(s->lock.tick, 20 * 1000);
}
