/* Painting for themes: window decorations (title bar, edges, shadow), wallpapers and gallery previews.
 * Pure cairo + pango, so the compositor, the shell and the settings app draw exactly the same pixels. */
#ifndef CHRONO_PAINT_H
#define CHRONO_PAINT_H

#include <cairo.h>
#include <stdbool.h>
#include "theme.h"

#ifdef __cplusplus
extern "C" {
#endif

enum chrono_edge { EDGE_LEFT, EDGE_RIGHT, EDGE_BOTTOM };

struct chrono_button_rect {
	enum chrono_button kind;
	int x, y, w, h;          /* in title-buffer coordinates */
};

/* Layout of the frame around a client of width client_w.
 * The title buffer is (client_w + 2*border) wide and `top` tall; the client sits at (border, top). */
struct chrono_decor_geom {
	int border;              /* left / right / bottom frame width */
	int top;                 /* frame height above the client (title bar + top frame) */
	int title_y, title_h;    /* title bar band inside the title buffer */
	int tab_w;               /* DECOR_TAB: width of the tab; 0 = the full width */
	int text_x, text_w;      /* where the title text goes */
	int nbuttons;
	struct chrono_button_rect buttons[2 * CHRONO_MAX_BUTTONS];
};

void chrono_decor_geometry(const struct chrono_theme *t, int client_w, const char *title,
			   struct chrono_decor_geom *g);

/* hover / pressed: index into g.buttons, or -1 */
void chrono_decor_paint_top(cairo_t *cr, const struct chrono_theme *t, int client_w, const char *title,
			    bool active, int hover, int pressed);
void chrono_decor_paint_edge(cairo_t *cr, const struct chrono_theme *t, enum chrono_edge e, int w, int h,
			     bool active);

/* Soft drop shadow for a frame of fw x fh; paints into a (fw + 2s) x (fh + 2s) area, s = shadow_size. */
void chrono_paint_shadow(cairo_t *cr, const struct chrono_theme *t, int fw, int fh, bool active);

void chrono_paint_desktop(cairo_t *cr, const struct chrono_theme *t, int w, int h);
void chrono_paint_preview(cairo_t *cr, const struct chrono_theme *t, int w, int h);

/* Helpers for panel painting in other toolkits */
void chrono_paint_bevel(cairo_t *cr, double x, double y, double w, double h, const struct chrono_rgba *light,
			const struct chrono_rgba *dark, const struct chrono_rgba *shadow, bool sunken);

#ifdef __cplusplus
}
#endif

#endif
