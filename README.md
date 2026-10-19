# Chrono

A Wayland desktop built from scratch, with themes from every era of computing — and a settings app
that lets you change everything without touching a config file.

- **Compositor** (`chrono`): C on wlroots 0.20. Server-side window frames drawn per theme, snapping
  (halves, quarters, maximize), workspaces, Alt+Tab switcher, built-in lock screen, idle lock and
  screen-off, night light, XWayland, screenshots, media/volume/brightness keys.
- **Shell** (`chrono-shell`): Qt 6 / QML on layer-shell. A bottom bar with the app menu, workspaces,
  pinned apps, running apps, status icons and clock; a dock for themes that want one; quick settings;
  calendar with notification history; notifications; volume/brightness OSD; power menu.
- **Settings** (`chrono-settings`): themes gallery, panel & dock, displays (drag to arrange,
  resolution, refresh rate, fractional scale, rotation, VRR, auto-revert), time & date, keyboard
  layouts, mouse & touchpad, windows & workspaces, shortcuts (with a key recorder), sound, power &
  lock, default apps & autostart, about.
- **17 themes**: retro — Ninety-Five, Platinum, Topaz, Yellow Tab, Motif, Cube, Meadow, Pinstripe;
  modern — Midnight (default), Daylight, Liquid Glass, Nord, Mocha, Synthwave, Gruvbox, Starfield, Mono.

## Install

Arch Linux:

```sh
sudo pacman -S --needed base-devel cmake ninja wlroots0.20 wayland wayland-protocols libinput \
  libxkbcommon pixman cairo pango pam xcb-util-wm xorg-xwayland qt6-base qt6-declarative \
  qt6-wayland layer-shell-qt
# recommended extras
sudo pacman -S --needed foot grim slurp wl-clipboard brightnessctl playerctl libnotify \
  pipewire-pulse wireplumber networkmanager adwaita-icon-theme
git clone https://github.com/rxvy-dev/chrono && cd chrono
./install.sh
```

Other distros: install the same libraries (wlroots **0.20** is required) and run `./install.sh`.
It builds with CMake and installs to `/usr` so login managers find the session.

Then pick **Chrono** in your login manager, or run `chrono-session` from a TTY.

## Use

| Keys | Does |
| --- | --- |
| Super+Space / Super+D | App menu |
| Super+Return | Terminal (Interstellar if installed) |
| Super+E / Super+B | Files / browser |
| Super+Q, Alt+F4 | Close window |
| Super+←/→/↑/↓ | Snap left/right, maximize, minimize |
| Super+1…9, Super+Shift+1…9 | Go to / move window to workspace |
| Alt+Tab | Switch windows |
| Super+L | Lock |
| Super+I | Settings |
| Print, Super+Shift+S | Screenshot, area screenshot |

Everything is changeable in **Settings → Shortcuts**.

Right-click any app in the app menu or on the bar to **pin it to the bar**. Pinned apps stay on the bar;
running apps appear next to them.

## Configure by hand (optional)

`~/.config/chrono/chrono.conf` (INI). The settings app writes this file; `chronoctl reload` applies edits.

```ini
[general]
theme = ninety-five
terminal = interstellar
workspaces = 4

[panel]
position = bottom      ; bottom | top | theme
pinned = true
labels = true
dock = theme           ; theme | true | false

[clock]
format = 24h           ; auto | 12h | 24h
seconds = false
date = true

[keybinds]
Super+Shift+Return = exec foot
Super+F1 = none        ; turn a built-in shortcut off
```

`chronoctl` talks to the running desktop: `chronoctl windows`, `chronoctl theme topaz`,
`chronoctl output DP-1 mode=2560x1440@144 scale=1.25`, `chronoctl subscribe`, …

## Make a theme

Copy a file from `/usr/share/chrono/themes/` to `~/.local/share/chrono/themes/`, rename it and edit it:
palette, title-bar style (`flat bevel platinum pixel tab motif cube meadow pinstripe glass neon`),
button layout and shape, fonts, panel style (`bevel flat floating glass pixel`), wallpaper
(`solid`, `gradient`, `pattern:<checker|pinstripe|dots|platinum|weave|stars|grid|hills|aurora>` or an
image path), cursor and icons. `chrono-theme-preview mytheme out.png` renders a preview.

## Development

```sh
cmake -B build -G Ninja && ninja -C build
WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/chrono   # or run nested inside another compositor
```

CI builds on Arch with wlroots 0.20, runs the compositor and shell headless, and pushes logs and
screenshots of every theme to the `ci-results` branch.
