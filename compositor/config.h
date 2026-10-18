#ifndef CHRONO_CONFIG_H
#define CHRONO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

#define CHRONO_MAX_BINDS 160

struct chrono_bind {
	uint32_t mods;            /* WLR_MODIFIER_* */
	xkb_keysym_t sym;
	char action[256];
};

struct chrono_config {
	char path[512];           /* ~/.config/chrono/chrono.conf */
	char outputs_path[512];   /* ~/.config/chrono/outputs.conf */

	/* general */
	char theme[64];
	char terminal[256];
	char file_manager[256];
	char browser[256];
	int workspaces;
	bool start_shell;
	char autostart[2048];     /* commands separated by ';' */

	/* keyboard */
	char kb_layout[64], kb_variant[64], kb_options[256], kb_model[64];
	int repeat_rate, repeat_delay;
	bool numlock;

	/* pointer / touchpad */
	double accel, tp_accel;
	bool flat_accel;
	bool natural_scroll, tp_natural_scroll, left_handed, tap, dwt;
	double scroll_factor;

	/* windows */
	bool focus_follows_mouse;
	bool snap;
	int snap_threshold;
	char double_click_title[16];  /* maximize | minimize | none */
	char placement[16];           /* center | cascade | cursor */
	int gaps;

	/* power */
	int lock_after, screen_off_after;   /* seconds, 0 = never */

	/* night light */
	bool night_enabled;
	int night_temp;
	int night_from, night_to;           /* minutes after midnight */

	/* cursor override */
	char cursor_theme[64];
	int cursor_size;

	struct chrono_bind binds[CHRONO_MAX_BINDS];
	int nbinds;
};

void config_load(struct chrono_config *c);
bool config_parse_combo(const char *combo, uint32_t *mods, xkb_keysym_t *sym);
const char *config_default_terminal(void);

#endif
