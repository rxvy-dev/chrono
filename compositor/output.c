/* Monitors: setup, saved layouts (~/.config/chrono/outputs.conf), the wlr-output-management protocol
 * (so the settings app, wlr-randr and kanshi can change them), DPMS, gamma / night light, and the
 * fallback wallpaper painted from the theme. */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wlr/render/color.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/util/log.h>
#include <wlr/util/transform.h>

#include "../common/ini.h"
#include "../common/paint.h"
#include "server.h"

static const char *transform_names[] = {"normal", "90", "180", "270", "flipped", "flipped-90", "flipped-180",
					"flipped-270"};

static enum wl_output_transform parse_transform(const char *s)
{
	for (int i = 0; i < 8; i++)
		if (!strcasecmp(s, transform_names[i]))
			return (enum wl_output_transform)i;
	return WL_OUTPUT_TRANSFORM_NORMAL;
}

struct chrono_output *output_at(struct chrono_server *s, double lx, double ly)
{
	struct wlr_output *wo = wlr_output_layout_output_at(s->output_layout, lx, ly);
	if (!wo) {
		/* off-screen: the closest output */
		double cx, cy;
		wlr_output_layout_closest_point(s->output_layout, NULL, lx, ly, &cx, &cy);
		wo = wlr_output_layout_output_at(s->output_layout, cx, cy);
	}
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link)
		if (o->wlr_output == wo)
			return o;
	return wl_list_empty(&s->outputs) ? NULL : wl_container_of(s->outputs.next, o, link);
}

struct chrono_output *output_of_view(struct chrono_view *v)
{
	if (wl_list_empty(&v->server->outputs))
		return NULL;
	return output_at(v->server, v->x + v->w / 2.0, v->y + v->h / 2.0);
}

/* ------------------------------------------------------------------------------- saved settings */
struct saved_output {
	bool found;
	int w, h;
	double refresh;
	double scale;
	enum wl_output_transform transform;
	bool has_pos;
	int x, y;
	bool enabled;
	bool vrr;
};

static void load_saved(struct chrono_server *s, const char *name, struct saved_output *so)
{
	memset(so, 0, sizeof *so);
	so->enabled = true;
	so->scale = 0;
	struct chrono_ini ini = {0};
	if (!chrono_ini_load(&ini, s->config.outputs_path))
		return;
	char k[160];
	snprintf(k, sizeof k, "output.%s.enabled", name);
	if (!chrono_ini_get(&ini, k, NULL)) {
		snprintf(k, sizeof k, "output.%s.mode", name);
		if (!chrono_ini_get(&ini, k, NULL)) {
			chrono_ini_free(&ini);
			return;
		}
	}
	so->found = true;
	snprintf(k, sizeof k, "output.%s.enabled", name);
	so->enabled = chrono_ini_bool(&ini, k, true);
	snprintf(k, sizeof k, "output.%s.mode", name);
	const char *m = chrono_ini_get(&ini, k, "");
	if (sscanf(m, "%dx%d@%lf", &so->w, &so->h, &so->refresh) < 2)
		so->w = so->h = 0;
	snprintf(k, sizeof k, "output.%s.scale", name);
	so->scale = chrono_ini_num(&ini, k, 0);
	snprintf(k, sizeof k, "output.%s.transform", name);
	so->transform = parse_transform(chrono_ini_get(&ini, k, "normal"));
	snprintf(k, sizeof k, "output.%s.x", name);
	const char *xs = chrono_ini_get(&ini, k, NULL);
	snprintf(k, sizeof k, "output.%s.y", name);
	const char *ys = chrono_ini_get(&ini, k, NULL);
	if (xs && ys) {
		so->has_pos = true;
		so->x = atoi(xs);
		so->y = atoi(ys);
	}
	snprintf(k, sizeof k, "output.%s.adaptive_sync", name);
	so->vrr = chrono_ini_bool(&ini, k, false);
	chrono_ini_free(&ini);
}

void output_save(struct chrono_server *s, struct chrono_output *o)
{
	struct wlr_output *wo = o->wlr_output;
	char sec[128], v[64];
	snprintf(sec, sizeof sec, "output.%s", wo->name);
	const char *path = s->config.outputs_path;
	chrono_ini_set_in_file(path, sec, "enabled", wo->enabled ? "true" : "false");
	if (wo->current_mode) {
		snprintf(v, sizeof v, "%dx%d@%.3f", wo->current_mode->width, wo->current_mode->height,
			 wo->current_mode->refresh / 1000.0);
		chrono_ini_set_in_file(path, sec, "mode", v);
	}
	snprintf(v, sizeof v, "%.3f", wo->scale);
	chrono_ini_set_in_file(path, sec, "scale", v);
	chrono_ini_set_in_file(path, sec, "transform", transform_names[wo->transform & 7]);
	struct wlr_box box;
	wlr_output_layout_get_box(s->output_layout, wo, &box);
	if (!wlr_box_empty(&box)) {
		snprintf(v, sizeof v, "%d", box.x);
		chrono_ini_set_in_file(path, sec, "x", v);
		snprintf(v, sizeof v, "%d", box.y);
		chrono_ini_set_in_file(path, sec, "y", v);
	}
	chrono_ini_set_in_file(path, sec, "adaptive_sync",
			       wo->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED ? "true" : "false");
}

static struct wlr_output_mode *find_mode(struct wlr_output *wo, int w, int h, double refresh)
{
	struct wlr_output_mode *m, *best = NULL;
	wl_list_for_each(m, &wo->modes, link) {
		if (m->width != w || m->height != h)
			continue;
		if (refresh <= 0) {
			if (!best || m->refresh > best->refresh)
				best = m;
			continue;
		}
		if (!best || fabs(m->refresh / 1000.0 - refresh) < fabs(best->refresh / 1000.0 - refresh))
			best = m;
	}
	return best;
}

/* A sensible default scale for high-DPI panels */
static double auto_scale(struct wlr_output *wo, struct wlr_output_mode *m)
{
	if (!m || wo->phys_width <= 0)
		return 1.0;
	double dpi = m->width / (wo->phys_width / 25.4);
	if (dpi > 260)
		return 2.0;
	if (dpi > 190)
		return 1.5;
	if (dpi > 150)
		return 1.25;
	return 1.0;
}

/* ------------------------------------------------------------------------------- desktop + layout */
void output_update_desktops(struct chrono_server *s)
{
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		if (!o->desktop)
			continue;
		bool show = o->wlr_output->enabled && o->full.width > 0;
		wlr_scene_node_set_enabled(&o->desktop->node, show);
		if (!show)
			continue;
		wlr_scene_node_set_position(&o->desktop->node, o->full.x, o->full.y);
		if (o->desktop_w == o->full.width && o->desktop_h == o->full.height && o->desktop_gen == s->theme_gen)
			continue;
		cairo_t *cr;
		/* painted at half resolution on huge screens: it's only seen while the shell starts */
		float sc = o->wlr_output->scale;
		if (o->full.width * sc > 3000)
			sc *= 0.5f;
		struct wlr_buffer *buf = cairo_buffer_create(o->full.width, o->full.height, sc, &cr);
		if (!buf)
			continue;
		chrono_paint_desktop(cr, &s->theme, o->full.width, o->full.height);
		cairo_buffer_done(buf);
		wlr_scene_buffer_set_buffer(o->desktop, buf);
		wlr_scene_buffer_set_dest_size(o->desktop, o->full.width, o->full.height);
		wlr_buffer_drop(buf);
		o->desktop_w = o->full.width;
		o->desktop_h = o->full.height;
		o->desktop_gen = s->theme_gen;
	}
}

void output_arrange_all(struct chrono_server *s)
{
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		wlr_output_layout_get_box(s->output_layout, o->wlr_output, &o->full);
		o->usable = o->full;
		layers_arrange(o);
	}
	output_update_desktops(s);

	/* keep every window on a visible output, and maximized / snapped / fullscreen ones sized to it */
	struct chrono_view *v;
	wl_list_for_each(v, &s->views, link) {
		if (!v->mapped)
			continue;
		struct wlr_box fb;
		view_frame_box(v, &fb);
		if (!wlr_output_layout_output_at(s->output_layout, fb.x + fb.width / 2.0, fb.y + 10)) {
			struct chrono_output *t = output_at(s, fb.x, fb.y);
			if (t)
				view_move_to(v, t->usable.x + 40, t->usable.y + 40 + (fb.y - v->y) * -1);
		}
		if (v->fullscreen) {
			struct chrono_output *t = output_of_view(v);
			if (t)
				view_resize(v, t->full.x, t->full.y, t->full.width, t->full.height);
		} else if (v->maximized) {
			v->maximized = false;
			view_set_maximized(v, true);
		} else if (v->snap) {
			int e = v->snap;
			v->snap = 0;
			view_snap(v, (uint32_t)e);
		}
	}
}

/* ------------------------------------------------------------------------------- output management */
void output_publish_config(struct chrono_server *s)
{
	if (!s->output_manager)
		return;
	struct wlr_output_configuration_v1 *cfg = wlr_output_configuration_v1_create();
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		struct wlr_output_configuration_head_v1 *head = wlr_output_configuration_head_v1_create(cfg, o->wlr_output);
		struct wlr_box box;
		wlr_output_layout_get_box(s->output_layout, o->wlr_output, &box);
		head->state.x = box.x;
		head->state.y = box.y;
	}
	wlr_output_manager_v1_set_configuration(s->output_manager, cfg);
}

static void layout_changed(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, output_layout_change);
	(void)data;
	output_arrange_all(s);
	output_publish_config(s);
	ipc_event(s, "outputs", NULL);
}

static bool apply_head(struct chrono_server *s, struct wlr_output_configuration_head_v1 *head, bool test_only)
{
	struct wlr_output *wo = head->state.output;
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	wlr_output_head_v1_state_apply(&head->state, &st);
	bool ok = test_only ? wlr_output_test_state(wo, &st) : wlr_output_commit_state(wo, &st);
	wlr_output_state_finish(&st);
	if (!ok || test_only)
		return ok;
	if (head->state.enabled) {
		if (!wlr_output_layout_get(s->output_layout, wo)) {
			struct wlr_output_layout_output *lo = wlr_output_layout_add(s->output_layout, wo, head->state.x, head->state.y);
			struct chrono_output *o;
			wl_list_for_each(o, &s->outputs, link) {
				if (o->wlr_output == wo) {
					if (!o->scene_output)
						o->scene_output = wlr_scene_output_create(s->scene, wo);
					wlr_scene_output_layout_add_output(s->scene_layout, lo, o->scene_output);
				}
			}
		} else {
			wlr_output_layout_add(s->output_layout, wo, head->state.x, head->state.y);
		}
	} else if (wlr_output_layout_get(s->output_layout, wo)) {
		struct chrono_output *o;
		wl_list_for_each(o, &s->outputs, link) {
			if (o->wlr_output == wo && o->scene_output) {
				wlr_scene_output_destroy(o->scene_output);
				o->scene_output = NULL;
			}
		}
		wlr_output_layout_remove(s->output_layout, wo);
	}
	return true;
}

static void manager_apply_or_test(struct chrono_server *s, struct wlr_output_configuration_v1 *cfg, bool test)
{
	bool ok = true;
	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each(head, &cfg->heads, link) {
		if (!apply_head(s, head, test)) {
			ok = false;
			break;
		}
	}
	if (ok)
		wlr_output_configuration_v1_send_succeeded(cfg);
	else
		wlr_output_configuration_v1_send_failed(cfg);
	wlr_output_configuration_v1_destroy(cfg);
	if (!test && ok) {
		struct chrono_output *o;
		wl_list_for_each(o, &s->outputs, link)
			output_save(s, o);
		wlr_xcursor_manager_load(s->xcursor, 1);
		wl_list_for_each(o, &s->outputs, link)
			wlr_xcursor_manager_load(s->xcursor, o->wlr_output->scale);
		output_arrange_all(s);
		output_publish_config(s);
		ipc_event(s, "outputs", NULL);
	}
}

static void manager_apply(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, output_manager_apply);
	manager_apply_or_test(s, data, false);
}

static void manager_test(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, output_manager_test);
	manager_apply_or_test(s, data, true);
}

/* "mode=2560x1440@143.9 scale=1.5 transform=90 x=0 y=0 enabled=true adaptive_sync=false" — used over IPC */
bool output_apply_settings(struct chrono_server *s, const char *name, const char *settings, char *err, size_t errlen)
{
	struct chrono_output *o = NULL, *it;
	wl_list_for_each(it, &s->outputs, link)
		if (!strcmp(it->wlr_output->name, name))
			o = it;
	if (!o) {
		snprintf(err, errlen, "no output called %s", name);
		return false;
	}
	struct wlr_output *wo = o->wlr_output;
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	bool enabled = wo->enabled;
	int x = INT32_MIN, y = INT32_MIN;
	char *copy = strdup(settings), *save = NULL;
	for (char *tok = strtok_r(copy, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
		char *eq = strchr(tok, '=');
		if (!eq)
			continue;
		*eq = '\0';
		const char *k = tok, *val = eq + 1;
		if (!strcmp(k, "mode")) {
			int w, h;
			double r = 0;
			if (sscanf(val, "%dx%d@%lf", &w, &h, &r) >= 2) {
				struct wlr_output_mode *m = find_mode(wo, w, h, r);
				if (m)
					wlr_output_state_set_mode(&st, m);
				else
					wlr_output_state_set_custom_mode(&st, w, h, (int32_t)(r * 1000));
			}
		} else if (!strcmp(k, "scale")) {
			double sc = atof(val);
			if (sc >= 0.5 && sc <= 4)
				wlr_output_state_set_scale(&st, (float)sc);
		} else if (!strcmp(k, "transform")) {
			wlr_output_state_set_transform(&st, parse_transform(val));
		} else if (!strcmp(k, "x")) {
			x = atoi(val);
		} else if (!strcmp(k, "y")) {
			y = atoi(val);
		} else if (!strcmp(k, "enabled")) {
			enabled = !strcmp(val, "true") || !strcmp(val, "1");
			wlr_output_state_set_enabled(&st, enabled);
		} else if (!strcmp(k, "adaptive_sync")) {
			wlr_output_state_set_adaptive_sync_enabled(&st, !strcmp(val, "true") || !strcmp(val, "1"));
		}
	}
	free(copy);
	/* never switch off the last enabled output */
	if (!enabled) {
		int on = 0;
		wl_list_for_each(it, &s->outputs, link)
			if (it->wlr_output->enabled)
				on++;
		if (on <= 1 && wo->enabled) {
			wlr_output_state_finish(&st);
			snprintf(err, errlen, "can't turn off the only screen");
			return false;
		}
	}
	if (!wlr_output_test_state(wo, &st)) {
		wlr_output_state_finish(&st);
		snprintf(err, errlen, "%s doesn't support that setting", name);
		return false;
	}
	bool ok = wlr_output_commit_state(wo, &st);
	wlr_output_state_finish(&st);
	if (!ok) {
		snprintf(err, errlen, "%s refused the change", name);
		return false;
	}
	if (enabled) {
		if (!wlr_output_layout_get(s->output_layout, wo)) {
			struct wlr_output_layout_output *lo = x != INT32_MIN
				? wlr_output_layout_add(s->output_layout, wo, x, y)
				: wlr_output_layout_add_auto(s->output_layout, wo);
			if (!o->scene_output)
				o->scene_output = wlr_scene_output_create(s->scene, wo);
			wlr_scene_output_layout_add_output(s->scene_layout, lo, o->scene_output);
		} else if (x != INT32_MIN && y != INT32_MIN) {
			wlr_output_layout_add(s->output_layout, wo, x, y);
		}
	} else if (wlr_output_layout_get(s->output_layout, wo)) {
		if (o->scene_output) {
			wlr_scene_output_destroy(o->scene_output);
			o->scene_output = NULL;
		}
		wlr_output_layout_remove(s->output_layout, wo);
	}
	output_save(s, o);
	wlr_xcursor_manager_load(s->xcursor, wo->scale);
	output_arrange_all(s);
	output_publish_config(s);
	ipc_event(s, "outputs", NULL);
	return true;
}

/* ------------------------------------------------------------------------------- per-output events */
static void output_frame(struct wl_listener *l, void *data)
{
	struct chrono_output *o = wl_container_of(l, o, frame);
	(void)data;
	if (!o->scene_output)
		return;
	struct wlr_scene_output_state_options opts = {.color_transform = o->night_tf};
	wlr_scene_output_commit(o->scene_output, &opts);
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(o->scene_output, &now);
}

static void output_request_state(struct wl_listener *l, void *data)
{
	struct chrono_output *o = wl_container_of(l, o, request_state);
	const struct wlr_output_event_request_state *e = data;
	wlr_output_commit_state(o->wlr_output, e->state);
}

static void output_destroy(struct wl_listener *l, void *data)
{
	struct chrono_output *o = wl_container_of(l, o, destroy);
	(void)data;
	wl_list_remove(&o->frame.link);
	wl_list_remove(&o->request_state.link);
	wl_list_remove(&o->destroy.link);
	wl_list_remove(&o->link);
	if (o->desktop)
		wlr_scene_node_destroy(&o->desktop->node);
	if (o->lock_buf)
		wlr_scene_node_destroy(&o->lock_buf->node);
	if (o->night_tf)
		wlr_color_transform_unref(o->night_tf);
	free(o);
}

static void new_output(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_output);
	struct wlr_output *wo = data;
	if (!wlr_output_init_render(wo, s->allocator, s->renderer)) {
		wlr_log(WLR_ERROR, "can't render on %s", wo->name);
		return;
	}
	struct saved_output so;
	load_saved(s, wo->name, &so);

	struct wlr_output_state st;
	wlr_output_state_init(&st);
	wlr_output_state_set_enabled(&st, so.enabled || !so.found);
	struct wlr_output_mode *mode = NULL;
	if (so.found && so.w > 0)
		mode = find_mode(wo, so.w, so.h, so.refresh);
	if (!mode)
		mode = wlr_output_preferred_mode(wo);
	if (mode)
		wlr_output_state_set_mode(&st, mode);
	double scale = so.found && so.scale > 0 ? so.scale : auto_scale(wo, mode);
	wlr_output_state_set_scale(&st, (float)scale);
	if (so.found)
		wlr_output_state_set_transform(&st, so.transform);
	if (so.vrr)
		wlr_output_state_set_adaptive_sync_enabled(&st, true);
	if (!wlr_output_commit_state(wo, &st)) {
		/* the saved mode may be gone (another monitor on that port): fall back to what it prefers */
		wlr_output_state_finish(&st);
		wlr_output_state_init(&st);
		wlr_output_state_set_enabled(&st, true);
		mode = wlr_output_preferred_mode(wo);
		if (mode)
			wlr_output_state_set_mode(&st, mode);
		wlr_output_commit_state(wo, &st);
	}
	wlr_output_state_finish(&st);

	struct chrono_output *o = calloc(1, sizeof *o);
	o->server = s;
	o->wlr_output = wo;
	wo->data = o;
	o->frame.notify = output_frame;
	wl_signal_add(&wo->events.frame, &o->frame);
	o->request_state.notify = output_request_state;
	wl_signal_add(&wo->events.request_state, &o->request_state);
	o->destroy.notify = output_destroy;
	wl_signal_add(&wo->events.destroy, &o->destroy);
	wl_list_insert(s->outputs.prev, &o->link);

	o->desktop = wlr_scene_buffer_create(s->layers[L_DESKTOP], NULL);

	if (wo->enabled) {
		struct wlr_output_layout_output *lo = so.found && so.has_pos
			? wlr_output_layout_add(s->output_layout, wo, so.x, so.y)
			: wlr_output_layout_add_auto(s->output_layout, wo);
		o->scene_output = wlr_scene_output_create(s->scene, wo);
		wlr_scene_output_layout_add_output(s->scene_layout, lo, o->scene_output);
	}
	wlr_xcursor_manager_load(s->xcursor, wo->scale);
	night_light_update(s);
	output_arrange_all(s);
	output_publish_config(s);
	lock_redraw(s);
	ipc_event(s, "outputs", NULL);
}

/* ------------------------------------------------------------------------------- DPMS */
void outputs_set_power(struct chrono_server *s, bool on)
{
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		if (!wlr_output_layout_get(s->output_layout, o->wlr_output))
			continue;
		struct wlr_output_state st;
		wlr_output_state_init(&st);
		wlr_output_state_set_enabled(&st, on);
		wlr_output_commit_state(o->wlr_output, &st);
		wlr_output_state_finish(&st);
	}
	s->screens_off = !on;
}

static void set_power_mode(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, set_output_power_mode);
	struct wlr_output_power_v1_set_mode_event *e = data;
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	wlr_output_state_set_enabled(&st, e->mode == ZWLR_OUTPUT_POWER_V1_MODE_ON);
	wlr_output_commit_state(e->output, &st);
	wlr_output_state_finish(&st);
	(void)s;
}

/* ------------------------------------------------------------------------------- gamma / night light */
static void temp_to_rgb(int kelvin, double *r, double *g, double *b)
{
	/* Tanner Helland's approximation of a black body, normalised so 6500 K is neutral */
	double t = kelvin / 100.0;
	double R, G, B;
	if (t <= 66) {
		R = 255;
		G = 99.4708025861 * log(t) - 161.1195681661;
		B = t <= 19 ? 0 : 138.5177312231 * log(t - 10) - 305.0447927307;
	} else {
		R = 329.698727446 * pow(t - 60, -0.1332047592);
		G = 288.1221695283 * pow(t - 60, -0.0755148492);
		B = 255;
	}
	*r = fmin(fmax(R, 0), 255) / 255.0;
	*g = fmin(fmax(G, 0), 255) / 255.0;
	*b = fmin(fmax(B, 0), 255) / 255.0;
}

static bool night_now(struct chrono_server *s)
{
	if (!s->config.night_enabled)
		return false;
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	int m = tm.tm_hour * 60 + tm.tm_min, from = s->config.night_from, to = s->config.night_to;
	if (from == to)
		return true;              /* always on */
	return from < to ? (m >= from && m < to) : (m >= from || m < to);
}

void night_light_update(struct chrono_server *s)
{
	bool on = night_now(s);
	s->night_active = on;
	/* one shared LUT; each output holds a reference. Applied in output_frame via the scene, so it
	 * works on every renderer and doesn't fight clients that use gamma-control. */
	struct wlr_color_transform *tf = NULL;
	if (on && !s->renderer->features.output_color_transform) {
		wlr_log(WLR_INFO, "night light: this renderer can't apply colour transforms (try the Vulkan or GLES renderer)");
		on = false;
	}
	if (on) {
		enum { N = 256 };
		uint16_t r[N], g[N], b[N];
		double fr, fg, fb;
		temp_to_rgb(s->config.night_temp, &fr, &fg, &fb);
		for (int i = 0; i < N; i++) {
			double v = (double)i / (N - 1) * 65535.0;
			r[i] = (uint16_t)(v * fr);
			g[i] = (uint16_t)(v * fg);
			b[i] = (uint16_t)(v * fb);
		}
		tf = wlr_color_transform_init_lut_3x1d(N, r, g, b);
	}
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		if (o->night_tf)
			wlr_color_transform_unref(o->night_tf);
		o->night_tf = tf ? wlr_color_transform_ref(tf) : NULL;
		wlr_output_schedule_frame(o->wlr_output);
	}
	if (tf)
		wlr_color_transform_unref(tf);
}

static int night_tick(void *data)
{
	struct chrono_server *s = data;
	if (night_now(s) != s->night_active)
		night_light_update(s);
	wl_event_source_timer_update(s->night_timer, 60 * 1000);
	return 0;
}

static void set_gamma(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, set_gamma);
	const struct wlr_gamma_control_manager_v1_set_gamma_event *e = data;
	if (s->config.night_enabled)
		return;                   /* our own night light owns the gamma ramps */
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	if (wlr_gamma_control_v1_apply(e->control, &st))
		wlr_output_commit_state(e->output, &st);
	wlr_output_state_finish(&st);
}

/* ------------------------------------------------------------------------------- IPC */
void output_write_json(struct chrono_server *s, char **buf, size_t *len)
{
	jbuf_append(buf, len, "[");
	bool first = true;
	struct chrono_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		struct wlr_output *wo = o->wlr_output;
		struct wlr_box box = {0};
		wlr_output_layout_get_box(s->output_layout, wo, &box);
		jbuf_append(buf, len, "%s{\"name\":", first ? "" : ",");
		first = false;
		jbuf_string(buf, len, wo->name);
		jbuf_append(buf, len, ",\"description\":");
		jbuf_string(buf, len, wo->description ? wo->description : "");
		jbuf_append(buf, len, ",\"make\":");
		jbuf_string(buf, len, wo->make ? wo->make : "");
		jbuf_append(buf, len, ",\"model\":");
		jbuf_string(buf, len, wo->model ? wo->model : "");
		jbuf_append(buf, len,
			    ",\"enabled\":%s,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"scale\":%.3f,"
			    "\"transform\":\"%s\",\"physical_width\":%d,\"physical_height\":%d,\"adaptive_sync\":%s",
			    wo->enabled ? "true" : "false", box.x, box.y, box.width, box.height, wo->scale,
			    transform_names[wo->transform & 7], wo->phys_width, wo->phys_height,
			    wo->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED ? "true" : "false");
		if (wo->current_mode)
			jbuf_append(buf, len, ",\"mode\":{\"width\":%d,\"height\":%d,\"refresh\":%.3f}",
				    wo->current_mode->width, wo->current_mode->height, wo->current_mode->refresh / 1000.0);
		else
			jbuf_append(buf, len, ",\"mode\":{\"width\":%d,\"height\":%d,\"refresh\":%.3f}", wo->width,
				    wo->height, wo->refresh / 1000.0);
		jbuf_append(buf, len, ",\"modes\":[");
		struct wlr_output_mode *m;
		bool mf = true;
		wl_list_for_each(m, &wo->modes, link) {
			jbuf_append(buf, len, "%s{\"width\":%d,\"height\":%d,\"refresh\":%.3f,\"preferred\":%s}", mf ? "" : ",",
				    m->width, m->height, m->refresh / 1000.0, m->preferred ? "true" : "false");
			mf = false;
		}
		jbuf_append(buf, len, "]}");
	}
	jbuf_append(buf, len, "]");
}

/* ------------------------------------------------------------------------------- init */
void output_init(struct chrono_server *s)
{
	wl_list_init(&s->outputs);
	s->output_layout = wlr_output_layout_create(s->display);
	s->scene_layout = wlr_scene_attach_output_layout(s->scene, s->output_layout);
	wlr_xdg_output_manager_v1_create(s->display, s->output_layout);
	s->output_layout_change.notify = layout_changed;
	wl_signal_add(&s->output_layout->events.change, &s->output_layout_change);

	s->new_output.notify = new_output;
	wl_signal_add(&s->backend->events.new_output, &s->new_output);

	s->output_manager = wlr_output_manager_v1_create(s->display);
	s->output_manager_apply.notify = manager_apply;
	wl_signal_add(&s->output_manager->events.apply, &s->output_manager_apply);
	s->output_manager_test.notify = manager_test;
	wl_signal_add(&s->output_manager->events.test, &s->output_manager_test);

	s->output_power = wlr_output_power_manager_v1_create(s->display);
	s->set_output_power_mode.notify = set_power_mode;
	wl_signal_add(&s->output_power->events.set_mode, &s->set_output_power_mode);

	s->gamma = wlr_gamma_control_manager_v1_create(s->display);
	s->set_gamma.notify = set_gamma;
	wl_signal_add(&s->gamma->events.set_gamma, &s->set_gamma);

	s->night_timer = wl_event_loop_add_timer(s->loop, night_tick, s);
	wl_event_source_timer_update(s->night_timer, 60 * 1000);
}
