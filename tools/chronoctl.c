/* chronoctl: talk to a running Chrono desktop.
 *   chronoctl windows | outputs | workspaces | themes | theme [id] | reload | lock | quit
 *   chronoctl action "workspace 2"         chronoctl focus 12
 *   chronoctl output DP-1 mode=2560x1440@144 scale=1.25
 *   chronoctl set keyboard layout "us,de"  chronoctl subscribe
 * Everything after the program name is sent as one line; the reply is printed as-is (JSON). */
#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int connect_socket(void)
{
	char path[512];
	const char *env = getenv("CHRONO_SOCK");
	if (env && *env) {
		snprintf(path, sizeof path, "%s", env);
	} else {
		const char *rt = getenv("XDG_RUNTIME_DIR");
		const char *wd = getenv("WAYLAND_DISPLAY");
		snprintf(path, sizeof path, "%s/chrono-%s.sock", rt ? rt : "/tmp", wd ? wd : "wayland-0");
	}
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	if (strlen(path) >= sizeof a.sun_path) {
		fprintf(stderr, "chronoctl: socket path too long: %s\n", path);
		return -1;
	}
	memcpy(a.sun_path, path, strlen(path) + 1);
	if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof a) < 0) {
		fprintf(stderr, "chronoctl: can't reach Chrono at %s (%s). Is it running?\n", path, strerror(errno));
		return -1;
	}
	return fd;
}

int main(int argc, char **argv)
{
	if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		fprintf(stderr,
			"usage: chronoctl <command> [args]\n"
			"  windows | outputs | workspaces | themes | theme [id] | config | reload | lock | quit\n"
			"  focus|close|minimize|toggle|maximize|fullscreen <window id>\n"
			"  move-to-workspace <window id> <n>\n"
			"  action \"<keybind action>\"   e.g. action \"exec terminal\", action \"workspace 2\"\n"
			"  output <name> mode=WxH@Hz scale=N transform=normal|90|180|270 x=N y=N enabled=true|false\n"
			"  set <section> <key> <value>   writes ~/.config/chrono/chrono.conf and applies it\n"
			"  subscribe                     print events as they happen\n");
		return argc < 2 ? 2 : 0;
	}
	char line[8192] = "";
	size_t n = 0;
	for (int i = 1; i < argc; i++) {
		int w = snprintf(line + n, sizeof line - n, "%s%s", i > 1 ? " " : "", argv[i]);
		if (w < 0 || (size_t)w >= sizeof line - n) {
			fprintf(stderr, "chronoctl: command too long\n");
			return 2;
		}
		n += (size_t)w;
	}
	line[n++] = '\n';
	int fd = connect_socket();
	if (fd < 0)
		return 1;
	if (write(fd, line, n) != (ssize_t)n)
		return 1;
	bool streaming = !strcmp(argv[1], "subscribe");
	char buf[65536];
	int status = 0;
	for (;;) {
		ssize_t r = read(fd, buf, sizeof buf);
		if (r <= 0)
			break;
		fwrite(buf, 1, (size_t)r, stdout);
		fflush(stdout);
		if (!streaming && memchr(buf, '\n', (size_t)r)) {
			if (strstr(buf, "\"ok\":false"))
				status = 1;
			break;
		}
	}
	close(fd);
	return status;
}
