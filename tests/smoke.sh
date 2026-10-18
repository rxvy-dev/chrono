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
PATH="$PWD/$BUILD:$PWD/data:$PATH"
"$BUILD/chrono" --no-shell -d >"$OUT/chrono.log" 2>&1 &
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
chronoctl action "snap left" >/dev/null; sleep 0.4
grim "$OUT/shot-snap.png"
chronoctl action "maximize" >/dev/null; sleep 0.4
grim "$OUT/shot-maximize.png"
kill -0 $PID 2>/dev/null || { echo "::error::chrono crashed during the test"; fail=1; }
chronoctl quit >/dev/null 2>&1; sleep 1; kill $PID 2>/dev/null
grep -E "\[ERROR\]|AddressSanitizer|runtime error" "$OUT/chrono.log" | head -20 | sed 's/^/::warning::/'
exit $fail
