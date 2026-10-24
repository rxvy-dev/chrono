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
# Firefox: a big real-world client (popups, subsurfaces, dmabuf/shm, activation, cursor shapes...)
if command -v firefox >/dev/null; then
    MOZ_ENABLE_WAYLAND=1 firefox --no-remote --new-instance --profile "$(mktemp -d)" "data:text/html,<h1>Chrono</h1><select><option>a</option><option>b</option></select>" >"$OUT/firefox.log" 2>&1 &
    FF=$!
    for i in $(seq 40); do chronoctl windows | grep -qi firefox && break; sleep 0.5; done
    sleep 3
    grim "$OUT/firefox.png"
    kill -0 $PID 2>/dev/null || { echo "::error::chrono crashed while Firefox started"; tail -60 "$OUT/chrono.log"; exit 1; }
    chronoctl windows > "$OUT/windows-firefox.json"
    FID=$(python3 -c 'import json,sys; w=[x for x in json.load(open(sys.argv[1])) if "irefox" in x["app_id"]]; print(w[0]["id"] if w else "")' "$OUT/windows-firefox.json")
    if [ -n "$FID" ]; then
        chronoctl maximize "$FID" >/dev/null; sleep 1; chronoctl maximize "$FID" >/dev/null; sleep 1
        chronoctl fullscreen "$FID" >/dev/null; sleep 1; chronoctl fullscreen "$FID" >/dev/null; sleep 1
        chronoctl minimize "$FID" >/dev/null; sleep 0.5; chronoctl focus "$FID" >/dev/null; sleep 0.5
        chronoctl close "$FID" >/dev/null; sleep 3
    else
        echo "::warning::Firefox didn't map a window"
    fi
    kill $FF 2>/dev/null; sleep 1
    kill -0 $PID 2>/dev/null || { echo "::error::chrono crashed with Firefox"; tail -60 "$OUT/chrono.log"; exit 1; }
fi

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
# right-clicks on the panel (bottom bar, 1080p): a taskbar button, then empty space
chronoctl pointer 5 5 >/dev/null; sleep 1        # gives the seat a pointer (no mouse on CI)
chronoctl pointer 5 5 left >/dev/null; sleep 0.4   # closes the power menu left open above
H=$(chronoctl outputs | python3 -c 'import json,sys; o=json.load(sys.stdin)[0]; print(o["height"])')
chronoctl pointer 260 $((H - 22)) right >/dev/null; sleep 0.8; grim "$OUT/shell-rightclick-task.png"
chronoctl pointer 5 5 left >/dev/null; sleep 0.4
chronoctl pointer 1200 $((H - 22)) right >/dev/null; sleep 0.8; grim "$OUT/shell-rightclick-panel.png"
chronoctl pointer 5 5 left >/dev/null; sleep 0.4
chronoctl pointer 28 $((H - 22)) left >/dev/null; sleep 0.8; grim "$OUT/shell-click-launcher.png"
chronoctl pointer 5 5 left >/dev/null; sleep 0.4

pgrep -x chrono-shell >/dev/null || { echo "::error::chrono-shell is not running"; fail=1; }
grep -q "Assertion" "$OUT/chrono.log" && { echo "::error::an assertion failed"; fail=1; }
grep -E "qrc:/.*(Error|error|TypeError|ReferenceError)" "$OUT/chrono.log" | sort -u | head -20 | sed "s/^/::warning::/"
grep -qE "qrc:/.*(TypeError|ReferenceError)" "$OUT/chrono.log" && { echo "::error::the shell hit a script error (see above)"; fail=1; }
chronoctl action "snap left" >/dev/null; sleep 0.4
grim "$OUT/shot-snap.png"
chronoctl action "maximize" >/dev/null; sleep 0.4
grim "$OUT/shot-maximize.png"
# close a window mid-way through its fade-out, then make sure the compositor is still fine
WID=$(chronoctl windows | python3 -c 'import json,sys; w=json.load(sys.stdin); print(w[0]["id"] if w else "")')
[ -n "$WID" ] && chronoctl close "$WID" >/dev/null && sleep 0.05 && grim "$OUT/shell-closing.png"; sleep 0.5
kill -0 $PID 2>/dev/null || { echo "::error::chrono crashed closing a window"; fail=1; }
chronoctl lock >/dev/null; sleep 1; grim "$OUT/shell-lock.png"
kill -0 $PID 2>/dev/null || { echo "::error::chrono crashed during the test"; fail=1; }
chronoctl quit >/dev/null 2>&1; sleep 1; kill $PID 2>/dev/null
grep -E "\[ERROR\]|AddressSanitizer|runtime error" "$OUT/chrono.log" | head -20 | sed 's/^/::warning::/'
exit $fail
