/* Hot reload: watch the theme folders, and when a .theme file is added or edited, re-read the
 * current theme (if that's the one that changed) and tell the shell / settings to refresh. */
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "server.h"

static int ino_fd = -1;
static struct wl_event_source *ino_src, *debounce;
static bool current_changed;

static int fire(void *data)
{
	struct chrono_server *s = data;
	if (current_changed) {
		wlr_log(WLR_INFO, "theme file changed, reloading '%s'", s->config.theme);
		server_refresh_theme(s);
	}
	current_changed = false;
	ipc_event(s, "themes", NULL);
	return 0;
}

static int readable(int fd, uint32_t mask, void *data)
{
	struct chrono_server *s = data;
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	ssize_t n;
	bool any = false;
	while ((n = read(fd, buf, sizeof buf)) > 0) {
		for (char *p = buf; p < buf + n;) {
			struct inotify_event *e = (struct inotify_event *)p;
			if (e->len && strstr(e->name, ".theme")) {
				any = true;
				size_t idl = strlen(s->theme.id);
				if (!strncmp(e->name, s->theme.id, idl) && !strcmp(e->name + idl, ".theme"))
					current_changed = true;
				idl = strlen(s->config.theme);
				if (!strncmp(e->name, s->config.theme, idl) && !strcmp(e->name + idl, ".theme"))
					current_changed = true;
			}
			p += sizeof *e + e->len;
		}
	}
	if (any)
		wl_event_source_timer_update(debounce, 250); /* editors write in several steps */
	return 0;
}

void themewatch_init(struct chrono_server *s)
{
	ino_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (ino_fd < 0) {
		wlr_log(WLR_ERROR, "inotify: %s (theme hot reload is off)", strerror(errno));
		return;
	}
	char dirs[8][4096];
	int n = chrono_theme_dirs(dirs, 8);
	for (int i = 0; i < n; i++) {
		if (i == 1 || (i == 0 && !getenv("CHRONO_THEME_DIR")))
			mkdir(dirs[i], 0755); /* make sure the user's folder exists so it can be watched */
		inotify_add_watch(ino_fd, dirs[i], IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_DELETE | IN_MOVED_FROM);
	}
	ino_src = wl_event_loop_add_fd(s->loop, ino_fd, WL_EVENT_READABLE, readable, s);
	debounce = wl_event_loop_add_timer(s->loop, fire, s);
}

void themewatch_finish(void)
{
	if (ino_src)
		wl_event_source_remove(ino_src);
	if (debounce)
		wl_event_source_remove(debounce);
	if (ino_fd >= 0)
		close(ino_fd);
	ino_src = debounce = NULL;
	ino_fd = -1;
}
