#!/bin/bash
# Start chrono on the headless backend, open some windows, switch through every theme and
# screenshot each one. Results (log + PNGs + IPC replies) land in $OUT.
set -u
BUILD=${1:-build}
OUT=${2:-results}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
export XDG_RUNTIME_DIR=$(mktemp -d) WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=1
export WLR_LIBINPUT_NO_DEVICES=1 CHRONO_THEME_DIR=$PWD/themes CHRONO_CONFIG=$OUT/test.conf
export HOME=$(mktemp -d)
chmod 700 "$XDG_RUNTIME_DIR"
printf '[general]\ntheme = midnight\n[power]\nlock_after = 0\nscreen_off_after = 0\n' > "$CHRONO_CONFIG"
PATH="$PWD/$BUILD:$PWD/$BUILD/shell:$PWD/$BUILD/settings:$PWD/data:$PATH"
export QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=software XDG_CURRENT_DESKTOP=Chrono QT_FORCE_STDERR_LOGGING=1
SHELLFLAG=""; [ -x "$BUILD/shell/chrono-shell" ] || SHELLFLAG=--no-shell
"$BUILD/chrono" $SHELLFLAG -d >"$OUT/chrono.log" 2>&1 &
PID=$!
sock=""
for i in $(seq 50); do
    sock=$(ls "$XDG_RUNTIME_DIR"/wayland-* 2>/dev/null | grep -v lock | head -1)
    [ -n "$sock" ] && break
    sleep 0.2
done
if ! kill -0 $PID 2>/dev/null || [ -z "$sock" ]; then
    echo "::error::chrono did not start"; tail -50 "$OUT/chrono.log"; exit 1
fi
export WAYLAND_DISPLAY=$(basename "$sock")
sleep 1
fail=0
chronoctl ping >"$OUT/ping.json" || { echo "::error::chronoctl ping failed"; fail=1; }
# a realistic 1080p screen for the screenshots
chronoctl output HEADLESS-1 mode=1920x1080 >/dev/null; sleep 1
chronoctl outputs >"$OUT/outputs.json"
chronoctl themes >"$OUT/themes.json"
# real clients: foot uses xdg-shell and asks for server-side decorations
foot --title "Terminal" sh -c 'fastfetch 2>/dev/null || uname -a; sleep 600' >"$OUT/foot1.log" 2>&1 &
sleep 1.5
foot --title "Files" sh -c 'ls -la /usr; sleep 600' >"$OUT/foot2.log" 2>&1 &
sleep 2
chronoctl windows >"$OUT/windows.json"
grep -q '"id"' "$OUT/windows.json" || { echo "::error::no windows mapped"; fail=1; }
for t in $(cd themes && ls *.theme | sed 's/\.theme$//'); do
    chronoctl theme "$t" >/dev/null || { echo "::error::theme $t failed to apply"; fail=1; }
    sleep 0.6
    grim "$OUT/shot-$t.png" || { echo "::error::grim failed for $t"; fail=1; }
    kill -0 $PID 2>/dev/null || { echo "::error::chrono crashed applying theme $t"; fail=1; break; }
done
# the shell: launcher, power menu, calendar + a notification, OSD
chronoctl theme midnight >/dev/null; sleep 1
command -v notify-send >/dev/null && notify-send -a "Smoke test" "Hello from CI" "This is a <b>Chrono</b> notification." && sleep 0.8
grim "$OUT/shell-notification.png"
# every theme: launcher open, two windows, a notification -> docs/screenshots material
NID=""
for t in $(cd themes && ls *.theme | sed 's/\.theme$//'); do
    chronoctl theme $t >/dev/null; sleep 1.2
    # gdbus instead of notify-send: notify-send waits for an answer when a notification has actions
    NID=$(timeout 3 gdbus call --session --dest org.freedesktop.Notifications --object-path /org/freedesktop/Notifications \
        --method org.freedesktop.Notifications.Notify "Mail" "${NID:-0}" "mail-unread" "Lunch on Friday?" \
        "Sam: the new place by the river, 12:30 works for everyone?" '["open", "Open", "later", "Later"]' '{}' 30000 2>/dev/null \
        | sed -n 's/^(uint32 \([0-9]*\),)$/\1/p')
    sleep 0.6
    chronoctl action launcher >/dev/null; sleep 1.0
    grim "$OUT/theme-$t.png"
    chronoctl action launcher >/dev/null; sleep 0.4
done
chronoctl theme midnight >/dev/null; sleep 1
chronoctl action notifications >/dev/null; sleep 1; grim "$OUT/shell-calendar.png"; chronoctl action notifications >/dev/null; sleep 0.4
chronoctl action power-menu >/dev/null; sleep 1; grim "$OUT/shell-power.png"
chronoctl action power-menu >/dev/null; sleep 0.3
# the settings app, a couple of pages
if command -v chrono-settings >/dev/null; then
    for p in themes displays time; do
        chrono-settings $p >"$OUT/settings-$p.log" 2>&1 & SP=$!; sleep 2.5; grim "$OUT/settings-$p.png"; kill $SP 2>/dev/null; wait $SP 2>/dev/null
    done
    grep -h "qrc:" "$OUT"/settings-*.log | sort -u | head -20 | sed 's/^/::warning::/'
fi
chronoctl action "volume up" >/dev/null; sleep 0.5; grim "$OUT/shell-osd.png"
pgrep -x chrono-shell >/dev/null || { echo "::error::chrono-shell is not running"; fail=1; }
grep -q "Assertion" "$OUT/chrono.log" && { echo "::error::an assertion failed"; fail=1; }
grep -E "qrc:/.*(Error|error|TypeError|ReferenceError)" "$OUT/chrono.log" | sort -u | head -20 | sed "s/^/::warning::/"
chronoctl action "snap left" >/dev/null; sleep 0.4
grim "$OUT/shot-snap.png"
chronoctl action "maximize" >/dev/null; sleep 0.4
grim "$OUT/shot-maximize.png"
chronoctl lock >/dev/null; sleep 1; grim "$OUT/shell-lock.png"
kill -0 $PID 2>/dev/null || { echo "::error::chrono crashed during the test"; fail=1; }
chronoctl quit >/dev/null 2>&1; sleep 1; kill $PID 2>/dev/null
grep -E "\[ERROR\]|AddressSanitizer|runtime error" "$OUT/chrono.log" | head -20 | sed 's/^/::warning::/'
exit $fail
