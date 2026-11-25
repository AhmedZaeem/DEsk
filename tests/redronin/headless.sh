#!/bin/sh
# Tiling, the stacks and desk-ctl, tried on a headless desk with real
# windows (foot), as tests/redronin/run.sh runs it in the Docker image.
# Pictures and the desk's log go to /out; each check prints PASS or FAIL,
# and the script exits non-zero if any failed.
set -u
fails=0
pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; fails=$((fails + 1)); }

export XDG_RUNTIME_DIR=/tmp/xdg XDG_CONFIG_HOME=/tmp/cfg XDG_CACHE_HOME=/tmp/desk-cache
export WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 XDG_SESSION_TYPE=wayland LANG=C.UTF-8
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME/desk" "$XDG_CACHE_HOME" /out
chmod 700 "$XDG_RUNTIME_DIR"
cat > "$XDG_CONFIG_HOME/desk/shell.conf" <<'EOF'
lights_at_login = no
weather = no
tiling = yes
tiling_gap = 8
graphics = low
ui_style = redronin
bind_defaults = redronin
bind = SUPER+Y exec true
bind = SUPER+nosuchkey close
EOF

desk --headless --size 1600x1000 > /out/desk.log 2>&1 &
desk_pid=$!
i=0
while ! ls "$XDG_RUNTIME_DIR"/desk-*.ctl >/dev/null 2>&1 && [ $i -lt 600 ]; do
	sleep 0.5; i=$((i + 1))
done
ctl=$(ls "$XDG_RUNTIME_DIR"/desk-*.ctl 2>/dev/null | head -n 1)
[ -S "$ctl" ] && pass "control socket at $ctl" || { fail "no control socket"; cat /out/desk.log; exit 1; }
[ "$(stat -c %a "$ctl")" = 600 ] && pass "socket is 0600" || fail "socket mode $(stat -c %a "$ctl")"
d=${ctl##*/desk-}; export WAYLAND_DISPLAY=${d%.ctl}

q() { desk-ctl query; }
count() { q | grep -o "$1" | wc -l; }
settle() { sleep "${1:-2}"; }
shot() { grim "/out/$1.png" && echo "     picture: $1.png"; }
wait_windows() {
	i=0
	while [ "$(count '"app_id"')" -lt "$1" ] && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
}

# the room is laid before the desk is up; wait for it to light
sleep 8
grep -q 'bind = SUPER+nosuchkey' /out/desk.log 2>/dev/null; grep -q 'no key called "nosuchkey"' /out/desk.log \
	&& pass "a bad bind is reported in the log" || fail "bad bind not reported"
q | grep -q '"binds":[1-9]' && pass "binds loaded: $(q | grep -o '"binds":[0-9]*')" || fail "no binds"
q | grep -q '"enso":1,' && pass "an empty stack 1 shows the enso" || fail "enso at start: $(q | grep -o '"enso":[0-9]*')"

for t in one two three; do foot -T "$t" >/dev/null 2>&1 & done
wait_windows 3; settle 3
[ "$(count '"tiled":true')" -eq 3 ] && pass "3 windows tiled (master)" || fail "tiled: $(count '"tiled":true')"
q | grep -q '"enso":0,' && pass "the enso goes when windows open" || fail "enso with windows: $(q | grep -o '"enso":[0-9]*')"
q > /out/q-master.json; shot 01-master

desk-ctl do layout dwindle >/dev/null; settle
q | grep -q '"layout":"dwindle"' && pass "layout dwindle" || fail "layout not dwindle"; shot 02-dwindle
desk-ctl do layout columns >/dev/null; settle; shot 03-columns
desk-ctl do layout master >/dev/null
desk-ctl do ratio 0.65 >/dev/null; settle
q | grep -q '"ratio":0.650' && pass "ratio set" || fail "ratio"; shot 04-ratio

# focus by direction: the master is the first; right goes into the stack
desk-ctl do cycle next >/dev/null; settle 1
desk-ctl do focus left >/dev/null; settle 1
f1=$(q | grep -o '"title":"[a-z]*"[^}]*"focused":true' | grep -o '"title":"[a-z]*"')
desk-ctl do focus right >/dev/null; settle 1
f2=$(q | grep -o '"title":"[a-z]*"[^}]*"focused":true' | grep -o '"title":"[a-z]*"')
[ -n "$f1" ] && [ -n "$f2" ] && [ "$f1" != "$f2" ] && pass "focus left/right: $f1 -> $f2" || fail "focus: [$f1] [$f2]"
desk-ctl do focus left >/dev/null 2>&1; desk-ctl do focus left >/dev/null 2>&1
desk-ctl do focus left > /dev/null 2>&1; rc=$?
[ $rc -ne 0 ] && pass "nothing left of the master (exit $rc)" || fail "focus left of master succeeded"

# swap: the master moved into the stack
master_before=$(q | grep -o '"title":"[a-z]*"[^}]*"x":[0-9]*' | sort -t: -k4 -n | head -1)
desk-ctl do move right >/dev/null; settle
q > /out/q-swapped.json; shot 05-swapped
desk-ctl do promote >/dev/null 2>&1; settle 1

# float one out, and back
desk-ctl do float >/dev/null; settle
[ "$(count '"tiled":true')" -eq 2 ] && [ "$(count '"floating":true')" -eq 1 ] && pass "float: 2 tiled, 1 floating" \
	|| fail "float: tiled $(count '"tiled":true') floating $(count '"floating":true')"
shot 06-floating
desk-ctl do float >/dev/null; settle
[ "$(count '"tiled":true')" -eq 3 ] && pass "back into the tiling" || fail "unfloat"

# the stacks
desk-ctl do workspace 2 >/dev/null; settle
q | grep -q '"workspace":2,' && pass "on stack 2" || fail "not on stack 2"
[ "$(count '"hidden":true')" -eq 3 ] && pass "stack 1's windows hidden" || fail "hidden: $(count '"hidden":true')"
q | grep -q '"enso":2,' && pass "empty stack 2 shows its enso" || fail "enso on 2: $(q | grep -o '"enso":[0-9]*')"
shot 07-ws2-empty
foot -T four >/dev/null 2>&1 &
wait_windows 4; settle 3
q | grep -q '"title":"four"[^}]*"workspace":2' && pass "new window on stack 2" || fail "four not on 2"
q | grep -q '"title":"four"[^}]*"x":[0-9]*,"y":[0-9]*,"w":1[0-9][0-9][0-9]' && pass "alone, tiled full width" || fail "four not full width"
shot 08-ws2-one
desk-ctl do movetoworkspacesilent 1 >/dev/null; settle
[ "$(count '"hidden":true')" -eq 4 ] && pass "sent to stack 1 silently: all hidden here" || fail "silent move: hidden $(count '"hidden":true')"
desk-ctl do workspace previous >/dev/null; settle 3
q | grep -q '"workspace":1,' && pass "back to stack 1" || fail "previous"
[ "$(count '"hidden":true')" -eq 0 ] && [ "$(count '"tiled":true')" -eq 4 ] && pass "4 windows tiled on stack 1" \
	|| fail "stack 1: hidden $(count '"hidden":true') tiled $(count '"tiled":true')"
q > /out/q-ws1.json; shot 09-ws1-four

# tiling off: back where they were
desk-ctl do tiling off >/dev/null; settle
[ "$(count '"tiled":true')" -eq 0 ] && pass "tiling off releases all" || fail "tiling off"
shot 10-tiling-off
desk-ctl do tiling on >/dev/null; settle
[ "$(count '"tiled":true')" -eq 4 ] && pass "tiling on again" || fail "tiling on"

# a program asking for its own window maximised is tiled instead (kitty does)
foot --maximized -T big >/dev/null 2>&1 &
wait_windows 5; settle 3
q | grep -q '"title":"big"[^}]*"tiled":true[^}]*"maximized":false' && pass "a maximised request is tiled" \
	|| fail "maximised request: $(q | grep -o '"title":"big"[^}]*')"
desk-ctl do maximize >/dev/null; settle
q | grep -q '"title":"big"[^}]*"maximized":true' && pass "the maximise key still opens it out" || fail "maximize key"
desk-ctl do close >/dev/null; settle 3

# errors say so
desk-ctl do teleport >/dev/null 2>&1; [ $? -ne 0 ] && pass "unknown action refused" || fail "unknown action accepted"
desk-ctl do workspace 11 >/dev/null 2>&1; [ $? -ne 0 ] && pass "workspace 11 refused" || fail "ws 11 accepted"

# close through the socket, and the rest close up
desk-ctl do close >/dev/null; settle 3
[ "$(count '"tiled":true')" -eq 3 ] && pass "closed one: 3 left tiled" || fail "after close: $(count '"tiled":true')"
shot 11-after-close

kill "$desk_pid" 2>/dev/null; wait "$desk_pid" 2>/dev/null
[ ! -e "$ctl" ] && pass "socket removed at exit" || fail "socket left behind"
grep -c -i -E 'assert|segfault|AddressSanitizer' /out/desk.log | grep -q '^0$' && pass "no crash in the log" || fail "crash in log"
echo "== $fails failed"
exit "$fails"
