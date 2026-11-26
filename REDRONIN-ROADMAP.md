# DEsk × Red Ronin: roadmap (2026-11-25, revised)

What you reported after your first real session, what I measured, and the
order of the work. Each phase has acceptance checks, and it is done only
when they pass with evidence: frame traces, test output, screenshots.

## Measured in the live session
- **Idle:** DEsk uses about 23 % of a CPU core and 37 % of the GPU with
  nothing happening. The GPU idles at P5 / 510 MHz, so bursts stutter.
- **Nested and idle**, profiler on:
  - about 5 frames a second, each uploading about 1.2 MB;
  - some frames repaint the whole desk: 36 ms and 15.6 MB, on the main
    thread. That is the hitch during animations.
- **Quality:** the room renders at Ultra, 4 samples a pixel.
- **Screens:** at login only DP-2 and eDP-1 were connected; nothing was
  detected on the portrait monitor (check its cable or power). DEsk keeps
  arrangements in `displays.conf`, but arranging needs the external
  `wdisplays`.
- **Your Hyprland keys and DEsk's today:**
  | Keys | Hyprland | DEsk now |
  | --- | --- | --- |
  | Super+Shift+dir | move the window, across screens at the edge | swap |
  | Super+Alt+dir | swap | the same as Super+Shift |
  | Super+−/= (Shift: height) | resize | master ratio / gaps |
  | Super+V | flip the split | next layout |
  | Super+D | app launcher | the bookcase |
  | Super+G | groups | nothing |
  | Super+Alt+S | scratchpad | nothing |
- **Already in place:** Spotify and `playerctl` are installed, and DEsk
  reads MPRIS (`src/media.c`). The VPN is a NetworkManager connection.

## Phase 0: quick wins
1. **Default login:** SDDM preselects DEsk (a theme `defaultSession` key;
   remove it to go back to "last used").
2. **Your Hyprland keys, exactly:**
   - Super+D: the launcher;
   - Super+Shift+dir: move; Super+Alt+dir: swap;
   - Super+−/=: width; Super+Shift+−/=: height;
   - Super+V: flip the split.
   - The old DEsk meanings move to other keys: Super+B for the bookcase,
     Super+Ctrl+−/= for the ratio.
3. **Tap Super:** tapping Super on its own opens the launcher, most-used
   apps first.
4. **Profiler:** the frame profiler is on for your next login.
- **Checks:**
  - `desk-ctl binds` shows each key against its Hyprland meaning;
  - the greeter preview shows DEsk;
  - a trace file exists after login.

## Phase 1: smoothness (first and largest)
1. **Idle:** stop idle redraws and repaints unless something changed (the
   clock, lamp, room films, overlays, the tiling tick).
2. **The 36 ms repaints:** take them off the frame path; repaint only what
   changed, or paint off-thread and swap.
3. **Glides:** while a window glides, scale its picture, and draw its
   chrome once when it lands.
4. **Quality:** choose the `graphics` level that holds 60 Hz, and tune
   NVIDIA frame pacing.
- **Checks, from a frame trace on DP-2 at 2560×1440:**
  - idle CPU under 3 %, with 0 frames a second at idle;
  - no frame over 16.7 ms during a glide, stack switch or drag;
  - p99 under 12 ms.

## Phase 2: moving and tiling windows, by keyboard and by hand
1. **Your own layout:** the dwindle layout becomes a real split tree, as
   in Hyprland:
   - Super+Shift+dir moves a window to that side of its neighbour,
     splitting there;
   - Super+V flips a split;
   - a preselect key chooses where the next window opens.
   - Master, columns and monocle stay as presets.
2. **Across screens:** moving past the screen edge takes the window to the
   next screen, and Super+Ctrl+Shift+dir sends it there directly.
3. **The mouse:**
   - Super+left-drag moves any window, tiled or not; dropped on a tile,
     the two swap, with a brush-stroke outline previewing where it lands;
   - Super+right-drag resizes, which moves the split for a tiled window;
   - drag the gap between tiles to resize the split;
   - double-click a heading to float or tile.
4. **Groups and scratchpad:**
   - Super+G makes a tab group, shown as sheets clipped together with
     their tabs along the heading;
   - Super+Alt+S puts a window in the drawer as a scratchpad, and a key
     brings it out over the desk.
- **Checks:**
  - e2e tests drive a virtual pointer and keyboard through every move,
    swap, resize, split flip, cross-screen move and group;
  - the frame trace shows nothing over 16.7 ms while dragging.

## Phase 3: workspaces (stacks)
1. **Per screen:** each screen has its own stacks, as in Hyprland.
   Super+1…0 acts on the screen under the pointer or with focus; an
   option keeps today's global behaviour.
2. **Where you are:** a brushed numeral strip at each screen's edge shows
   which stacks hold sheets and which one you're on. It's clickable, and a
   window can be dropped onto a stack there.
3. **Overview:** an emakimono scroll unrolls with every stack as a
   thumbnail. Click one to go there, or drag a window between them.
4. **Behaviour:**
   - an empty stack is skipped by next/previous;
   - the previous-stack key (back-and-forth) works per screen;
   - a sent window can follow or stay.
- **Checks:** e2e tests per screen with two headless outputs, plus vision
  QA of the strip and the scroll.

## Phase 4: folding and unfolding
1. **Speed:** folding takes at most 200 ms, so it no longer stalls the
   desk.
2. **Where packets go:** folded sheets go to a tidy letter tray (a
   *fubako*) on each stack, not loose on the desk. Each packet shows the
   app's icon and title.
3. **Unfolding:** a sheet unfolds back to exactly where it was, its tile
   included. Keys:
   - Super+M: fold;
   - Super+Shift+M: unfold the last folded;
   - Super+Ctrl+M: pick from the tray with the keyboard.
   - A click on the packet unfolds it.
4. **Tiling:** the tiling closes the gap when a sheet folds, and reopens
   its place when it unfolds.
- **Checks:** e2e fold/unfold round trips keep position and tile; a frame
  trace stays under 16.7 ms; vision QA of the tray.

## Phase 5: arranging the screens
1. **Inside DEsk:** a screens sheet where each monitor is a pane of the
   right shape and size, dragged into place to snap edges. Per screen:
   rotation (for the portrait monitor), scale and refresh rate, and which
   screen has the desk.
2. **Remembered:** saved per set of connected monitors, in the existing
   `displays.conf`.
3. **Keys:**
   - Super+P cycles presets (extend, mirror, laptop only, externals only);
   - Super+Ctrl+arrows move focus between screens.
- **Checks:** e2e tests with two and three headless outputs (arrange,
  rotate, save, reconnect, restore); vision QA.

## Done since (2026-11-25/26, built and tested, not yet committed)
- **Smoothness:** a window's frame and shadow are stretched while it glides and
  drawn once where it lands (dropped frames over a test run: 25 -> 2).
- **Idle:** `screen_off_plugged` (default never): no dimming or locking on the mains.
- **Explicit sync:** DEsk's wlroots is patched
  (`packaging/redronin/wlroots-0.18.3-explicit-sync.patch`) to wait for
  finished client frames and release buffers behind a GPU fence; DEsk offers
  linux-drm-syncobj-v1. Fixes Chromium's flicker on NVIDIA. The temporary
  `~/.config/chromium-flags.conf` can go once it is checked.
- **3D stack changes** (`src/wstrans.c`, `src/wstrans.glsl`): katana, byobu,
  scroll (emakimono, chosen as the default), shoji; `ws_switch` in shell.conf.

## Done: the night and the storm (2026-11-26, installed, not committed)
- `src/storm.c`, `src/storm.glsl`: the night over the desk, a shoji window's
  light, lightning with thunder (`DESK_SND_THUNDER`), a cord that opens the
  window: rain heard (`DESK_SND_RAIN`, a seamless loop), streaks in the light,
  drops that splash and dry. Under the windows; idle stays at 2 frames a second.
- `storm`, `desk_night`, `rain_volume` in shell.conf; desk-ctl `window`, `lightning`.
- The room bake now runs at low GPU priority and in smaller slices (the minute
  after login); `~/.config/desk/environment` is read by desk-session (a frame
  trace is on for the next login).

## Next: the dojo entrance (lock screen and login)
The lock screen is still a Western front door (bricks, a panelled door, a brass
plate) that walks into a Western study. It becomes a dojo's entrance at night:
a wooden gate with shoji panels, a noren curtain, a stone lantern, rain on the
stones; the password on a wooden board; unlocking, the shoji slide open. The
SDDM greeter is made from the same scene (rendered by desk-preview) with the
same password board, so the login and the lock are one place.

## Phase 6: the Japanese / Ronin look, second pass
The desk itself is Japanese now, but what sits around it is still a
Western study. In a coherent design pass, re-make:
1. **The post box:** a Japanese red 〒 postbox; notifications come as
   *tanzaku* slips.
2. **Name plates:** the brass plates ("CONTROL PANEL", "ALL APPS") become
   wooden name boards (*hyōsatsu*) with brushed kanji and small Latin text
   under them.
3. **Drawer and bookcase:** a *tansu* chest and a *hondana*.
4. **Clock and Control Panel:** lacquer and brass to match the negoro
   desk.
5. **Lock screen:** shoji panels slide shut; unlocking slides them open.
6. **Sounds:** wooden *hyōshigi* clacks, a *rin* bell, paper rustle.
7. **Pointer and stamp:** a brush-tip cursor theme; the focus seal stays.
- **Checks:** a review against the Red Ronin palette, vision QA on both
  screens, no frame-time regression.

## Phase 7: the player (a real thing on the desk)
1. **The object:** a new thing like the calculator or phone, a cassette
   deck in the phase 6 style. It has:
   - play/pause, previous and next keys;
   - a volume knob;
   - a tape window you drag to seek;
   - the album art as its J-card;
   - track and artist on a VFD.
2. **Players:** it drives any MPRIS player and prefers Spotify; pressing
   play starts Spotify if it isn't running. It can be put in the drawer.
- **Checks:**
  - every control works against Spotify and against mpv;
  - no idle redraws except when the track or position changes;
  - vision QA in every state.

## Phase 8: VPN on the Control Panel
1. **Key switch:** a physical key switch that brings the VPN up or down by
   name with `nmcli` and lights when connected. It never reads or stores
   the VPN's settings or secrets.
2. **Shortcuts:** `desk-ctl do vpn` and a key.
- **Checks:** it follows the real state, including changes made outside
  DEsk; an error shows "off" with a note.

## Phase 9: more to touch (pick what you want)
- **Senkō timer:** an incense stick as a focus timer, with a bell.
- **Inkstone volume:** rub the inkstone to change the volume.
- **Brush box (fudebako):** pinned apps, dragged in to pin them.
- **Shishi-odoshi:** a bamboo clack for notifications.
- **Seal stamp:** drop a file on the hanko to copy its path or share it.

## Order and cost
Phases 0 and 1 first, because nothing feels good while frames drop. Each
phase then goes through:
1. build;
2. an independent review;
3. vision QA;
4. Docker and nested tests;
5. an install, which needs your sudo password in a window DEsk opens;
6. a commit.
Phases 2, 3 and 6 are the largest.
