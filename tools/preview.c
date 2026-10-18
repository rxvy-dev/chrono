/* chrono-theme-preview: render a theme as a miniature desktop (used by the settings gallery and the docs).
 *   chrono-theme-preview <theme id or file> <out.png> [width height]
 *   chrono-theme-preview --all <out dir> [width height]
 *   chrono-theme-preview --wallpaper <theme> <out.png> <width> <height>
 */
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paint.h"
#include "theme.h"

static int W = 640, H = 400;

static int render(const struct chrono_theme *t, const char *out, int wallpaper_only)
{
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
	cairo_t *cr = cairo_create(s);
	if (wallpaper_only)
		chrono_paint_desktop(cr, t, W, H);
	else
		chrono_paint_preview(cr, t, W, H);
	cairo_destroy(cr);
	cairo_status_t st = cairo_surface_write_to_png(s, out);
	cairo_surface_destroy(s);
	if (st != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "chrono-theme-preview: %s: %s\n", out, cairo_status_to_string(st));
		return 1;
	}
	return 0;
}

static const char *outdir;
static int failures;

static void each(const char *path, const char *id, void *data)
{
	(void)data;
	struct chrono_theme t;
	if (!chrono_theme_load_file(&t, path)) {
		failures++;
		return;
	}
	char out[4096];
	snprintf(out, sizeof out, "%s/%s.png", outdir, id);
	failures += render(&t, out, 0);
	printf("%s\t%s\t%s\n", id, t.name, t.era);
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s <theme> <out.png> [w h] | --all <dir> [w h] | --wallpaper <theme> <out.png> w h\n",
			argv[0]);
		return 2;
	}
	int wp = 0;
	if (!strcmp(argv[1], "--wallpaper")) {
		wp = 1;
		argv++;
		argc--;
	}
	if (argc >= 5) {
		W = atoi(argv[3]);
		H = atoi(argv[4]);
		if (W < 16 || H < 16 || W > 16384 || H > 16384)
			return 2;
	}
	if (!strcmp(argv[1], "--all")) {
		outdir = argv[2];
		chrono_theme_list(each, NULL);
		return failures ? 1 : 0;
	}
	struct chrono_theme t;
	if (strchr(argv[1], '/') || strstr(argv[1], ".theme")) {
		if (!chrono_theme_load_file(&t, argv[1])) {
			fprintf(stderr, "can't read %s\n", argv[1]);
			return 1;
		}
	} else {
		chrono_theme_load(&t, argv[1]);
	}
	return render(&t, argv[2], wp);
}
