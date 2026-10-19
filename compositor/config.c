#define _GNU_SOURCE
#include "config.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/types/wlr_keyboard.h>

#include "../common/ini.h"

static const char *default_binds[][2] = {
	{"Super+Return", "exec terminal"},
	{"Super+T", "exec terminal"},
	{"Super+E", "exec files"},
	{"Super+B", "exec browser"},
	{"Super+Space", "launcher"},
	{"Alt+F1", "launcher"},
	{"Super+I", "settings"},
	{"Super+L", "lock"},
	{"Super+Q", "close"},
	{"Alt+F4", "close"},
	{"Super+Up", "maximize"},
	{"Super+Down", "minimize"},
	{"Super+Left", "snap left"},
	{"Super+Right", "snap right"},
	{"Super+F", "fullscreen"},
	{"Alt+Tab", "switcher next"},
	{"Alt+Shift+Tab", "switcher prev"},
	{"Super+Tab", "switcher next"},
	{"Ctrl+Super+Left", "workspace prev"},
	{"Ctrl+Super+Right", "workspace next"},
	{"Ctrl+Alt+Left", "workspace prev"},
	{"Ctrl+Alt+Right", "workspace next"},
	{"Super+1", "workspace 1"}, {"Super+2", "workspace 2"}, {"Super+3", "workspace 3"},
	{"Super+4", "workspace 4"}, {"Super+5", "workspace 5"}, {"Super+6", "workspace 6"},
	{"Super+7", "workspace 7"}, {"Super+8", "workspace 8"}, {"Super+9", "workspace 9"},
	{"Super+Shift+1", "move-to-workspace 1"}, {"Super+Shift+2", "move-to-workspace 2"},
	{"Super+Shift+3", "move-to-workspace 3"}, {"Super+Shift+4", "move-to-workspace 4"},
	{"Super+Shift+5", "move-to-workspace 5"}, {"Super+Shift+6", "move-to-workspace 6"},
	{"Super+Shift+7", "move-to-workspace 7"}, {"Super+Shift+8", "move-to-workspace 8"},
	{"Super+Shift+9", "move-to-workspace 9"},
	{"Super+S", "screenshot"},
	{"Print", "screenshot"},
	{"Shift+Print", "screenshot area"},
	{"Super+Shift+S", "screenshot area"},
	{"Super+Shift+R", "reload"},
	{"Super+Shift+E", "power-menu"},
	{"Ctrl+Alt+Delete", "power-menu"},
	{"XF86AudioRaiseVolume", "volume up"},
	{"XF86AudioLowerVolume", "volume down"},
	{"XF86AudioMute", "volume mute"},
	{"XF86AudioMicMute", "mic mute"},
	{"XF86MonBrightnessUp", "brightness up"},
	{"XF86MonBrightnessDown", "brightness down"},
	{"XF86AudioPlay", "media play-pause"},
	{"XF86AudioPause", "media play-pause"},
	{"XF86AudioNext", "media next"},
	{"XF86AudioPrev", "media previous"},
};

static const struct { const char *name; uint32_t mod; } modnames[] = {
	{"super", WLR_MODIFIER_LOGO}, {"logo", WLR_MODIFIER_LOGO}, {"win", WLR_MODIFIER_LOGO},
	{"meta", WLR_MODIFIER_LOGO}, {"mod4", WLR_MODIFIER_LOGO},
	{"ctrl", WLR_MODIFIER_CTRL}, {"control", WLR_MODIFIER_CTRL},
	{"alt", WLR_MODIFIER_ALT}, {"mod1", WLR_MODIFIER_ALT},
	{"shift", WLR_MODIFIER_SHIFT},
	{"altgr", WLR_MODIFIER_MOD5}, {"mod5", WLR_MODIFIER_MOD5},
};

bool config_parse_combo(const char *combo, uint32_t *mods, xkb_keysym_t *sym)
{
	*mods = 0;
	*sym = XKB_KEY_NoSymbol;
	char buf[128];
	snprintf(buf, sizeof buf, "%s", combo);
	char *save = NULL;
	for (char *tok = strtok_r(buf, "+", &save); tok; tok = strtok_r(NULL, "+", &save)) {
		while (*tok == ' ')
			tok++;
		char *e = tok + strlen(tok);
		while (e > tok && e[-1] == ' ')
			*--e = '\0';
		bool found = false;
		for (size_t i = 0; i < sizeof modnames / sizeof *modnames; i++) {
			if (!strcasecmp(tok, modnames[i].name)) {
				*mods |= modnames[i].mod;
				found = true;
			}
		}
		if (found)
			continue;
		xkb_keysym_t ks = xkb_keysym_from_name(tok, XKB_KEYSYM_NO_FLAGS);
		if (ks == XKB_KEY_NoSymbol)
			ks = xkb_keysym_from_name(tok, XKB_KEYSYM_CASE_INSENSITIVE);
		if (ks == XKB_KEY_NoSymbol)
			return false;
		*sym = xkb_keysym_to_lower(ks);
	}
	return *sym != XKB_KEY_NoSymbol;
}

static void add_bind(struct chrono_config *c, const char *combo, const char *action)
{
	uint32_t mods;
	xkb_keysym_t sym;
	if (!config_parse_combo(combo, &mods, &sym)) {
		fprintf(stderr, "chrono: ignoring keybind with unknown key: %s\n", combo);
		return;
	}
	/* a later binding replaces an earlier one for the same keys; "none" removes it */
	for (int i = 0; i < c->nbinds; i++) {
		if (c->binds[i].mods == mods && c->binds[i].sym == sym) {
			if (!strcasecmp(action, "none") || !*action) {
				c->binds[i] = c->binds[--c->nbinds];
			} else {
				snprintf(c->binds[i].action, sizeof c->binds[i].action, "%s", action);
			}
			return;
		}
	}
	if (!strcasecmp(action, "none") || !*action || c->nbinds >= CHRONO_MAX_BINDS)
		return;
	struct chrono_bind *b = &c->binds[c->nbinds++];
	b->mods = mods;
	b->sym = sym;
	snprintf(b->action, sizeof b->action, "%s", action);
}

static bool on_path(const char *prog)
{
	const char *path = getenv("PATH");
	if (!path)
		path = "/usr/local/bin:/usr/bin:/bin";
	char *dup = strdup(path), *save = NULL;
	bool ok = false;
	for (char *d = strtok_r(dup, ":", &save); d && !ok; d = strtok_r(NULL, ":", &save)) {
		char p[PATH_MAX];
		snprintf(p, sizeof p, "%s/%s", d, prog);
		ok = access(p, X_OK) == 0;
	}
	free(dup);
	return ok;
}

const char *config_default_terminal(void)
{
	static const char *terms[] = {"interstellar", "kitty", "foot", "alacritty", "wezterm", "ghostty", "konsole",
				      "gnome-terminal", "xfce4-terminal", "xterm"};
	for (size_t i = 0; i < sizeof terms / sizeof *terms; i++)
		if (on_path(terms[i]))
			return terms[i];
	return "xterm";
}

static int parse_hhmm(const char *s, int def)
{
	int h, m;
	if (s && sscanf(s, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60)
		return h * 60 + m;
	return def;
}

#define STR(dst, key, def) snprintf(dst, sizeof dst, "%s", chrono_ini_get(&ini, key, def))

void config_load(struct chrono_config *c)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	char dir[400];
	if (xdg && *xdg)
		snprintf(dir, sizeof dir, "%s/chrono", xdg);
	else
		snprintf(dir, sizeof dir, "%s/.config/chrono", home ? home : "/tmp");
	snprintf(c->path, sizeof c->path, "%s/chrono.conf", dir);
	snprintf(c->outputs_path, sizeof c->outputs_path, "%s/outputs.conf", dir);
	if (getenv("CHRONO_CONFIG"))
		snprintf(c->path, sizeof c->path, "%s", getenv("CHRONO_CONFIG"));

	struct chrono_ini ini = {0};
	chrono_ini_load(&ini, c->path);

	STR(c->theme, "general.theme", "midnight");
	STR(c->appearance_mode, "appearance.mode", "theme");
	STR(c->accent, "appearance.accent", "theme");
	c->reduce_motion = chrono_ini_bool(&ini, "appearance.reduce_motion", false);
	c->super_opens_launcher = chrono_ini_bool(&ini, "general.super_opens_launcher", true);
	STR(c->terminal, "general.terminal", "");
	if (!c->terminal[0])
		snprintf(c->terminal, sizeof c->terminal, "%s", config_default_terminal());
	STR(c->file_manager, "general.file_manager", "xdg-open ~");
	STR(c->browser, "general.browser", "xdg-open https://");
	c->workspaces = (int)chrono_ini_num(&ini, "general.workspaces", 4);
	if (c->workspaces < 1) c->workspaces = 1;
	if (c->workspaces > 9) c->workspaces = 9;
	c->start_shell = chrono_ini_bool(&ini, "general.shell", true);
	STR(c->autostart, "general.autostart", "");

	STR(c->kb_layout, "keyboard.layout", "");
	STR(c->kb_variant, "keyboard.variant", "");
	STR(c->kb_options, "keyboard.options", "");
	STR(c->kb_model, "keyboard.model", "");
	/* the settings app keeps the Caps Lock choice separate from the layout-switch option */
	const char *caps = chrono_ini_get(&ini, "keyboard.caps", "");
	if (caps[0] && strlen(c->kb_options) + strlen(caps) + 2 < sizeof c->kb_options) {
		if (c->kb_options[0])
			strcat(c->kb_options, ",");
		strcat(c->kb_options, caps);
	}
	c->repeat_rate = (int)chrono_ini_num(&ini, "keyboard.repeat_rate", 30);
	c->repeat_delay = (int)chrono_ini_num(&ini, "keyboard.repeat_delay", 300);
	c->numlock = chrono_ini_bool(&ini, "keyboard.numlock", true);

	c->accel = chrono_ini_num(&ini, "pointer.accel", 0);
	c->flat_accel = !strcasecmp(chrono_ini_get(&ini, "pointer.accel_profile", "adaptive"), "flat");
	c->natural_scroll = chrono_ini_bool(&ini, "pointer.natural_scroll", false);
	c->left_handed = chrono_ini_bool(&ini, "pointer.left_handed", false);
	c->scroll_factor = chrono_ini_num(&ini, "pointer.scroll_speed", 1.0);
	c->tap = chrono_ini_bool(&ini, "touchpad.tap", true);
	c->tp_natural_scroll = chrono_ini_bool(&ini, "touchpad.natural_scroll", true);
	c->dwt = chrono_ini_bool(&ini, "touchpad.disable_while_typing", true);
	c->tp_accel = chrono_ini_num(&ini, "touchpad.accel", 0.2);

	c->focus_follows_mouse = chrono_ini_bool(&ini, "windows.focus_follows_mouse", false);
	c->snap = chrono_ini_bool(&ini, "windows.snap", true);
	c->snap_threshold = (int)chrono_ini_num(&ini, "windows.snap_threshold", 10);
	STR(c->double_click_title, "windows.double_click_title", "maximize");
	STR(c->placement, "windows.placement", "center");
	c->gaps = (int)chrono_ini_num(&ini, "windows.gaps", 0);

	c->lock_after = (int)chrono_ini_num(&ini, "power.lock_after", 600);
	c->screen_off_after = (int)chrono_ini_num(&ini, "power.screen_off_after", 900);

	c->night_enabled = chrono_ini_bool(&ini, "night_light.enabled", false);
	c->night_temp = (int)chrono_ini_num(&ini, "night_light.temperature", 4000);
	if (c->night_temp < 1500) c->night_temp = 1500;
	if (c->night_temp > 6500) c->night_temp = 6500;
	c->night_from = parse_hhmm(chrono_ini_get(&ini, "night_light.from", NULL), 20 * 60);
	c->night_to = parse_hhmm(chrono_ini_get(&ini, "night_light.to", NULL), 7 * 60);

	STR(c->cursor_theme, "cursor.theme", "");
	c->cursor_size = (int)chrono_ini_num(&ini, "cursor.size", 0);

	c->nbinds = 0;
	for (size_t i = 0; i < sizeof default_binds / sizeof *default_binds; i++)
		add_bind(c, default_binds[i][0], default_binds[i][1]);
	for (size_t i = 0; i < ini.count; i++) {
		const char *k = ini.entries[i].key;
		if (!strncmp(k, "keybinds.", 9))
			add_bind(c, k + 9, ini.entries[i].value);
	}
	chrono_ini_free(&ini);
}
