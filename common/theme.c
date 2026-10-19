#define _GNU_SOURCE
#include "theme.h"
#include "ini.h"

#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static int hexv(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

bool chrono_parse_color(const char *s, struct chrono_rgba *out)
{
	if (!s)
		return false;
	while (*s == ' ')
		s++;
	if (*s == '#')
		s++;
	size_t n = strspn(s, "0123456789abcdefABCDEF");
	int v[8];
	if (n != 6 && n != 8 && n != 3)
		return false;
	if (n == 3) {
		for (int i = 0; i < 3; i++)
			v[i] = hexv(s[i]) * 17;
		*out = (struct chrono_rgba){v[0] / 255.0, v[1] / 255.0, v[2] / 255.0, 1.0};
		return true;
	}
	for (size_t i = 0; i < n / 2; i++)
		v[i] = hexv(s[2 * i]) * 16 + hexv(s[2 * i + 1]);
	*out = (struct chrono_rgba){v[0] / 255.0, v[1] / 255.0, v[2] / 255.0, n == 8 ? v[3] / 255.0 : 1.0};
	return true;
}

static void col(const struct chrono_ini *ini, const char *key, struct chrono_rgba *dst)
{
	struct chrono_rgba c;
	if (chrono_parse_color(chrono_ini_get(ini, key, NULL), &c))
		*dst = c;
}

static void str(const struct chrono_ini *ini, const char *key, char *dst, size_t n)
{
	const char *v = chrono_ini_get(ini, key, NULL);
	if (v)
		snprintf(dst, n, "%s", v);
}

static enum chrono_button parse_button(const char *s, size_t n)
{
	if (n == 5 && !strncasecmp(s, "close", 5)) return BTN_CLOSE;
	if (n == 8 && !strncasecmp(s, "maximize", 8)) return BTN_MAXIMIZE;
	if (n == 3 && !strncasecmp(s, "max", 3)) return BTN_MAXIMIZE;
	if (n == 8 && !strncasecmp(s, "minimize", 8)) return BTN_MINIMIZE;
	if (n == 3 && !strncasecmp(s, "min", 3)) return BTN_MINIMIZE;
	if (n == 4 && !strncasecmp(s, "menu", 4)) return BTN_MENU;
	return BTN_NONE;
}

/* "menu:minimize,maximize,close" — left of the colon on the left */
static void parse_layout(const char *s, struct chrono_theme *t)
{
	memset(t->buttons_left, 0, sizeof t->buttons_left);
	memset(t->buttons_right, 0, sizeof t->buttons_right);
	const char *colon = strchr(s, ':');
	enum chrono_button *side = colon ? t->buttons_left : t->buttons_right;
	int k = 0;
	const char *p = s;
	while (*p) {
		if (*p == ':') {
			side = t->buttons_right;
			k = 0;
			p++;
			continue;
		}
		if (*p == ',' || *p == ' ') {
			p++;
			continue;
		}
		size_t n = strcspn(p, ",: ");
		enum chrono_button b = parse_button(p, n);
		if (b && k < CHRONO_MAX_BUTTONS)
			side[k++] = b;
		p += n;
	}
}

static const struct { const char *name; enum chrono_decor_style s; } styles[] = {
	{"flat", DECOR_FLAT}, {"bevel", DECOR_BEVEL}, {"platinum", DECOR_PLATINUM}, {"pixel", DECOR_PIXEL},
	{"tab", DECOR_TAB}, {"motif", DECOR_MOTIF}, {"cube", DECOR_CUBE}, {"meadow", DECOR_MEADOW},
	{"pinstripe", DECOR_PINSTRIPE}, {"glass", DECOR_GLASS}, {"neon", DECOR_NEON},
};

static const struct { const char *name; enum chrono_button_shape s; } shapes[] = {
	{"flat", BSHAPE_FLAT}, {"bevel", BSHAPE_BEVEL}, {"circle", BSHAPE_CIRCLE}, {"gel", BSHAPE_GEL},
	{"square", BSHAPE_SQUARE}, {"roundrect", BSHAPE_ROUND_RECT},
};

void chrono_theme_defaults(struct chrono_theme *t)
{
	memset(t, 0, sizeof *t);
	snprintf(t->id, sizeof t->id, "midnight");
	snprintf(t->name, sizeof t->name, "Midnight");
	snprintf(t->era, sizeof t->era, "modern");
	chrono_parse_color("#1e2128", &t->bg);
	chrono_parse_color("#e6e8ee", &t->fg);
	chrono_parse_color("#5b9cff", &t->accent);
	chrono_parse_color("#ffffff", &t->accent_fg);
	chrono_parse_color("#5b9cff66", &t->selection);
	t->window_bg = t->bg;
	t->window_fg = t->fg;
	chrono_parse_color("#2a2e37", &t->title_active);
	t->title_active2 = t->title_active;
	t->title_active_fg = t->fg;
	chrono_parse_color("#23262d", &t->title_inactive);
	t->title_inactive2 = t->title_inactive;
	chrono_parse_color("#8a8f9c", &t->title_inactive_fg);
	chrono_parse_color("#3a3f4b", &t->border_active);
	chrono_parse_color("#2c3038", &t->border_inactive);
	chrono_parse_color("#ffffff", &t->bevel_light);
	chrono_parse_color("#808080", &t->bevel_dark);
	chrono_parse_color("#000000", &t->bevel_shadow);
	chrono_parse_color("#3a3f4b", &t->button_bg);
	t->button_fg = t->fg;
	chrono_parse_color("#e5484d", &t->close_bg);
	chrono_parse_color("#181a20e6", &t->panel_bg);
	t->panel_fg = t->fg;
	chrono_parse_color("#ffffff14", &t->panel_border);
	t->decor = DECOR_FLAT;
	t->title_height = 30;
	t->border_width = 1;
	t->corner_radius = 10;
	t->title_center = true;
	t->title_bold = true;
	parse_layout("menu:minimize,maximize,close", t);
	t->button_shape = BSHAPE_CIRCLE;
	t->button_size = 14;
	t->shadow = true;
	t->shadow_size = 18;
	t->shadow_alpha = 0.45;
	snprintf(t->font_ui, sizeof t->font_ui, "Inter, Noto Sans, DejaVu Sans 10");
	snprintf(t->font_title, sizeof t->font_title, "Inter, Noto Sans, DejaVu Sans Bold 10");
	snprintf(t->font_mono, sizeof t->font_mono, "JetBrains Mono, DejaVu Sans Mono 10");
	snprintf(t->panel_position, sizeof t->panel_position, "bottom");
	t->panel_height = 40;
	snprintf(t->panel_style, sizeof t->panel_style, "flat");
	t->panel_dock = false;
	snprintf(t->wallpaper, sizeof t->wallpaper, "gradient");
	chrono_parse_color("#141821", &t->desktop1);
	chrono_parse_color("#2b3550", &t->desktop2);
	snprintf(t->cursor_theme, sizeof t->cursor_theme, "Adwaita");
	t->cursor_size = 24;
	snprintf(t->icon_theme, sizeof t->icon_theme, "Adwaita");
}

bool chrono_theme_load_file(struct chrono_theme *t, const char *path)
{
	struct chrono_ini ini = {0};
	chrono_theme_defaults(t);
	if (!chrono_ini_load(&ini, path))
		return false;
	snprintf(t->path, sizeof t->path, "%s", path);
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	snprintf(t->id, sizeof t->id, "%.*s", (int)strcspn(base, "."), base);

	str(&ini, "theme.name", t->name, sizeof t->name);
	str(&ini, "theme.era", t->era, sizeof t->era);
	str(&ini, "theme.description", t->description, sizeof t->description);
	str(&ini, "theme.variant", t->variant, sizeof t->variant);
	str(&ini, "theme.pair", t->pair, sizeof t->pair);

	col(&ini, "colors.bg", &t->bg);
	col(&ini, "colors.fg", &t->fg);
	col(&ini, "colors.accent", &t->accent);
	col(&ini, "colors.accent_fg", &t->accent_fg);
	col(&ini, "colors.selection", &t->selection);
	t->window_bg = t->bg;
	t->window_fg = t->fg;
	col(&ini, "colors.window_bg", &t->window_bg);
	col(&ini, "colors.window_fg", &t->window_fg);
	col(&ini, "colors.title_active", &t->title_active);
	t->title_active2 = t->title_active;
	col(&ini, "colors.title_active2", &t->title_active2);
	col(&ini, "colors.title_active_fg", &t->title_active_fg);
	col(&ini, "colors.title_inactive", &t->title_inactive);
	t->title_inactive2 = t->title_inactive;
	col(&ini, "colors.title_inactive2", &t->title_inactive2);
	col(&ini, "colors.title_inactive_fg", &t->title_inactive_fg);
	col(&ini, "colors.border_active", &t->border_active);
	col(&ini, "colors.border_inactive", &t->border_inactive);
	col(&ini, "colors.bevel_light", &t->bevel_light);
	col(&ini, "colors.bevel_dark", &t->bevel_dark);
	col(&ini, "colors.bevel_shadow", &t->bevel_shadow);
	col(&ini, "colors.button_bg", &t->button_bg);
	col(&ini, "colors.button_fg", &t->button_fg);
	col(&ini, "colors.close_bg", &t->close_bg);
	col(&ini, "colors.panel_bg", &t->panel_bg);
	col(&ini, "colors.panel_fg", &t->panel_fg);
	col(&ini, "colors.panel_border", &t->panel_border);

	const char *st = chrono_ini_get(&ini, "decor.style", NULL);
	for (size_t i = 0; st && i < sizeof styles / sizeof *styles; i++)
		if (!strcasecmp(st, styles[i].name))
			t->decor = styles[i].s;
	t->title_height = (int)chrono_ini_num(&ini, "decor.title_height", t->title_height);
	t->border_width = (int)chrono_ini_num(&ini, "decor.border_width", t->border_width);
	t->corner_radius = (int)chrono_ini_num(&ini, "decor.corner_radius", t->corner_radius);
	t->title_center = chrono_ini_bool(&ini, "decor.title_center", t->title_center);
	t->title_bold = chrono_ini_bool(&ini, "decor.title_bold", t->title_bold);
	const char *lay = chrono_ini_get(&ini, "decor.buttons", NULL);
	if (lay)
		parse_layout(lay, t);
	const char *bs = chrono_ini_get(&ini, "decor.button_shape", NULL);
	for (size_t i = 0; bs && i < sizeof shapes / sizeof *shapes; i++)
		if (!strcasecmp(bs, shapes[i].name))
			t->button_shape = shapes[i].s;
	t->button_size = (int)chrono_ini_num(&ini, "decor.button_size", t->button_size);
	t->shadow = chrono_ini_bool(&ini, "decor.shadow", t->shadow);
	t->shadow_size = (int)chrono_ini_num(&ini, "decor.shadow_size", t->shadow_size);
	t->shadow_alpha = chrono_ini_num(&ini, "decor.shadow_alpha", t->shadow_alpha);

	str(&ini, "fonts.ui", t->font_ui, sizeof t->font_ui);
	str(&ini, "fonts.title", t->font_title, sizeof t->font_title);
	str(&ini, "fonts.mono", t->font_mono, sizeof t->font_mono);

	str(&ini, "panel.position", t->panel_position, sizeof t->panel_position);
	t->panel_height = (int)chrono_ini_num(&ini, "panel.height", t->panel_height);
	str(&ini, "panel.style", t->panel_style, sizeof t->panel_style);
	str(&ini, "panel.launcher_label", t->launcher_label, sizeof t->launcher_label);
	t->panel_dock = chrono_ini_bool(&ini, "panel.dock", t->panel_dock);

	str(&ini, "desktop.wallpaper", t->wallpaper, sizeof t->wallpaper);
	col(&ini, "desktop.color1", &t->desktop1);
	t->desktop2 = t->desktop1;
	col(&ini, "desktop.color2", &t->desktop2);

	str(&ini, "cursor.theme", t->cursor_theme, sizeof t->cursor_theme);
	t->cursor_size = (int)chrono_ini_num(&ini, "cursor.size", t->cursor_size);
	str(&ini, "icons.theme", t->icon_theme, sizeof t->icon_theme);

	if (!t->variant[0])
		snprintf(t->variant, sizeof t->variant, "%s", chrono_luminance(t->window_bg) < 0.35 ? "dark" : "light");

	/* sanity */
	if (t->title_height < 0) t->title_height = 0;
	if (t->title_height > 80) t->title_height = 80;
	if (t->border_width < 0) t->border_width = 0;
	if (t->border_width > 16) t->border_width = 16;
	if (t->corner_radius < 0) t->corner_radius = 0;
	if (t->corner_radius > 30) t->corner_radius = 30;
	if (t->panel_height < 20) t->panel_height = 20;
	if (t->panel_height > 96) t->panel_height = 96;
	if (t->shadow_size > 64) t->shadow_size = 64;

	chrono_ini_free(&ini);
	return true;
}

int chrono_theme_dirs(char dirs[][4096], int max)
{
	int n = 0;
	const char *env = getenv("CHRONO_THEME_DIR");
	const char *home = getenv("HOME");
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *data = getenv("XDG_DATA_HOME");
#define ADD(...) do { if (n < max) snprintf(dirs[n++], 4096, __VA_ARGS__); } while (0)
	if (env && *env)
		ADD("%s", env);
	/* your own themes: ~/.config/chrono/themes (and the older ~/.local/share location) */
	if (cfg && *cfg)
		ADD("%s/chrono/themes", cfg);
	else if (home)
		ADD("%s/.config/chrono/themes", home);
	if (data && *data)
		ADD("%s/chrono/themes", data);
	else if (home)
		ADD("%s/.local/share/chrono/themes", home);
	ADD("/usr/local/share/chrono/themes");
	ADD("/usr/share/chrono/themes");
	ADD("themes");
#undef ADD
	return n;
}

static void theme_dirs(char dirs[][PATH_MAX], int *n)
{
	*n = chrono_theme_dirs(dirs, 8);
}

bool chrono_theme_load(struct chrono_theme *t, const char *id)
{
	char dirs[8][PATH_MAX];
	int n;
	theme_dirs(dirs, &n);
	for (int i = 0; i < n; i++) {
		char p[PATH_MAX * 2];
		if (snprintf(p, sizeof p, "%s/%s.theme", dirs[i], id) >= (int)sizeof p)
			continue;
		if (access(p, R_OK) == 0 && chrono_theme_load_file(t, p))
			return true;
	}
	chrono_theme_defaults(t);
	return false;
}

void chrono_theme_list(void (*cb)(const char *path, const char *id, void *data), void *data)
{
	char dirs[8][PATH_MAX];
	int n;
	theme_dirs(dirs, &n);
	char seen[256][64];
	int nseen = 0;
	for (int i = 0; i < n; i++) {
		DIR *d = opendir(dirs[i]);
		if (!d)
			continue;
		struct dirent *e;
		while ((e = readdir(d))) {
			size_t len = strlen(e->d_name);
			if (len < 7 || strcmp(e->d_name + len - 6, ".theme"))
				continue;
			char id[64];
			snprintf(id, sizeof id, "%.*s", (int)(len - 6), e->d_name);
			bool dup = false;
			for (int k = 0; k < nseen; k++)
				if (!strcmp(seen[k], id))
					dup = true;
			if (dup || nseen >= 256)
				continue;
			snprintf(seen[nseen++], 64, "%s", id);
			char p[PATH_MAX * 2];
			if (snprintf(p, sizeof p, "%s/%s", dirs[i], e->d_name) >= (int)sizeof p)
				continue;
			cb(p, id, data);
		}
		closedir(d);
	}
}

/* ------------------------------------------------------------------ colour helpers */
static double lin(double c)
{
	return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

double chrono_luminance(struct chrono_rgba c)
{
	return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

double chrono_contrast(struct chrono_rgba a, struct chrono_rgba b)
{
	double la = chrono_luminance(a), lb = chrono_luminance(b);
	if (la < lb) {
		double x = la;
		la = lb;
		lb = x;
	}
	return (la + 0.05) / (lb + 0.05);
}

static void rgb_to_hsl(struct chrono_rgba c, double *h, double *s, double *l)
{
	double mx = fmax(c.r, fmax(c.g, c.b)), mn = fmin(c.r, fmin(c.g, c.b));
	*l = (mx + mn) / 2;
	if (mx == mn) {
		*h = *s = 0;
		return;
	}
	double d = mx - mn;
	*s = *l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
	if (mx == c.r) *h = (c.g - c.b) / d + (c.g < c.b ? 6 : 0);
	else if (mx == c.g) *h = (c.b - c.r) / d + 2;
	else *h = (c.r - c.g) / d + 4;
	*h /= 6;
}

static double hue2(double p, double q, double t)
{
	if (t < 0) t += 1;
	if (t > 1) t -= 1;
	if (t < 1.0 / 6) return p + (q - p) * 6 * t;
	if (t < 0.5) return q;
	if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
	return p;
}

static struct chrono_rgba hsl(double h, double s, double l, double a)
{
	if (s == 0)
		return (struct chrono_rgba){l, l, l, a};
	double q = l < 0.5 ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
	return (struct chrono_rgba){hue2(p, q, h + 1.0 / 3), hue2(p, q, h), hue2(p, q, h - 1.0 / 3), a};
}

/* readable text colour on top of c */
static struct chrono_rgba text_on(struct chrono_rgba c)
{
	struct chrono_rgba w = {1, 1, 1, 1}, k = {0.08, 0.08, 0.1, 1};
	return chrono_contrast(c, w) >= chrono_contrast(c, k) ? w : k;
}

void chrono_theme_apply_accent(struct chrono_theme *t, const char *accent)
{
	if (!accent || !*accent || !strcasecmp(accent, "theme"))
		return;
	struct chrono_rgba a;
	bool dark = !strcmp(t->variant, "dark");
	if (!strcasecmp(accent, "wallpaper")) {
		/* Material-You style: take the more colourful wallpaper colour and make it a usable accent */
		double h1, s1, l1, h2, s2, l2;
		rgb_to_hsl(t->desktop1, &h1, &s1, &l1);
		rgb_to_hsl(t->desktop2, &h2, &s2, &l2);
		double h = s2 > s1 ? h2 : h1, sat = fmax(fmax(s1, s2), 0.45);
		a = hsl(h, fmin(sat, 0.85), dark ? 0.68 : 0.42, 1);
	} else if (!chrono_parse_color(accent, &a)) {
		return;
	}
	struct chrono_rgba old = t->accent;
	t->accent = a;
	t->accent_fg = text_on(a);
	t->selection = (struct chrono_rgba){a.r, a.g, a.b, 0.4};
	/* modern frames that used the old accent for the focused border follow the new one */
	if (fabs(t->border_active.r - old.r) + fabs(t->border_active.g - old.g) + fabs(t->border_active.b - old.b) < 0.02)
		t->border_active = (struct chrono_rgba){a.r, a.g, a.b, t->border_active.a};
}

bool chrono_theme_load_prefs(struct chrono_theme *t, const char *id, const char *mode, const char *accent)
{
	bool ok = chrono_theme_load(t, id);
	if (ok && mode && (!strcmp(mode, "light") || !strcmp(mode, "dark")) && strcmp(t->variant, mode) && t->pair[0]) {
		char pair[64];
		snprintf(pair, sizeof pair, "%s", t->pair);
		struct chrono_theme other;
		if (chrono_theme_load(&other, pair) && !strcmp(other.variant, mode))
			*t = other;
	}
	chrono_theme_apply_accent(t, accent);
	return ok;
}
