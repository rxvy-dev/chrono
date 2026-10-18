/* Idle: lock after a while and turn the screens off a bit later, unless something (a video, a game)
 * holds an idle inhibitor. Also feeds ext-idle-notify so tools like swayidle keep working. */
#include <time.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>

#include "server.h"

static struct timespec last;

static double since_activity(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;
}

static bool inhibited(struct chrono_server *s)
{
	return s->idle_inhibit && !wl_list_empty(&s->idle_inhibit->inhibitors);
}

static int tick(void *data)
{
	struct chrono_server *s = data;
	double idle = since_activity();
	wlr_idle_notifier_v1_set_inhibited(s->idle_notifier, inhibited(s));
	if (!inhibited(s)) {
		if (s->config.lock_after > 0 && idle >= s->config.lock_after && !lock_active(s))
			lock_screen(s);
		if (s->config.screen_off_after > 0 && idle >= s->config.screen_off_after && !s->screens_off)
			outputs_set_power(s, false);
	}
	wl_event_source_timer_update(s->idle_timer, 5000);
	return 0;
}

void idle_reset(struct chrono_server *s)
{
	(void)s;
	clock_gettime(CLOCK_MONOTONIC, &last);
}

static void inhibitor_changed(struct wl_listener *l, void *data)
{
	struct chrono_server *s = wl_container_of(l, s, new_inhibitor);
	(void)data;
	idle_reset(s);
}

void idle_init(struct chrono_server *s)
{
	clock_gettime(CLOCK_MONOTONIC, &last);
	s->idle_notifier = wlr_idle_notifier_v1_create(s->display);
	s->idle_inhibit = wlr_idle_inhibit_v1_create(s->display);
	s->new_inhibitor.notify = inhibitor_changed;
	wl_signal_add(&s->idle_inhibit->events.new_inhibitor, &s->new_inhibitor);
	s->idle_timer = wl_event_loop_add_timer(s->loop, tick, s);
	wl_event_source_timer_update(s->idle_timer, 5000);
}
