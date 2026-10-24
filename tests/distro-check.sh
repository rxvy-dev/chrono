#!/bin/sh
# After ./install.sh on a fresh distro: start the *installed* chrono headless with its shell,
# open Settings, and check nothing is missing at runtime (QML modules, plugins, libraries).
#   tests/distro-check.sh OUTDIR
set -u
OUT=${1:-results}
mkdir -p "$OUT"
export XDG_RUNTIME_DIR=$(mktemp -d) WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=1
export WLR_LIBINPUT_NO_DEVICES=1 HOME=$(mktemp -d) QT_QUICK_BACKEND=software QT_FORCE_STDERR_LOGGING=1
chmod 700 "$XDG_RUNTIME_DIR"
mkdir -p "$HOME/.config/chrono"
printf '[general]\ntheme = midnight\n[power]\nlock_after = 0\nscreen_off_after = 0\n' > "$HOME/.config/chrono/chrono.conf"
fail=0
err() { echo "::error::$*"; fail=1; }

if command -v ldd >/dev/null; then
	ldd "$(command -v chrono)" | grep -q "not found" && { ldd "$(command -v chrono)" | grep "not found"; err "chrono has missing libraries"; }
	ldd "$(command -v chrono)" | grep -E "wlroots|wayland-server|drm" > "$OUT/wlroots-lib.txt"
fi

chrono -d >"$OUT/chrono.log" 2>&1 &
PID=$!
sock=""
for i in $(seq 50); do
	sock=$(ls "$XDG_RUNTIME_DIR"/wayland-* 2>/dev/null | grep -v lock | head -1)
	[ -n "$sock" ] && break
	sleep 0.2
done
[ -n "$sock" ] && kill -0 $PID 2>/dev/null || { tail -40 "$OUT/chrono.log"; err "chrono did not start"; exit 1; }
export WAYLAND_DISPLAY=$(basename "$sock")
sleep 6
chronoctl ping >/dev/null || err "chronoctl ping failed"
running() { grep -lx "$1" /proc/[0-9]*/comm >/dev/null 2>&1; }
running chrono-shell || err "chrono-shell is not running"
QT_QPA_PLATFORM=wayland chrono-settings >"$OUT/settings.log" 2>&1 &
SP=$!
sleep 5
kill -0 $SP 2>/dev/null || err "chrono-settings exited"
grim "$OUT/screen.png" 2>/dev/null || true
kill $SP 2>/dev/null
kill -0 $PID 2>/dev/null || err "chrono crashed"
kill $PID 2>/dev/null; sleep 1

if grep -hE "is not installed|failed to load|module .* not found|Cannot load library|TypeError|ReferenceError|could not find the Qt platform plugin" \
	"$OUT/chrono.log" "$OUT/settings.log" | sort -u | head -20 | sed 's/^/  /' | grep .; then
	err "runtime errors above"
fi
[ $fail -eq 0 ] && echo "distro check passed"
exit $fail
