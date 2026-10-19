#define _GNU_SOURCE
#include "paint.h"

#include <math.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>

#define C(c) (c).r, (c).g, (c).b, (c).a

/* ----------------------------------------------------------------------------- small helpers */
static void set(cairo_t *cr, const struct chrono_rgba *c) { cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a); }

static struct chrono_rgba mix(struct chrono_rgba a, struct chrono_rgba b, double t)
{
	return (struct chrono_rgba){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
				    a.a + (b.a - a.a) * t};
}

static struct chrono_rgba rgba(double r, double g, double b, double a) { return (struct chrono_rgba){r, g, b, a}; }

static void rect(cairo_t *cr, double x, double y, double w, double h, const struct chrono_rgba *c)
{
	set(cr, c);
	cairo_rectangle(cr, x, y, w, h);
	cairo_fill(cr);
}

static void round_rect(cairo_t *cr, double x, double y, double w, double h, double r, bool top_only)
{
	r = fmin(r, fmin(w, h) / 2);
	if (r <= 0) {
		cairo_rectangle(cr, x, y, w, h);
		return;
	}
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
	if (top_only) {
		cairo_line_to(cr, x + w, y + h);
		cairo_line_to(cr, x, y + h);
	} else {
		cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
		cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
	}
	cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
	cairo_close_path(cr);
}

static void vgrad(cairo_t *cr, double y0, double y1, struct chrono_rgba a, struct chrono_rgba b)
{
	cairo_pattern_t *p = cairo_pattern_create_linear(0, y0, 0, y1);
	cairo_pattern_add_color_stop_rgba(p, 0, C(a));
	cairo_pattern_add_color_stop_rgba(p, 1, C(b));
	cairo_set_source(cr, p);
	cairo_pattern_destroy(p);
}

static void hgrad(cairo_t *cr, double x0, double x1, struct chrono_rgba a, struct chrono_rgba b)
{
	cairo_pattern_t *p = cairo_pattern_create_linear(x0, 0, x1, 0);
	cairo_pattern_add_color_stop_rgba(p, 0, C(a));
	cairo_pattern_add_color_stop_rgba(p, 1, C(b));
	cairo_set_source(cr, p);
	cairo_pattern_destroy(p);
}

void chrono_paint_bevel(cairo_t *cr, double x, double y, double w, double h, const struct chrono_rgba *light,
			const struct chrono_rgba *dark, const struct chrono_rgba *shadow, bool sunken)
{
	const struct chrono_rgba *tl = sunken ? shadow : light, *br = sunken ? light : shadow;
	const struct chrono_rgba *tl2 = sunken ? dark : NULL, *br2 = sunken ? NULL : dark;
	cairo_save(cr);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	/* outer */
	rect(cr, x, y, w, 1, tl);
	rect(cr, x, y, 1, h, tl);
	rect(cr, x, y + h - 1, w, 1, br);
	rect(cr, x + w - 1, y, 1, h, br);
	/* inner */
	if (tl2) {
		rect(cr, x + 1, y + 1, w - 2, 1, tl2);
		rect(cr, x + 1, y + 1, 1, h - 2, tl2);
	}
	if (br2) {
		rect(cr, x + 1, y + h - 2, w - 2, 1, br2);
		rect(cr, x + w - 2, y + 1, 1, h - 2, br2);
	}
	cairo_restore(cr);
}

static bool inset_style(const struct chrono_theme *t)
{
	return t->decor == DECOR_BEVEL || t->decor == DECOR_MOTIF || t->decor == DECOR_CUBE;
}

static bool rounded_style(const struct chrono_theme *t)
{
	return t->decor == DECOR_FLAT || t->decor == DECOR_GLASS || t->decor == DECOR_NEON ||
	       t->decor == DECOR_MEADOW || t->decor == DECOR_PINSTRIPE;
}

static PangoLayout *title_layout(cairo_t *cr, const struct chrono_theme *t, const char *text)
{
	PangoLayout *l = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(t->font_title);
	if (t->title_bold)
		pango_font_description_set_weight(fd, PANGO_WEIGHT_BOLD);
	pango_layout_set_font_description(l, fd);
	pango_font_description_free(fd);
	pango_layout_set_text(l, text ? text : "", -1);
	pango_layout_set_single_paragraph_mode(l, TRUE);
	return l;
}

static int text_width(const struct chrono_theme *t, const char *text)
{
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(s);
	PangoLayout *l = title_layout(cr, t, text);
	int w, h;
	pango_layout_get_pixel_size(l, &w, &h);
	g_object_unref(l);
	cairo_destroy(cr);
	cairo_surface_destroy(s);
	return w;
}

/* ----------------------------------------------------------------------------- geometry */
void chrono_decor_geometry(const struct chrono_theme *t, int client_w, const char *title, struct chrono_decor_geom *g)
{
	memset(g, 0, sizeof *g);
	g->border = t->border_width;
	g->title_h = t->title_height;
	if (inset_style(t)) {
		g->title_y = g->border;
		g->top = g->border + g->title_h + (t->decor == DECOR_BEVEL ? 1 : 0);
	} else {
		g->title_y = 0;
		g->top = g->title_h;
	}
	if (g->title_h <= 0)
		return;
	int W = client_w + 2 * g->border;
	int bs = t->button_size;
	if (t->decor == DECOR_BEVEL || t->decor == DECOR_MOTIF)
		bs = g->title_h - 4;
	if (bs > g->title_h)
		bs = g->title_h;
	int bw = t->decor == DECOR_BEVEL ? bs + 2 : bs;   /* 95-style buttons are a bit wider than tall */
	int by = g->title_y + (g->title_h - bs) / 2;
	int pad = inset_style(t) ? 2 : (g->title_h - bs) / 2;
	int gap = t->button_shape == BSHAPE_GEL || t->button_shape == BSHAPE_CIRCLE ? 8 : (t->decor == DECOR_BEVEL ? 0 : 4);
	if (t->decor == DECOR_PIXEL || t->decor == DECOR_MOTIF)
		gap = 0;

	int x = g->border + pad;
	for (int i = 0; i < CHRONO_MAX_BUTTONS && t->buttons_left[i]; i++) {
		g->buttons[g->nbuttons++] = (struct chrono_button_rect){t->buttons_left[i], x, by, bw, bs};
		x += bw + gap;
	}
	int left_end = x;

	int nr = 0;
	while (nr < CHRONO_MAX_BUTTONS && t->buttons_right[nr])
		nr++;
	int rx = W - g->border - pad;
	int first_right = g->nbuttons;
	for (int i = nr - 1; i >= 0; i--) {
		/* the classic close button keeps a little distance from its neighbours */
		if (t->decor == DECOR_BEVEL && i < nr - 1 && t->buttons_right[i + 1] == BTN_CLOSE)
			rx -= 2;
		rx -= bw;
		g->buttons[g->nbuttons++] = (struct chrono_button_rect){t->buttons_right[i], rx, by, bw, bs};
		rx -= gap;
	}
	int right_start = g->nbuttons > first_right ? g->buttons[first_right + nr - 1].x : W - g->border;
	/* buttons were added right-to-left; right_start = leftmost right button */
	for (int i = first_right; i < g->nbuttons; i++)
		if (g->buttons[i].x < right_start)
			right_start = g->buttons[i].x;

	g->text_x = left_end + 6;
	g->text_w = right_start - 6 - g->text_x;

	if (t->decor == DECOR_TAB) {
		/* the tab hugs its content: left buttons, title, right buttons */
		int tw = text_width(t, title);
		int content = (left_end - g->border) + 12 + tw + 12 + (W - g->border - right_start);
		int tab = content + g->border;
		if (tab < 120)
			tab = 120;
		if (tab > W)
			tab = W;
		int shift = (W - g->border - pad) - (tab - pad);
		for (int i = first_right; i < g->nbuttons; i++)
			g->buttons[i].x -= shift;
		right_start -= shift;
		g->tab_w = tab;
		g->text_w = right_start - 6 - g->text_x;
	}
	if (g->text_w < 0)
		g->text_w = 0;
}

/* ----------------------------------------------------------------------------- buttons */
static void glyph(cairo_t *cr, enum chrono_button k, double x, double y, double w, double h,
		  const struct chrono_rgba *c, double lw, bool crisp)
{
	double s = fmin(w, h);
	double cx = x + w / 2, cy = y + h / 2;
	double r = s * 0.28;
	set(cr, c);
	cairo_set_line_width(cr, lw);
	cairo_set_line_cap(cr, crisp ? CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_ROUND);
	switch (k) {
	case BTN_CLOSE:
		cairo_move_to(cr, cx - r, cy - r);
		cairo_line_to(cr, cx + r, cy + r);
		cairo_move_to(cr, cx + r, cy - r);
		cairo_line_to(cr, cx - r, cy + r);
		cairo_stroke(cr);
		break;
	case BTN_MAXIMIZE:
		cairo_rectangle(cr, floor(cx - r) + 0.5, floor(cy - r) + 0.5, floor(2 * r), floor(2 * r));
		cairo_stroke(cr);
		if (crisp) {
			cairo_rectangle(cr, floor(cx - r), floor(cy - r), floor(2 * r) + 1, 2);
			cairo_fill(cr);
		}
		break;
	case BTN_MINIMIZE:
		if (crisp) {
			cairo_rectangle(cr, floor(cx - r), floor(cy + r) - 1, floor(1.4 * r), 2);
			cairo_fill(cr);
		} else {
			cairo_move_to(cr, cx - r, cy);
			cairo_line_to(cr, cx + r, cy);
			cairo_stroke(cr);
		}
		break;
	case BTN_MENU:
		for (int i = -1; i <= 1; i++) {
			cairo_move_to(cr, cx - r, cy + i * r * 0.75);
			cairo_line_to(cr, cx + r, cy + i * r * 0.75);
		}
		cairo_stroke(cr);
		break;
	default:
		break;
	}
}

static void paint_button(cairo_t *cr, const struct chrono_theme *t, const struct chrono_button_rect *b,
			 bool active, bool hover, bool pressed)
{
	double x = b->x, y = b->y, w = b->w, h = b->h;
	cairo_new_path(cr);
	struct chrono_rgba fg = active ? t->button_fg : mix(t->button_fg, t->title_inactive, 0.45);
	cairo_save(cr);
	switch (t->button_shape) {
	case BSHAPE_BEVEL:
		rect(cr, x, y, w, h, &t->button_bg);
		chrono_paint_bevel(cr, x, y, w, h, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, pressed);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		glyph(cr, b->kind, x + (pressed ? 1 : 0), y + (pressed ? 1 : 0), w - 1, h - 1, &t->button_fg, 2, true);
		break;
	case BSHAPE_SQUARE: {
		/* pixel gadgets */
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		struct chrono_rgba face = pressed ? t->accent : t->button_bg;
		rect(cr, x, y, w, h, &face);
		set(cr, &t->bevel_shadow);
		cairo_set_line_width(cr, 1);
		cairo_rectangle(cr, x + 0.5, y + 0.5, w - 1, h - 1);
		cairo_stroke(cr);
		if (b->kind == BTN_CLOSE) {
			rect(cr, x + w / 2 - 2, y + h / 2 - 2, 4, 4, &t->button_fg);
		} else if (b->kind == BTN_MAXIMIZE || b->kind == BTN_MINIMIZE) {
			/* depth gadget: two overlapping squares */
			set(cr, &t->button_fg);
			cairo_rectangle(cr, x + 3.5, y + 3.5, w * 0.5, h * 0.45);
			cairo_stroke(cr);
			rect(cr, x + w * 0.38, y + h * 0.4, w * 0.5, h * 0.45, b->kind == BTN_MAXIMIZE ? &t->accent : &t->button_fg);
		} else {
			glyph(cr, b->kind, x, y, w, h, &t->button_fg, 1, true);
		}
		break;
	}
	case BSHAPE_GEL: {
		static const char *cols[] = {NULL, "#ff5f57", "#28c840", "#febc2e", "#9aa0aa"};
		struct chrono_rgba c;
		chrono_parse_color(cols[b->kind < 5 ? b->kind : 4], &c);
		if (!active && !hover)
			c = rgba(0.78, 0.78, 0.80, 1);
		if (pressed)
			c = mix(c, rgba(0, 0, 0, 1), 0.25);
		double r = fmin(w, h) / 2;
		cairo_arc(cr, x + w / 2, y + h / 2, r, 0, 2 * M_PI);
		cairo_pattern_t *p = cairo_pattern_create_radial(x + w / 2, y + h * 0.7, r * 0.1, x + w / 2, y + h / 2, r);
		cairo_pattern_add_color_stop_rgba(p, 0, C(mix(c, rgba(1, 1, 1, 1), 0.35)));
		cairo_pattern_add_color_stop_rgba(p, 1, C(c));
		cairo_set_source(cr, p);
		cairo_fill_preserve(cr);
		cairo_pattern_destroy(p);
		cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
		cairo_set_line_width(cr, 0.8);
		cairo_stroke(cr);
		/* glossy highlight */
		cairo_save(cr);
		cairo_translate(cr, x + w / 2, y + h * 0.33);
		cairo_scale(cr, r * 0.62, r * 0.36);
		cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
		cairo_restore(cr);
		cairo_set_source_rgba(cr, 1, 1, 1, 0.55);
		cairo_fill(cr);
		if (hover) {
			struct chrono_rgba g = rgba(0, 0, 0, 0.55);
			glyph(cr, b->kind, x + w * 0.12, y + h * 0.12, w * 0.76, h * 0.76, &g, 1.2, false);
		}
		break;
	}
	case BSHAPE_CIRCLE: {
		struct chrono_rgba bg = b->kind == BTN_CLOSE && (hover || pressed) ? t->close_bg : t->button_bg;
		if (hover && b->kind != BTN_CLOSE)
			bg = mix(bg, t->title_active_fg, 0.18);
		if (pressed)
			bg = mix(bg, rgba(0, 0, 0, 1), 0.2);
		cairo_arc(cr, x + w / 2, y + h / 2, fmin(w, h) / 2, 0, 2 * M_PI);
		set(cr, &bg);
		cairo_fill(cr);
		struct chrono_rgba g = b->kind == BTN_CLOSE && (hover || pressed) ? rgba(1, 1, 1, 1) : fg;
		glyph(cr, b->kind, x + 1, y + 1, w - 2, h - 2, &g, 1.4, false);
		break;
	}
	case BSHAPE_ROUND_RECT: {
		struct chrono_rgba bg = b->kind == BTN_CLOSE ? t->close_bg : t->button_bg;
		if (!active)
			bg = mix(bg, t->title_inactive, 0.5);
		if (hover)
			bg = mix(bg, rgba(1, 1, 1, 1), 0.2);
		if (pressed)
			bg = mix(bg, rgba(0, 0, 0, 1), 0.2);
		round_rect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 3, false);
		vgrad(cr, y, y + h, mix(bg, rgba(1, 1, 1, 1), 0.35), bg);
		cairo_fill_preserve(cr);
		cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
		cairo_set_line_width(cr, 1);
		cairo_stroke(cr);
		struct chrono_rgba g = rgba(1, 1, 1, 1);
		glyph(cr, b->kind, x + 2, y + 2, w - 4, h - 4, &g, 2, false);
		break;
	}
	case BSHAPE_FLAT:
	default: {
		if (hover || pressed) {
			struct chrono_rgba bg = b->kind == BTN_CLOSE ? t->close_bg : mix(t->title_active, t->title_active_fg, 0.15);
			if (pressed)
				bg = mix(bg, rgba(0, 0, 0, 1), 0.2);
			round_rect(cr, x - 3, y - 3, w + 6, h + 6, 4, false);
			set(cr, &bg);
			cairo_fill(cr);
		}
		struct chrono_rgba g = (hover && b->kind == BTN_CLOSE) ? rgba(1, 1, 1, 1) : fg;
		glyph(cr, b->kind, x, y, w, h, &g, 1.5, false);
		break;
	}
	}
	cairo_restore(cr);
}

/* ----------------------------------------------------------------------------- title text */
static void paint_title_text(cairo_t *cr, const struct chrono_theme *t, const struct chrono_decor_geom *g,
			     int W, const char *title, const struct chrono_rgba *color, bool shadow)
{
	if (!title || !*title || g->text_w < 8)
		return;
	PangoLayout *l = title_layout(cr, t, title);
	pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_END);
	pango_layout_set_width(l, g->text_w * PANGO_SCALE);
	int tw, th;
	pango_layout_get_pixel_size(l, &tw, &th);
	double x = g->text_x;
	if (t->title_center) {
		int span = g->tab_w ? g->tab_w : W;
		double cx = (span - tw) / 2.0;
		if (cx < g->text_x)
			cx = g->text_x;
		if (cx + tw > g->text_x + g->text_w)
			cx = g->text_x + g->text_w - tw;
		x = cx;
	}
	double y = g->title_y + (g->title_h - th) / 2.0;
	if (shadow) {
		cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
		cairo_move_to(cr, round(x) + 1, round(y) + 1);
		pango_cairo_show_layout(cr, l);
	}
	set(cr, color);
	cairo_move_to(cr, round(x), round(y));
	pango_cairo_show_layout(cr, l);
	cairo_new_path(cr);
	g_object_unref(l);
}

/* horizontal ridges either side of a centred title (late-90s Mac look) */
static void ridges(cairo_t *cr, const struct chrono_theme *t, double x0, double x1, double y0, double h)
{
	if (x1 - x0 < 6)
		return;
	cairo_save(cr);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	for (double y = y0 + 3; y < y0 + h - 3; y += 2) {
		rect(cr, x0, y, x1 - x0, 1, &t->bevel_light);
		rect(cr, x0 + 1, y + 1, x1 - x0, 1, &t->bevel_dark);
	}
	cairo_restore(cr);
}

static void title_band_text_extent(cairo_t *cr, const struct chrono_theme *t, const struct chrono_decor_geom *g,
				   int W, const char *title, double *tx0, double *tx1)
{
	PangoLayout *l = title_layout(cr, t, title);
	int tw, th;
	pango_layout_get_pixel_size(l, &tw, &th);
	g_object_unref(l);
	if (tw > g->text_w)
		tw = g->text_w;
	*tx0 = (W - tw) / 2.0 - 8;
	*tx1 = (W + tw) / 2.0 + 8;
}

/* ----------------------------------------------------------------------------- the title buffer */
void chrono_decor_paint_top(cairo_t *cr, const struct chrono_theme *t, int client_w, const char *title, bool active,
			    int hover, int pressed)
{
	struct chrono_decor_geom g;
	chrono_decor_geometry(t, client_w, title, &g);
	int W = client_w + 2 * g.border, H = g.top;
	if (H <= 0)
		return;
	struct chrono_rgba ta = active ? t->title_active : t->title_inactive;
	struct chrono_rgba ta2 = active ? t->title_active2 : t->title_inactive2;
	struct chrono_rgba tf = active ? t->title_active_fg : t->title_inactive_fg;
	struct chrono_rgba bc = active ? t->border_active : t->border_inactive;

	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	bool text_shadow = false;
	switch (t->decor) {
	case DECOR_BEVEL: {
		rect(cr, 0, 0, W, H, &t->window_bg);
		/* outer frame: light / white on the top-left, black / dark on the right */
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		rect(cr, 0, 0, W, 1, &t->bevel_dark);       /* outermost top uses the soft light */
		rect(cr, 0, 0, W, 1, &t->bevel_light);
		rect(cr, 1, 1, W - 2, 1, &t->bevel_light);
		rect(cr, 0, 0, 1, H, &t->bevel_light);
		rect(cr, 1, 1, 1, H, &t->bevel_light);
		rect(cr, W - 1, 0, 1, H, &t->bevel_shadow);
		rect(cr, W - 2, 1, 1, H, &t->bevel_dark);
		double tx = g.border, tw = W - 2 * g.border;
		hgrad(cr, tx, tx + tw, ta, ta2);
		cairo_rectangle(cr, tx, g.title_y, tw, g.title_h);
		cairo_fill(cr);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		break;
	}
	case DECOR_MOTIF: {
		rect(cr, 0, 0, W, H, &ta);
		chrono_paint_bevel(cr, 0, 0, W, H + 8, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, false);
		double tx = g.border, tw = W - 2 * g.border;
		/* title is a raised bevel between the button boxes */
		double l = g.text_x - 6, r = g.text_x + g.text_w + 6;
		if (l < tx) l = tx;
		if (r > tx + tw) r = tx + tw;
		rect(cr, l, g.title_y, r - l, g.title_h, &ta2);
		chrono_paint_bevel(cr, l, g.title_y, r - l, g.title_h, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, false);
		break;
	}
	case DECOR_CUBE: {
		rect(cr, 0, 0, W, H, &t->window_bg);
		chrono_paint_bevel(cr, 0, 0, W, H + 4, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, false);
		rect(cr, g.border, g.title_y, W - 2 * g.border, g.title_h, &ta);
		rect(cr, g.border, g.title_y + g.title_h - 1, W - 2 * g.border, 1, &t->bevel_shadow);
		break;
	}
	case DECOR_PLATINUM: {
		vgrad(cr, 0, H, mix(ta, rgba(1, 1, 1, 1), 0.25), ta2);
		cairo_rectangle(cr, 0, 0, W, H);
		cairo_fill(cr);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		rect(cr, 0, 0, W, 1, &bc);
		rect(cr, 0, 0, 1, H, &bc);
		rect(cr, W - 1, 0, 1, H, &bc);
		rect(cr, 1, 1, W - 2, 1, &t->bevel_light);
		rect(cr, 0, H - 1, W, 1, &t->bevel_dark);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		if (active) {
			double tx0, tx1;
			title_band_text_extent(cr, t, &g, W, title, &tx0, &tx1);
			double lend = g.border + 4;
			double rstart = W - g.border - 4;
			for (int i = 0; i < g.nbuttons; i++) {
				if (g.buttons[i].x < W / 2.0)
					lend = fmax(lend, g.buttons[i].x + g.buttons[i].w + 5);
				else
					rstart = fmin(rstart, g.buttons[i].x - 5);
			}
			ridges(cr, t, lend, tx0, 0, H);
			ridges(cr, t, tx1, rstart, 0, H);
		}
		break;
	}
	case DECOR_PIXEL: {
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		rect(cr, 0, 0, W, H, &ta);
		rect(cr, 0, 0, W, 1, &bc);
		rect(cr, 0, 0, 1, H, &bc);
		rect(cr, W - 1, 0, 1, H, &bc);
		rect(cr, 0, H - 1, W, 1, &bc);
		if (active) {
			/* striped bars either side of the title, like old 4-colour screens */
			PangoLayout *l = title_layout(cr, t, title);
			int tw, th;
			pango_layout_get_pixel_size(l, &tw, &th);
			g_object_unref(l);
			if (tw > g.text_w)
				tw = g.text_w;
			double tx1 = t->title_center ? (W + tw) / 2.0 + 6 : g.text_x + tw + 6;
			double tx0 = t->title_center ? (W - tw) / 2.0 - 6 : g.text_x - 4;
			for (double y = 4; y < H - 3; y += 3) {
				if (t->title_center && tx0 > g.text_x)
					rect(cr, g.text_x, y, tx0 - g.text_x, 1, &ta2);
				if (g.text_x + g.text_w > tx1)
					rect(cr, tx1, y, g.text_x + g.text_w - tx1, 1, &ta2);
			}
		}
		break;
	}
	case DECOR_TAB: {
		int tab = g.tab_w ? g.tab_w : W;
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		vgrad(cr, 0, H, mix(ta, rgba(1, 1, 1, 1), 0.3), ta2);
		cairo_rectangle(cr, 0, 0, tab, H);
		cairo_fill(cr);
		rect(cr, 0, 0, tab, 1, &bc);
		rect(cr, 0, 0, 1, H, &bc);
		rect(cr, tab - 1, 0, 1, H, &bc);
		rect(cr, 1, 1, tab - 2, 1, &t->bevel_light);
		rect(cr, 1, 1, 1, H - 1, &t->bevel_light);
		/* the window's top edge continues past the tab */
		rect(cr, 0, H - 1, W, 1, &bc);
		break;
	}
	case DECOR_MEADOW: {
		round_rect(cr, 0, 0, W, H + 1, t->corner_radius, true);
		cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 0, H);
		cairo_pattern_add_color_stop_rgba(p, 0, C(mix(ta, rgba(1, 1, 1, 1), 0.35)));
		cairo_pattern_add_color_stop_rgba(p, 0.12, C(ta));
		cairo_pattern_add_color_stop_rgba(p, 0.55, C(ta2));
		cairo_pattern_add_color_stop_rgba(p, 1, C(mix(ta, rgba(0, 0, 0, 1), 0.15)));
		cairo_set_source(cr, p);
		cairo_fill(cr);
		cairo_pattern_destroy(p);
		text_shadow = true;
		break;
	}
	case DECOR_PINSTRIPE: {
		round_rect(cr, 0, 0, W, H + 1, t->corner_radius, true);
		cairo_save(cr);
		cairo_clip_preserve(cr);
		vgrad(cr, 0, H, mix(ta, rgba(1, 1, 1, 1), 0.4), ta2);
		cairo_fill(cr);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		struct chrono_rgba stripe = mix(ta2, rgba(1, 1, 1, 1), 0.5);
		stripe.a = active ? 0.7 : 0.35;
		for (int y = 1; y < H; y += 2)
			rect(cr, 0, y, W, 1, &stripe);
		cairo_restore(cr);
		rect(cr, 0, H - 1, W, 1, &bc);
		break;
	}
	case DECOR_GLASS: {
		round_rect(cr, 0, 0, W, H + 1, t->corner_radius, true);
		vgrad(cr, 0, H, ta, ta2);
		cairo_fill(cr);
		/* lit top edge */
		round_rect(cr, 0.5, 0.5, W - 1, H + 2, t->corner_radius, true);
		cairo_set_source_rgba(cr, 1, 1, 1, active ? 0.45 : 0.2);
		cairo_set_line_width(cr, 1);
		cairo_stroke(cr);
		break;
	}
	case DECOR_NEON: {
		round_rect(cr, 0, 0, W, H + 1, t->corner_radius, true);
		vgrad(cr, 0, H, ta, ta2);
		cairo_fill(cr);
		if (active) {
			hgrad(cr, 0, W, t->accent, mix(t->accent, t->close_bg, 0.8));
			cairo_rectangle(cr, 0, H - 2, W, 2);
			cairo_fill(cr);
			for (int i = 1; i <= 4; i++) {
				struct chrono_rgba gl = t->accent;
				gl.a = 0.18 / i;
				rect(cr, 0, H - 2 - i, W, 1, &gl);
			}
		}
		break;
	}
	case DECOR_FLAT:
	default: {
		round_rect(cr, 0, 0, W, H + 1, t->corner_radius, true);
		vgrad(cr, 0, H, ta, ta2);
		cairo_fill(cr);
		if (g.border > 0) {
			round_rect(cr, 0.5, 0.5, W - 1, H + 2, t->corner_radius, true);
			set(cr, &bc);
			cairo_set_line_width(cr, 1);
			cairo_stroke(cr);
		}
		rect(cr, g.border, H - 1, W - 2 * g.border, 1, &bc);
		break;
	}
	}

	paint_title_text(cr, t, &g, W, title, &tf, text_shadow);
	for (int i = 0; i < g.nbuttons; i++)
		paint_button(cr, t, &g.buttons[i], active, hover == i, pressed == i);
	cairo_restore(cr);
}

/* ----------------------------------------------------------------------------- edges */
void chrono_decor_paint_edge(cairo_t *cr, const struct chrono_theme *t, enum chrono_edge e, int w, int h, bool active)
{
	struct chrono_rgba bc = active ? t->border_active : t->border_inactive;
	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	switch (t->decor) {
	case DECOR_BEVEL:
	case DECOR_CUBE:
	case DECOR_MOTIF: {
		struct chrono_rgba face = t->decor == DECOR_MOTIF ? (active ? t->title_active : t->title_inactive) : t->window_bg;
		rect(cr, 0, 0, w, h, &face);
		if (e == EDGE_LEFT) {
			rect(cr, 0, 0, 1, h, &t->bevel_light);
			if (w > 2) rect(cr, 1, 0, 1, h, &t->bevel_light);
			if (t->decor != DECOR_BEVEL && w > 3) rect(cr, w - 1, 0, 1, h, &t->bevel_dark);
		} else if (e == EDGE_RIGHT) {
			rect(cr, w - 1, 0, 1, h, &t->bevel_shadow);
			if (w > 2) rect(cr, w - 2, 0, 1, h, &t->bevel_dark);
			if (t->decor != DECOR_BEVEL && w > 3) rect(cr, 0, 0, 1, h, &t->bevel_light);
		} else {
			rect(cr, 0, h - 1, w, 1, &t->bevel_shadow);
			if (h > 2) rect(cr, 1, h - 2, w - 2, 1, &t->bevel_dark);
			rect(cr, 0, 0, 1, h - 1, &t->bevel_light);
			if (h > 2) rect(cr, 1, 0, 1, h - 2, &t->bevel_light);
			rect(cr, w - 1, 0, 1, h, &t->bevel_shadow);
			if (w > 2) rect(cr, w - 2, 0, 1, h - 1, &t->bevel_dark);
			if (t->decor != DECOR_BEVEL && h > 3) rect(cr, w > 4 ? 2 : 0, 0, w > 4 ? w - 4 : w, 1, &t->bevel_dark);
		}
		break;
	}
	default:
		rect(cr, 0, 0, w, h, &bc);
		break;
	}
	cairo_restore(cr);
}

/* ----------------------------------------------------------------------------- shadow */
void chrono_paint_shadow(cairo_t *cr, const struct chrono_theme *t, int fw, int fh, bool active)
{
	int s = t->shadow_size;
	double a = t->shadow_alpha * (active ? 1.0 : 0.6);
	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	if (s <= 0 || !t->shadow) {
		cairo_restore(cr);
		return;
	}
	if (rounded_style(t) || t->decor == DECOR_FLAT) {
		/* soft gaussian-ish falloff from stacked rounded rects */
		double r = t->corner_radius + s * 0.6;
		for (int i = s; i > 0; i--) {
			double k = (double)i / s;
			double alpha = a * pow(1.0 - k, 2.2) / s * 3.2;
			round_rect(cr, s - i, s - i + s * 0.25, fw + 2 * i, fh + 2 * i, r * k + t->corner_radius * (1 - k), false);
			cairo_set_source_rgba(cr, 0, 0, 0, alpha);
			cairo_fill(cr);
		}
	} else {
		/* retro: a hard offset drop shadow */
		int off = s / 3 > 2 ? s / 3 : 2;
		cairo_set_source_rgba(cr, 0, 0, 0, a);
		cairo_rectangle(cr, s + off, s + off, fw, fh);
		cairo_fill(cr);
	}
	cairo_restore(cr);
}

/* ----------------------------------------------------------------------------- wallpapers */
static unsigned rng_state;
static double rnd(void)
{
	rng_state = rng_state * 1103515245u + 12345u;
	return ((rng_state >> 8) & 0xFFFFFF) / (double)0x1000000;
}

static void desktop_pattern(cairo_t *cr, const struct chrono_theme *t, const char *name, int w, int h)
{
	struct chrono_rgba a = t->desktop1, b = t->desktop2, acc = t->accent;
	double sc = fmax(w, h) / 1920.0;
	if (!strcmp(name, "waves")) {
		/* layered soft waves rising from the bottom, colour a at the top to b at the bottom */
		vgrad(cr, 0, h, a, mix(a, b, 0.35));
		cairo_paint(cr);
		for (int i = 0; i < 6; i++) {
			double base = h * (0.38 + i * 0.11), amp = h * (0.05 - i * 0.004), ph = i * 1.7;
			struct chrono_rgba c = mix(mix(a, b, 0.25 + i * 0.14), acc, i == 5 ? 0.12 : 0.04 * i);
			cairo_move_to(cr, 0, h);
			for (int x = 0; x <= w; x += 8)
				cairo_line_to(cr, x, base + sin(x / (w / (2.2 + i * 0.35)) * M_PI + ph) * amp + sin(x / (w / 7.0) + ph * 2) * amp * 0.25);
			cairo_line_to(cr, w, h);
			cairo_close_path(cr);
			set(cr, &c);
			cairo_fill(cr);
		}
	} else if (!strcmp(name, "topo")) {
		/* contour lines of an imaginary landscape */
		vgrad(cr, 0, h, a, mix(a, b, 0.5));
		cairo_paint(cr);
		cairo_set_line_width(cr, 1.2 * fmax(1, sc));
		for (int k = 1; k < 22; k++) {
			double lv = k / 22.0;
			struct chrono_rgba c = mix(b, acc, k % 5 == 0 ? 0.5 : 0.0);
			c.a = k % 5 == 0 ? 0.55 : 0.28;
			set(cr, &c);
			for (int ring = 0; ring < 3; ring++) {
				double cx = w * (0.25 + 0.3 * ring), cy = h * (0.35 + 0.2 * ((ring + 1) % 2));
				double R = fmax(w, h) * (0.06 + lv * 0.75) * (0.6 + 0.25 * ring);
				for (int st = 0; st <= 180; st++) {
					double th = st / 180.0 * 2 * M_PI;
					double r = R * (1 + 0.12 * sin(3 * th + ring + k * 0.2) + 0.06 * sin(7 * th - k * 0.3));
					double x = cx + cos(th) * r, y = cy + sin(th) * r * 0.75;
					if (st == 0) cairo_move_to(cr, x, y); else cairo_line_to(cr, x, y);
				}
				cairo_close_path(cr);
				cairo_stroke(cr);
			}
		}
	} else if (!strcmp(name, "blobs") || !strcmp(name, "material")) {
		/* big soft overlapping shapes; "material" uses crisp circles like a phone wallpaper */
		bool crisp = !strcmp(name, "material");
		rect(cr, 0, 0, w, h, &a);
		struct { double x, y, r, f; } c[] = {
			{0.15, 0.2, 0.42, 0.9}, {0.85, 0.15, 0.32, 0.5}, {0.7, 0.85, 0.48, 0.75}, {0.25, 0.95, 0.3, 0.35}, {0.5, 0.45, 0.2, 0.15},
		};
		for (size_t i = 0; i < sizeof c / sizeof *c; i++) {
			struct chrono_rgba col = mix(mix(a, b, c[i].f), acc, crisp ? 0.25 * (i % 2) : 0.18 * (i % 3 == 0));
			double R = c[i].r * fmax(w, h);
			if (crisp) {
				col.a = 0.9;
				set(cr, &col);
				cairo_arc(cr, c[i].x * w, c[i].y * h, R, 0, 2 * M_PI);
				cairo_fill(cr);
			} else {
				cairo_pattern_t *g = cairo_pattern_create_radial(c[i].x * w, c[i].y * h, 0, c[i].x * w, c[i].y * h, R);
				cairo_pattern_add_color_stop_rgba(g, 0, col.r, col.g, col.b, 0.85);
				cairo_pattern_add_color_stop_rgba(g, 1, col.r, col.g, col.b, 0);
				cairo_set_source(cr, g);
				cairo_paint(cr);
				cairo_pattern_destroy(g);
			}
		}
	} else if (!strcmp(name, "scanlines")) {
		/* a dark CRT: faint scanlines and a vignette */
		rect(cr, 0, 0, w, h, &a);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		struct chrono_rgba l = b;
		l.a = 0.35;
		for (int y = 0; y < h; y += 3)
			rect(cr, 0, y, w, 1, &l);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_pattern_t *v = cairo_pattern_create_radial(w / 2.0, h / 2.0, fmin(w, h) * 0.3, w / 2.0, h / 2.0, fmax(w, h) * 0.75);
		cairo_pattern_add_color_stop_rgba(v, 0, 0, 0, 0, 0);
		cairo_pattern_add_color_stop_rgba(v, 1, 0, 0, 0, 0.6);
		cairo_set_source(cr, v);
		cairo_paint(cr);
		cairo_pattern_destroy(v);
	} else if (!strcmp(name, "circuit")) {
		/* neon traces on a dark board */
		vgrad(cr, 0, h, a, b);
		cairo_paint(cr);
		rng_state = 11;
		double g = 32 * fmax(1, sc);
		cairo_set_line_width(cr, 1.5 * fmax(1, sc));
		for (int i = 0; i < 38; i++) {
			double x = floor(rnd() * w / g) * g, y = floor(rnd() * h / g) * g;
			struct chrono_rgba c = rnd() < 0.5 ? acc : mix(acc, b, 0.6);
			c.a = 0.18 + rnd() * 0.35;
			set(cr, &c);
			cairo_move_to(cr, x, y);
			for (int s2 = 0; s2 < 5; s2++) {
				double d = g * (1 + floor(rnd() * 4));
				if (rnd() < 0.5) x += rnd() < 0.5 ? d : -d; else y += rnd() < 0.5 ? d : -d;
				cairo_line_to(cr, x, y);
			}
			cairo_stroke(cr);
			cairo_arc(cr, x, y, 3 * fmax(1, sc), 0, 2 * M_PI);
			cairo_fill(cr);
		}
	} else if (!strcmp(name, "dunes")) {
		/* warm sand dunes under a pale sky */
		vgrad(cr, 0, h * 0.6, b, mix(b, a, 0.3));
		cairo_paint(cr);
		for (int i = 0; i < 4; i++) {
			struct chrono_rgba c = mix(a, mix(b, acc, 0.15), 0.15 + i * 0.12);
			double base = h * (0.55 + i * 0.12);
			cairo_move_to(cr, 0, h);
			cairo_line_to(cr, 0, base);
			cairo_curve_to(cr, w * 0.3, base - h * 0.12, w * 0.55, base + h * 0.08, w, base - h * (0.05 + 0.03 * i));
			cairo_line_to(cr, w, h);
			cairo_close_path(cr);
			set(cr, &c);
			cairo_fill(cr);
		}
	} else if (!strcmp(name, "rings")) {
		/* concentric rings from a corner (Rosé Pine / soft themes) */
		vgrad(cr, 0, h, a, mix(a, b, 0.4));
		cairo_paint(cr);
		cairo_set_line_width(cr, 2 * fmax(1, sc));
		for (int i = 1; i < 26; i++) {
			struct chrono_rgba c = mix(b, acc, (i % 4 == 0) ? 0.6 : 0.1);
			c.a = 0.10 + 0.25 * (i % 4 == 0);
			set(cr, &c);
			cairo_arc(cr, w * 0.82, h * 0.92, i * 58 * fmax(1, sc), 0, 2 * M_PI);
			cairo_stroke(cr);
		}
	} else if (!strcmp(name, "checker")) {
		rect(cr, 0, 0, w, h, &a);
		int s = 16;
		set(cr, &b);
		for (int y = 0; y < h; y += s)
			for (int x = (y / s) % 2 ? s : 0; x < w; x += 2 * s)
				cairo_rectangle(cr, x, y, s, s);
		cairo_fill(cr);
	} else if (!strcmp(name, "pinstripe")) {
		rect(cr, 0, 0, w, h, &a);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		for (int y = 0; y < h; y += 4)
			rect(cr, 0, y, w, 2, &b);
	} else if (!strcmp(name, "dots") || !strcmp(name, "platinum")) {
		rect(cr, 0, 0, w, h, &a);
		set(cr, &b);
		int s = !strcmp(name, "platinum") ? 4 : 12;
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		for (int y = 0; y < h; y += s)
			for (int x = (y / s) % 2 ? s / 2 : 0; x < w; x += s)
				cairo_rectangle(cr, x, y, s == 4 ? 1 : 2, s == 4 ? 1 : 2);
		cairo_fill(cr);
	} else if (!strcmp(name, "weave")) {
		/* a woven backdrop, as on old Unix workstations */
		rect(cr, 0, 0, w, h, &a);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
		int s = 8;
		for (int y = 0; y < h; y += s)
			for (int x = 0; x < w; x += s) {
				bool odd = ((x / s) + (y / s)) % 2;
				if (odd)
					rect(cr, x, y + s / 2 - 1, s, 2, &b);
				else
					rect(cr, x + s / 2 - 1, y, 2, s, &b);
			}
	} else if (!strcmp(name, "stars")) {
		vgrad(cr, 0, h, a, b);
		cairo_paint(cr);
		rng_state = 7;
		int n = w * h / 2600;
		for (int i = 0; i < n; i++) {
			double x = rnd() * w, y = rnd() * h, r = 0.4 + rnd() * rnd() * 1.6;
			cairo_set_source_rgba(cr, 1, 1, 1, 0.25 + rnd() * 0.7);
			cairo_arc(cr, x, y, r, 0, 2 * M_PI);
			cairo_fill(cr);
		}
	} else if (!strcmp(name, "grid")) {
		/* synthwave: sky gradient, a striped sun, a perspective grid */
		double hz = h * 0.62;
		vgrad(cr, 0, hz, a, b);
		cairo_rectangle(cr, 0, 0, w, hz);
		cairo_fill(cr);
		double sr = fmin(w, h) * 0.2;
		cairo_save(cr);
		cairo_arc(cr, w / 2.0, hz, sr, M_PI, 2 * M_PI);
		cairo_clip(cr);
		vgrad(cr, hz - sr, hz, t->title_active2, t->accent);
		cairo_paint(cr);
		for (int i = 0; i < 6; i++) {
			double y = hz - sr * 0.45 + i * sr * 0.09;
			rect(cr, 0, y, w, 2 + i * 0.8, &a);
		}
		cairo_restore(cr);
		struct chrono_rgba floor = rgba(0.04, 0.0, 0.08, 1);
		rect(cr, 0, hz, w, h - hz, &floor);
		struct chrono_rgba line = t->accent;
		line.a = 0.85;
		set(cr, &line);
		cairo_set_line_width(cr, 1.5);
		for (int i = -20; i <= 20; i++) {
			cairo_move_to(cr, w / 2.0 + i * w * 0.02, hz);
			cairo_line_to(cr, w / 2.0 + i * w * 0.12, h);
		}
		for (double k = 0.02; k < 1.0; k *= 1.45) {
			double y = hz + (h - hz) * k;
			cairo_move_to(cr, 0, y);
			cairo_line_to(cr, w, y);
		}
		cairo_stroke(cr);
	} else if (!strcmp(name, "hills")) {
		vgrad(cr, 0, h * 0.7, a, mix(a, rgba(1, 1, 1, 1), 0.55));
		cairo_paint(cr);
		/* a few soft clouds */
		rng_state = 3;
		for (int i = 0; i < 9; i++) {
			double cx = rnd() * w, cy = rnd() * h * 0.35, r = (0.03 + rnd() * 0.05) * w;
			cairo_pattern_t *p = cairo_pattern_create_radial(cx, cy, 0, cx, cy, r);
			cairo_pattern_add_color_stop_rgba(p, 0, 1, 1, 1, 0.55);
			cairo_pattern_add_color_stop_rgba(p, 1, 1, 1, 1, 0);
			cairo_set_source(cr, p);
			cairo_paint(cr);
			cairo_pattern_destroy(p);
		}
		cairo_move_to(cr, 0, h * 0.72);
		cairo_curve_to(cr, w * 0.3, h * 0.5, w * 0.62, h * 0.58, w, h * 0.66);
		cairo_line_to(cr, w, h);
		cairo_line_to(cr, 0, h);
		cairo_close_path(cr);
		vgrad(cr, h * 0.55, h, mix(b, rgba(1, 1, 0.6, 1), 0.25), mix(b, rgba(0, 0, 0, 1), 0.3));
		cairo_fill(cr);
	} else if (!strcmp(name, "aurora")) {
		vgrad(cr, 0, h, a, b);
		cairo_paint(cr);
		struct chrono_rgba cols[4] = {t->accent, t->title_active2, t->close_bg, t->selection};
		double pos[4][3] = {{0.18, 0.25, 0.55}, {0.82, 0.2, 0.45}, {0.6, 0.85, 0.6}, {0.1, 0.9, 0.4}};
		cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);
		for (int i = 0; i < 4; i++) {
			double cx = pos[i][0] * w, cy = pos[i][1] * h, r = pos[i][2] * fmax(w, h);
			cairo_pattern_t *p = cairo_pattern_create_radial(cx, cy, 0, cx, cy, r);
			cairo_pattern_add_color_stop_rgba(p, 0, cols[i].r, cols[i].g, cols[i].b, 0.55);
			cairo_pattern_add_color_stop_rgba(p, 1, cols[i].r, cols[i].g, cols[i].b, 0);
			cairo_set_source(cr, p);
			cairo_paint(cr);
			cairo_pattern_destroy(p);
		}
		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	} else {
		rect(cr, 0, 0, w, h, &a);
	}
}

void chrono_paint_desktop(cairo_t *cr, const struct chrono_theme *t, int w, int h)
{
	cairo_save(cr);
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_clip(cr);
	const char *wp = t->wallpaper;
	if (!strcmp(wp, "solid")) {
		rect(cr, 0, 0, w, h, &t->desktop1);
	} else if (!strcmp(wp, "gradient")) {
		vgrad(cr, 0, h, t->desktop1, t->desktop2);
		cairo_paint(cr);
	} else if (!strncmp(wp, "pattern:", 8)) {
		desktop_pattern(cr, t, wp + 8, w, h);
	} else {
		/* an image path: PNG via cairo; other formats are drawn by the shell */
		cairo_surface_t *img = cairo_image_surface_create_from_png(wp);
		if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
			int iw = cairo_image_surface_get_width(img), ih = cairo_image_surface_get_height(img);
			double sc = fmax((double)w / iw, (double)h / ih);
			cairo_translate(cr, (w - iw * sc) / 2, (h - ih * sc) / 2);
			cairo_scale(cr, sc, sc);
			cairo_set_source_surface(cr, img, 0, 0);
			cairo_paint(cr);
		} else {
			vgrad(cr, 0, h, t->desktop1, t->desktop2);
			cairo_paint(cr);
		}
		cairo_surface_destroy(img);
	}
	cairo_restore(cr);
}

/* ----------------------------------------------------------------------------- gallery preview */
static void text_at(cairo_t *cr, const char *font, const char *s, double x, double y, const struct chrono_rgba *c,
		    double scale, bool bold, int *out_w)
{
	PangoLayout *l = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(font);
	double sz = pango_font_description_get_size(fd) / (double)PANGO_SCALE;
	if (sz <= 0)
		sz = 10;
	pango_font_description_set_absolute_size(fd, sz * scale * PANGO_SCALE * 1.33);
	if (bold)
		pango_font_description_set_weight(fd, PANGO_WEIGHT_BOLD);
	pango_layout_set_font_description(l, fd);
	pango_font_description_free(fd);
	pango_layout_set_text(l, s, -1);
	int tw, th;
	pango_layout_get_pixel_size(l, &tw, &th);
	set(cr, c);
	cairo_move_to(cr, x, y - th / 2.0);
	pango_cairo_show_layout(cr, l);
	cairo_new_path(cr);
	if (out_w)
		*out_w = tw;
	g_object_unref(l);
}

static void preview_window(cairo_t *cr, const struct chrono_theme *t, double x, double y, int cw, int ch,
			   const char *title, bool active, double s)
{
	struct chrono_decor_geom g;
	chrono_decor_geometry(t, cw, title, &g);
	int W = cw + 2 * g.border, H = ch + g.top + g.border;
	cairo_save(cr);
	cairo_translate(cr, x, y);
	cairo_scale(cr, s, s);
	/* every piece is painted into its own group: the painters clear their target first */
	if (t->shadow) {
		cairo_save(cr);
		cairo_translate(cr, -t->shadow_size, -t->shadow_size);
		cairo_push_group(cr);
		chrono_paint_shadow(cr, t, W, H, active);
		cairo_pop_group_to_source(cr);
		cairo_paint(cr);
		cairo_restore(cr);
	}
	/* client area: a document with a few lines of text */
	rect(cr, g.border, g.top, cw, ch, &t->window_bg);
	struct chrono_rgba line = t->window_fg;
	line.a = 0.55;
	for (int i = 0; i < ch / 18 - 1; i++) {
		double lw = cw * (0.45 + 0.4 * ((i * 37) % 10) / 10.0);
		rect(cr, g.border + 14, g.top + 14 + i * 18, lw - 28, 6, &line);
	}
	cairo_save(cr);
	cairo_rectangle(cr, 0, 0, W, g.top);
	cairo_clip(cr);
	cairo_push_group(cr);
	chrono_decor_paint_top(cr, t, cw, title, active, -1, -1);
	cairo_pop_group_to_source(cr);
	cairo_paint(cr);
	cairo_restore(cr);
	if (g.border > 0) {
		struct { enum chrono_edge e; double x, y, w, h; } pieces[] = {
			{EDGE_LEFT, 0, g.top, g.border, ch},
			{EDGE_RIGHT, W - g.border, g.top, g.border, ch},
			{EDGE_BOTTOM, 0, g.top + ch, W, g.border},
		};
		for (int i = 0; i < 3; i++) {
			cairo_save(cr);
			cairo_translate(cr, pieces[i].x, pieces[i].y);
			cairo_rectangle(cr, 0, 0, pieces[i].w, pieces[i].h);
			cairo_clip(cr);
			cairo_push_group(cr);
			chrono_decor_paint_edge(cr, t, pieces[i].e, (int)pieces[i].w, (int)pieces[i].h, active);
			cairo_pop_group_to_source(cr);
			cairo_paint(cr);
			cairo_restore(cr);
		}
	}
	cairo_restore(cr);
}

void chrono_paint_preview(cairo_t *cr, const struct chrono_theme *t, int w, int h)
{
	cairo_save(cr);
	chrono_paint_desktop(cr, t, w, h);
	double s = w / 1280.0;            /* draw as if on a 1280-wide screen */
	double ph = t->panel_height * s;
	bool top = !strcmp(t->panel_position, "top");
	double py = top ? 0 : h - ph;

	preview_window(cr, t, w * 0.07, h * 0.12 + (top ? ph : 0), 520, 300, "Documents", false, s);
	preview_window(cr, t, w * 0.36, h * 0.27 + (top ? ph : 0), 600, 340, "Chrono Terminal", true, s);

	/* panel */
	bool floating = !strcmp(t->panel_style, "floating") || !strcmp(t->panel_style, "glass");
	double px = floating ? 8 * s : 0, pw = floating ? w - 16 * s : w;
	double pyy = floating ? (top ? 6 * s : py - 6 * s) : py;
	if (!strcmp(t->panel_style, "bevel")) {
		rect(cr, px, pyy, pw, ph, &t->panel_bg);
		chrono_paint_bevel(cr, px, pyy, pw, ph, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, false);
	} else if (floating) {
		round_rect(cr, px, pyy, pw, ph, ph / 2.6, false);
		set(cr, &t->panel_bg);
		cairo_fill_preserve(cr);
		set(cr, &t->panel_border);
		cairo_set_line_width(cr, 1);
		cairo_stroke(cr);
	} else {
		rect(cr, px, pyy, pw, ph, &t->panel_bg);
		rect(cr, px, top ? pyy + ph - 1 : pyy, pw, 1, &t->panel_border);
	}
	double cy = pyy + ph / 2;
	double bx = px + 6 * s;
	int lw = 0;
	if (t->launcher_label[0]) {
		/* launcher button */
		PangoLayout *dummy = NULL;
		(void)dummy;
		double bh = ph - 8 * s, bw = (strlen(t->launcher_label) * 9 + 34) * s;
		if (!strcmp(t->panel_style, "bevel")) {
			rect(cr, bx, pyy + 4 * s, bw, bh, &t->panel_bg);
			chrono_paint_bevel(cr, bx, pyy + 4 * s, bw, bh, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, false);
		} else {
			round_rect(cr, bx, pyy + 4 * s, bw, bh, bh / 2.5, false);
			set(cr, &t->accent);
			cairo_fill(cr);
		}
		struct chrono_rgba lc = !strcmp(t->panel_style, "bevel") ? t->panel_fg : t->accent_fg;
		text_at(cr, t->font_ui, t->launcher_label, bx + 12 * s, cy, &lc, s, true, &lw);
		bx += bw + 8 * s;
	} else {
		cairo_arc(cr, bx + ph / 2, cy, ph * 0.28, 0, 2 * M_PI);
		set(cr, &t->accent);
		cairo_fill(cr);
		bx += ph + 4 * s;
	}
	/* two task buttons */
	const char *tasks[] = {"Chrono Terminal", "Documents"};
	for (int i = 0; i < 2; i++) {
		double tw = 170 * s, th = ph - 10 * s;
		if (!strcmp(t->panel_style, "bevel")) {
			struct chrono_rgba face = i == 0 ? mix(t->panel_bg, t->bevel_light, 0.5) : t->panel_bg;
			rect(cr, bx, pyy + 5 * s, tw, th, &face);
			chrono_paint_bevel(cr, bx, pyy + 5 * s, tw, th, &t->bevel_light, &t->bevel_dark, &t->bevel_shadow, i == 0);
		} else if (i == 0) {
			round_rect(cr, bx, pyy + 5 * s, tw, th, 6 * s, false);
			struct chrono_rgba hl = t->accent;
			hl.a = 0.28;
			set(cr, &hl);
			cairo_fill(cr);
		}
		text_at(cr, t->font_ui, tasks[i], bx + 10 * s, cy, &t->panel_fg, s, i == 0, NULL);
		bx += tw + 6 * s;
	}
	/* clock */
	int cw;
	text_at(cr, t->font_ui, "12:45", -1000, cy, &t->panel_fg, s, false, &cw);
	if (!strcmp(t->panel_style, "bevel")) {
		double ww = cw + 24 * s;
		rect(cr, px + pw - ww - 6 * s, pyy + 5 * s, ww, ph - 10 * s, &t->panel_bg);
		chrono_paint_bevel(cr, px + pw - ww - 6 * s, pyy + 5 * s, ww, ph - 10 * s, &t->bevel_light, &t->bevel_dark,
				   &t->bevel_shadow, true);
	}
	text_at(cr, t->font_ui, "12:45", px + pw - cw - 18 * s, cy, &t->panel_fg, s, false, NULL);

	/* dock for the modern themes */
	if (t->panel_dock) {
		double dh = 54 * s, dw = 6 * 46 * s + 16 * s;
		double dx = (w - dw) / 2, dy = top ? h - dh - 10 * s : py - dh - 12 * s;
		round_rect(cr, dx, dy, dw, dh, 16 * s, false);
		set(cr, &t->panel_bg);
		cairo_fill_preserve(cr);
		set(cr, &t->panel_border);
		cairo_stroke(cr);
		struct chrono_rgba ic[6] = {t->accent, t->close_bg, t->title_active2, t->selection, t->accent, t->bevel_dark};
		for (int i = 0; i < 6; i++) {
			round_rect(cr, dx + 10 * s + i * 46 * s, dy + 8 * s, 38 * s, 38 * s, 9 * s, false);
			struct chrono_rgba c = ic[i];
			c.a = 1;
			vgrad(cr, dy, dy + dh, mix(c, rgba(1, 1, 1, 1), 0.3), c);
			cairo_fill(cr);
		}
	}
	cairo_restore(cr);
}
