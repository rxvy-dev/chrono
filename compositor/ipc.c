/* IPC: a Unix socket for the shell, the settings app and `chronoctl`.
 * One text command per line in, one JSON line out:
 *   ping | windows | outputs | workspaces | theme [id] | themes | config | reload | lock | quit
 *   focus|close|minimize|unminimize|toggle|maximize|fullscreen <id>
 *   move-to-workspace <id> <n>
 *   action <any keybind action, e.g. "workspace 2" or "exec terminal">
 *   output <name> mode=WxH@Hz scale=1.5 transform=normal|90|... x=0 y=0 enabled=true adaptive_sync=false
 *   set <section> <key> <value...>      (writes chrono.conf and applies it)
 *   subscribe                            (the connection then receives {"event":...} lines)
 * The socket is $CHRONO_SOCK, i.e. $XDG_RUNTIME_DIR/chrono-<WAYLAND_DISPLAY>.sock */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "../common/ini.h"
#include <wlr/types/wlr_keyboard.h>

#include "server.h"

struct ipc_client {
	struct wl_list link;
	struct chrono_server *server;
	int fd;
	struct wl_event_source *src;
	char in[8192];
	size_t inlen;
	bool subscribed;
	bool inhibits_binds;
};

/* ------------------------------------------------------------------------------- JSON helpers */
void jbuf_append(char **buf, size_t *len, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *piece = NULL;
	int n = vasprintf(&piece, fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	*buf = realloc(*buf, *len + (size_t)n + 1);
	memcpy(*buf + *len, piece, (size_t)n + 1);
	*len += (size_t)n;
	free(piece);
}

void jbuf_string(char **buf, size_t *len, const char *s)
{
	jbuf_append(buf, len, "\"");
	for (const unsigned char *p = (const unsigned char *)s; p && *p; p++) {
		if (*p == '"' || *p == '\\')
			jbuf_append(buf, len, "\\%c", *p);
		else if (*p == '\n')
			jbuf_append(buf, len, "\\n");
		else if (*p == '\t')
			jbuf_append(buf, len, "\\t");
		else if (*p < 0x20)
			jbuf_append(buf, len, "\\u%04x", *p);
		else
			jbuf_append(buf, len, "%c", *p);
	}
	jbuf_append(buf, len, "\"");
}

/* ------------------------------------------------------------------------------- clients */
static void client_close(struct ipc_client *c)
{
	if (c->inhibits_binds)
		c->server->binds_inhibited = false;
	wl_event_source_remove(c->src);
	close(c->fd);
	wl_list_remove(&c->link);
	free(c);
}

static void send_line(struct ipc_client *c, const char *s, size_t n)
{
	/* replies are small; a client that stops reading gets dropped rather than stalling the desktop */
	while (n > 0) {
		ssize_t w = send(c->fd, s, n, MSG_NOSIGNAL | MSG_DONTWAIT);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return;
		}
		s += w;
		n -= (size_t)w;
	}
}

static void reply(struct ipc_client *c, char *json, size_t len)
{
	jbuf_append(&json, &len, "\n");
	send_line(c, json, len);
	free(json);
}

static void reply_ok(struct ipc_client *c)
{
	send_line(c, "{\"ok\":true}\n", 12);
}

static void reply_err(struct ipc_client *c, const char *msg)
{
	char *b = NULL;
	size_t n = 0;
	jbuf_append(&b, &n, "{\"ok\":false,\"error\":");
	jbuf_string(&b, &n, msg);
	jbuf_append(&b, &n, "}");
	reply(c, b, n);
}

void ipc_event(struct chrono_server *s, const char *event, const char *payload)
{
	if (!s->ipc_clients.next)
		return;
	char *b = NULL;
	size_t n = 0;
	jbuf_append(&b, &n, "{\"event\":\"%s\"%s%s}\n", event, payload ? ",\"data\":" : "", payload ? payload : "");
	struct ipc_client *c, *tmp;
	wl_list_for_each_safe(c, tmp, &s->ipc_clients, link)
		if (c->subscribed)
			send_line(c, b, n);
	free(b);
}

static void theme_list_cb(const char *path, const char *id, void *data)
{
	void **d = data;
	char **b = d[0];
	size_t *n = d[1];
	struct chrono_theme t;
	if (!chrono_theme_load_file(&t, path))
		return;
	jbuf_append(b, n, "%s{\"id\":", *n > 1 ? "," : "");
	jbuf_string(b, n, id);
	jbuf_append(b, n, ",\"name\":");
	jbuf_string(b, n, t.name);
	jbuf_append(b, n, ",\"era\":");
	jbuf_string(b, n, t.era);
	jbuf_append(b, n, ",\"description\":");
	jbuf_string(b, n, t.description);
	jbuf_append(b, n, ",\"path\":");
	jbuf_string(b, n, path);
	jbuf_append(b, n, ",\"variant\":");
	jbuf_string(b, n, t.variant);
	jbuf_append(b, n, ",\"pair\":");
	jbuf_string(b, n, t.pair);
	jbuf_append(b, n, "}");
}

static void handle(struct ipc_client *c, char *line)
{
	struct chrono_server *s = c->server;
	char cmd[64] = "";
	char rest[4096] = "";
	sscanf(line, "%63s %4095[^\n]", cmd, rest);
	char *b = NULL;
	size_t n = 0;

	if (!strcmp(cmd, "ping")) {
		jbuf_append(&b, &n, "{\"ok\":true,\"name\":\"chrono\",\"version\":\"%s\"}", CHRONO_VERSION);
		reply(c, b, n);
	} else if (!strcmp(cmd, "windows")) {
		view_write_json(s, &b, &n);
		reply(c, b, n);
	} else if (!strcmp(cmd, "outputs")) {
		output_write_json(s, &b, &n);
		reply(c, b, n);
	} else if (!strcmp(cmd, "workspaces")) {
		jbuf_append(&b, &n, "{\"current\":%d,\"count\":%d}", s->workspace + 1, s->config.workspaces);
		reply(c, b, n);
	} else if (!strcmp(cmd, "themes")) {
		jbuf_append(&b, &n, "[");
		void *d[2] = {&b, &n};
		chrono_theme_list(theme_list_cb, d);
		jbuf_append(&b, &n, "]");
		reply(c, b, n);
	} else if (!strcmp(cmd, "theme")) {
		if (rest[0]) {
			server_apply_theme(s, rest);
			reply_ok(c);
		} else {
			jbuf_append(&b, &n, "{\"id\":");
			jbuf_string(&b, &n, s->theme.id);
			jbuf_append(&b, &n, ",\"name\":");
			jbuf_string(&b, &n, s->theme.name);
			jbuf_append(&b, &n, ",\"path\":");
			jbuf_string(&b, &n, s->theme.path);
			jbuf_append(&b, &n, ",\"selected\":");
			jbuf_string(&b, &n, s->config.theme);
			jbuf_append(&b, &n, "}");
			reply(c, b, n);
		}
	} else if (!strcmp(cmd, "config")) {
		jbuf_append(&b, &n, "{\"path\":");
		jbuf_string(&b, &n, s->config.path);
		jbuf_append(&b, &n, ",\"outputs_path\":");
		jbuf_string(&b, &n, s->config.outputs_path);
		jbuf_append(&b, &n, ",\"terminal\":");
		jbuf_string(&b, &n, s->config.terminal);
		jbuf_append(&b, &n, ",\"workspaces\":%d}", s->config.workspaces);
		reply(c, b, n);
	} else if (!strcmp(cmd, "inhibit-binds")) {
		/* "inhibit-binds on|off": while on (and while this client stays connected) shortcuts reach apps */
		c->inhibits_binds = strcmp(rest, "off") != 0;
		s->binds_inhibited = c->inhibits_binds;
		reply_ok(c);
	} else if (!strcmp(cmd, "keybinds")) {
		jbuf_append(&b, &n, "[");
		for (int i = 0; i < s->config.nbinds; i++) {
			const struct chrono_bind *kb = &s->config.binds[i];
			char combo[160] = "", key[64] = "";
			if (kb->mods & WLR_MODIFIER_CTRL) strcat(combo, "Ctrl+");
			if (kb->mods & WLR_MODIFIER_ALT) strcat(combo, "Alt+");
			if (kb->mods & WLR_MODIFIER_SHIFT) strcat(combo, "Shift+");
			if (kb->mods & WLR_MODIFIER_LOGO) strcat(combo, "Super+");
			if (kb->mods & WLR_MODIFIER_MOD5) strcat(combo, "AltGr+");
			xkb_keysym_get_name(kb->sym, key, sizeof key);
			if (strlen(key) == 1) key[0] = (char)toupper((unsigned char)key[0]);
			strncat(combo, key, sizeof combo - strlen(combo) - 1);
			jbuf_append(&b, &n, "%s{\"keys\":", i ? "," : "");
			jbuf_string(&b, &n, combo);
			jbuf_append(&b, &n, ",\"action\":");
			jbuf_string(&b, &n, kb->action);
			jbuf_append(&b, &n, "}");
		}
		jbuf_append(&b, &n, "]");
		reply(c, b, n);
	} else if (!strcmp(cmd, "reload")) {
		server_reload(s);
		reply_ok(c);
	} else if (!strcmp(cmd, "lock")) {
		lock_screen(s);
		reply_ok(c);
	} else if (!strcmp(cmd, "quit")) {
		reply_ok(c);
		server_quit(s);
	} else if (!strcmp(cmd, "subscribe")) {
		c->subscribed = true;
		reply_ok(c);
	} else if (!strcmp(cmd, "action")) {
		if (run_action(s, rest))
			reply_ok(c);
		else
			reply_err(c, "unknown action");
	} else if (!strcmp(cmd, "output")) {
		char name[64] = "", settings[4000] = "";
		sscanf(rest, "%63s %3999[^\n]", name, settings);
		char err[200] = "";
		if (output_apply_settings(s, name, settings, err, sizeof err))
			reply_ok(c);
		else
			reply_err(c, err);
	} else if (!strcmp(cmd, "set")) {
		char sec[64] = "", key[64] = "", val[2048] = "";
		if (sscanf(rest, "%63s %63s %2047[^\n]", sec, key, val) < 2) {
			reply_err(c, "usage: set <section> <key> <value>");
			return;
		}
		if (!chrono_ini_set_in_file(s->config.path, sec, key, val)) {
			reply_err(c, "can't write the config file");
			return;
		}
		server_reload(s);
		reply_ok(c);
	} else {
		/* window commands: <verb> <id> [arg] */
		unsigned id = 0;
		int arg = 0;
		int got = sscanf(rest, "%u %d", &id, &arg);
		struct chrono_view *v = got >= 1 ? view_by_id(s, id) : NULL;
		if (!v) {
			reply_err(c, got >= 1 ? "no such window" : "unknown command");
			return;
		}
		if (!strcmp(cmd, "focus") || !strcmp(cmd, "unminimize"))
			view_focus(v);
		else if (!strcmp(cmd, "close"))
			view_close(v);
		else if (!strcmp(cmd, "minimize"))
			view_set_minimized(v, true);
		else if (!strcmp(cmd, "toggle")) {
			/* what a taskbar button does */
			if (s->focused == v && !v->minimized)
				view_set_minimized(v, true);
			else
				view_focus(v);
		} else if (!strcmp(cmd, "maximize"))
			view_set_maximized(v, !v->maximized);
		else if (!strcmp(cmd, "fullscreen"))
			view_set_fullscreen(v, !v->fullscreen);
		else if (!strcmp(cmd, "move-to-workspace") && got == 2)
			view_to_workspace(v, arg - 1);
		else {
			reply_err(c, "unknown command");
			return;
		}
		reply_ok(c);
	}
}

static int client_readable(int fd, uint32_t mask, void *data)
{
	struct ipc_client *c = data;
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		client_close(c);
		return 0;
	}
	ssize_t r = read(fd, c->in + c->inlen, sizeof c->in - c->inlen - 1);
	if (r <= 0) {
		if (r < 0 && (errno == EAGAIN || errno == EINTR))
			return 0;
		client_close(c);
		return 0;
	}
	c->inlen += (size_t)r;
	c->in[c->inlen] = '\0';
	char *nl;
	while ((nl = memchr(c->in, '\n', c->inlen))) {
		*nl = '\0';
		char line[8192];
		snprintf(line, sizeof line, "%s", c->in);
		size_t used = (size_t)(nl - c->in) + 1;
		memmove(c->in, nl + 1, c->inlen - used);
		c->inlen -= used;
		c->in[c->inlen] = '\0';
		if (line[0])
			handle(c, line);
	}
	if (c->inlen >= sizeof c->in - 1) {
		reply_err(c, "line too long");
		client_close(c);
	}
	return 0;
}

static int accept_client(int fd, uint32_t mask, void *data)
{
	struct chrono_server *s = data;
	(void)mask;
	int cfd = accept4(fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
	if (cfd < 0)
		return 0;
	struct ipc_client *c = calloc(1, sizeof *c);
	c->server = s;
	c->fd = cfd;
	c->src = wl_event_loop_add_fd(s->loop, cfd, WL_EVENT_READABLE, client_readable, c);
	wl_list_insert(&s->ipc_clients, &c->link);
	return 0;
}

void ipc_init(struct chrono_server *s)
{
	wl_list_init(&s->ipc_clients);
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!rt)
		rt = "/tmp";
	snprintf(s->ipc_path, sizeof s->ipc_path, "%s/chrono-%s.sock", rt, s->socket_name);
	unlink(s->ipc_path);
	s->ipc_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	struct sockaddr_un addr = {.sun_family = AF_UNIX};
	snprintf(addr.sun_path, sizeof addr.sun_path, "%s", s->ipc_path);
	if (bind(s->ipc_fd, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(s->ipc_fd, 16) < 0) {
		wlr_log(WLR_ERROR, "IPC socket %s: %s", s->ipc_path, strerror(errno));
		close(s->ipc_fd);
		s->ipc_fd = -1;
		return;
	}
	chmod(s->ipc_path, 0600);
	setenv("CHRONO_SOCK", s->ipc_path, 1);
	s->ipc_source = wl_event_loop_add_fd(s->loop, s->ipc_fd, WL_EVENT_READABLE, accept_client, s);
}

void ipc_finish(struct chrono_server *s)
{
	struct ipc_client *c, *tmp;
	wl_list_for_each_safe(c, tmp, &s->ipc_clients, link)
		client_close(c);
	if (s->ipc_source)
		wl_event_source_remove(s->ipc_source);
	if (s->ipc_fd >= 0) {
		close(s->ipc_fd);
		unlink(s->ipc_path);
	}
}
