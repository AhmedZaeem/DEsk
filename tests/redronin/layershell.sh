#!/bin/sh
# layer-shell on a headless desk: fuzzel (overlay, the keys) and waybar (a bar
# keeping its zone clear), installed from Debian in the container. Run by
# tests/redronin/run.sh layer. Pictures and logs to /out.
set -u
fails=0
pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; fails=$((fails + 1)); }
apt-get update -qq >/dev/null 2>&1
apt-get install -y -qq --no-install-recommends waybar fuzzel dbus dbus-bin >/dev/null 2>&1 \
	|| { echo "FAIL: could not install waybar and fuzzel"; exit 1; }
# waybar wants a session bus, as it has in a real session
[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ] || exec dbus-run-session -- sh "$0" "$@"
export XDG_RUNTIME_DIR=/tmp/xdg XDG_CONFIG_HOME=/tmp/cfg XDG_CACHE_HOME=/tmp/desk-cache
export WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 LANG=C.UTF-8
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME/desk" "$XDG_CONFIG_HOME/waybar" "$XDG_CONFIG_HOME/fuzzel"
chmod 700 "$XDG_RUNTIME_DIR"
{ cat /work/packaging/redronin/shell.conf; printf 'lights_at_login = no\nweather = no\ngraphics = low\n'; } \
	> "$XDG_CONFIG_HOME/desk/shell.conf"
cp /work/packaging/redronin/fuzzel.ini "$XDG_CONFIG_HOME/fuzzel/fuzzel.ini"
printf '{"layer":"top","position":"bottom","height":30,"modules-left":["clock"]}\n' \
	> "$XDG_CONFIG_HOME/waybar/config"
desk --headless --size 1600x1000 > /out/ls-desk.log 2>&1 &
i=0; while ! ls "$XDG_RUNTIME_DIR"/desk-*.ctl >/dev/null 2>&1 && [ $i -lt 240 ]; do sleep 0.5; i=$((i+1)); done
export WAYLAND_DISPLAY=wayland-0
i=0; while ! grep -q "the desk, 1440x" /out/ls-desk.log && [ $i -lt 240 ]; do sleep 1; i=$((i+1)); done
sleep 3
for t in one two; do foot -T $t >/dev/null 2>&1 & done; sleep 5
h0=$(desk-ctl query | grep -o '"title":"one"[^}]*"h":[0-9]*' | grep -o '"h":[0-9]*' | cut -d: -f2)
waybar > /out/ls-waybar.log 2>&1 & sleep 6
h1=$(desk-ctl query | grep -o '"title":"one"[^}]*"h":[0-9]*' | grep -o '"h":[0-9]*' | cut -d: -f2)
grep -q 'comes over the desk: waybar' /out/ls-desk.log && pass "waybar is a layer surface" || fail "no waybar surface"
[ -n "$h0" ] && [ -n "$h1" ] && [ "$h1" -lt "$h0" ] && pass "tiled sheets clear of the bar ($h0 -> $h1)" \
	|| fail "exclusive zone not kept ($h0 -> $h1)"
grim /out/40-waybar.png
fuzzel > /out/ls-fuzzel.log 2>&1 & sleep 4
grep -q 'comes over the desk: launcher' /out/ls-desk.log && pass "fuzzel is a layer surface" || fail "no fuzzel surface"
[ "$(desk-ctl query | grep -o '"focused":true' | wc -l)" -eq 0 ] && pass "fuzzel has the keys" || fail "a window kept the keys"
grim /out/41-fuzzel.png
foot -T three >/dev/null 2>&1 & sleep 4
[ "$(desk-ctl query | grep -o '"focused":true' | wc -l)" -eq 0 ] && pass "a new window does not take the keys from fuzzel" \
	|| fail "a new window took the keys from an exclusive launcher"
pkill fuzzel; sleep 2
[ "$(desk-ctl query | grep -o '"focused":true' | wc -l)" -eq 1 ] && pass "keys back to a window when it goes" \
	|| fail "no window has the keys after fuzzel"
desk-ctl binds > /out/binds.txt; [ "$(wc -l < /out/binds.txt)" -gt 80 ] && pass "desk-ctl binds lists $(wc -l < /out/binds.txt)" \
	|| fail "binds: $(wc -l < /out/binds.txt)"
desk-ctl binds | fuzzel --dmenu --prompt "keys  " --width 70 --font "monospace:size=11" > /dev/null 2>&1 & sleep 4
grim /out/42-cheatsheet.png; pkill fuzzel
pkill waybar; sleep 3
h2=$(desk-ctl query | grep -o '"title":"one"[^}]*"h":[0-9]*' | grep -o '"h":[0-9]*' | cut -d: -f2)
[ "$h2" = "$h0" ] && pass "bar gone: sheets take the room back" || fail "after bar gone h=$h2 (was $h0)"
grep -i -E 'segfault|assert' /out/ls-desk.log >/dev/null && fail "crash in log" || pass "no crash in the log"
echo "== $fails failed"
exit "$fails"
