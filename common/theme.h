/* Chrono themes: one INI file describes the whole desktop — window decorations, panel, colours,
 * fonts, wallpaper, cursor and icons. Shared by the compositor (C) and the Qt shell / settings app. */
#ifndef CHRONO_THEME_H
#define CHRONO_THEME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct chrono_rgba {
	double r, g, b, a;
};

enum chrono_decor_style {
	DECOR_FLAT,       /* modern flat title bar */
	DECOR_BEVEL,      /* 3D bevelled frame, gradient title (90s PCs) */
	DECOR_PLATINUM,   /* grey title with horizontal ridges, centred title (late-90s Macs) */
	DECOR_PIXEL,      /* hard-edged 1-bit-ish frame with gadgets (Amiga-era) */
	DECOR_TAB,        /* title is a tab only as wide as the title (BeOS-era) */
	DECOR_MOTIF,      /* chunky Motif / CDE bevels in the palette colour */
	DECOR_CUBE,       /* black title, grey bevel frame (NeXT-era) */
	DECOR_MEADOW,     /* glossy rounded blue title (2000s) */
	DECOR_PINSTRIPE,  /* pinstriped title with gel "traffic light" buttons (early 2000s) */
	DECOR_GLASS,      /* translucent title with a lit edge (modern) */
	DECOR_NEON,       /* dark title with a glowing accent line (synthwave) */
};

enum chrono_button { BTN_NONE = 0, BTN_CLOSE, BTN_MAXIMIZE, BTN_MINIMIZE, BTN_MENU };
enum chrono_button_shape { BSHAPE_FLAT, BSHAPE_BEVEL, BSHAPE_CIRCLE, BSHAPE_GEL, BSHAPE_SQUARE, BSHAPE_ROUND_RECT };

#define CHRONO_MAX_BUTTONS 4

struct chrono_theme {
	char id[64];
	char name[64];
	char era[16];            /* "retro" | "modern" */
	char description[256];
	char path[1024];
	char variant[8];         /* "dark" | "light" (worked out from the background if not set) */
	char pair[64];           /* id of the same theme in the other variant, for the light/dark switch */

	/* palette */
	struct chrono_rgba bg, fg, accent, accent_fg, selection;
	struct chrono_rgba window_bg, window_fg;
	struct chrono_rgba title_active, title_active2, title_active_fg;
	struct chrono_rgba title_inactive, title_inactive2, title_inactive_fg;
	struct chrono_rgba border_active, border_inactive;
	struct chrono_rgba bevel_light, bevel_dark, bevel_shadow;
	struct chrono_rgba button_bg, button_fg, close_bg;
	struct chrono_rgba panel_bg, panel_fg, panel_border;

	/* window decorations */
	enum chrono_decor_style decor;
	int title_height;
	int border_width;
	int corner_radius;
	bool title_center;
	bool title_bold;
	enum chrono_button buttons_left[CHRONO_MAX_BUTTONS];
	enum chrono_button buttons_right[CHRONO_MAX_BUTTONS];
	enum chrono_button_shape button_shape;
	int button_size;
	bool shadow;
	int shadow_size;
	double shadow_alpha;

	/* fonts (Pango font descriptions, e.g. "Inter, DejaVu Sans 10") */
	char font_ui[128];
	char font_title[128];
	char font_mono[128];

	/* panel */
	char panel_position[16];  /* top | bottom */
	int panel_height;
	char panel_style[16];     /* bevel | flat | floating | glass | pixel */
	char launcher_label[32];  /* text on the launcher button, "" = icon only */
	bool panel_dock;          /* show the separate dock (modern themes) */

	/* desktop */
	char wallpaper[512];      /* solid | gradient | pattern:<name> | /path/to/image */
	struct chrono_rgba desktop1, desktop2;

	/* cursor + icons */
	char cursor_theme[64];
	int cursor_size;
	char icon_theme[64];
};

/* Directories searched for <id>.theme, in order: $CHRONO_THEME_DIR, ~/.local/share/chrono/themes,
 * /usr/local/share/chrono/themes, /usr/share/chrono/themes, ./themes */
bool chrono_theme_load_file(struct chrono_theme *t, const char *path);
bool chrono_theme_load(struct chrono_theme *t, const char *id);
void chrono_theme_defaults(struct chrono_theme *t);

/* Lists themes: calls cb(path, id) for every *.theme found (first occurrence of an id wins). */
void chrono_theme_list(void (*cb)(const char *path, const char *id, void *data), void *data);

bool chrono_parse_color(const char *s, struct chrono_rgba *out);

/* Load a theme the way the user's preferences ask for it:
 *   mode   "theme" (or NULL) | "light" | "dark" — switches to the theme's pair when the variant differs
 *   accent NULL/"" /"theme" | "#rrggbb" | "wallpaper" (derived from the theme's wallpaper colours) */
bool chrono_theme_load_prefs(struct chrono_theme *t, const char *id, const char *mode, const char *accent,
			     const char *config_path);
/* The user's own colours: [colors] and [desktop] in chrono.conf, applied on top of any theme. */
void chrono_theme_apply_overrides(struct chrono_theme *t, const char *config_path);
/* Write the theme (with whatever overrides it carries) as a .theme file named `name`. */
bool chrono_theme_save(const struct chrono_theme *t, const char *path, const char *name);
void chrono_theme_apply_accent(struct chrono_theme *t, const char *accent);

/* WCAG 2 contrast ratio between two colours (alpha is ignored), 1..21 */
double chrono_contrast(struct chrono_rgba a, struct chrono_rgba b);
double chrono_luminance(struct chrono_rgba c);

/* Directories themes are read from, most important first; returns how many were written. */
int chrono_theme_dirs(char dirs[][4096], int max);

#ifdef __cplusplus
}
#endif

#endif
