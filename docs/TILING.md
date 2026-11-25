# Red Ronin: tiling, stacks, key binds and the look

Added for Red Ronin OS. None of it changes the desk until `shell.conf` asks:
with no `tiling`, `bind` or `bind_defaults` line, DEsk behaves as upstream
(the stacks exist, but you only notice them once you change to another one).

The Red Ronin preset is `packaging/redronin/shell.conf`. Copy it to
`~/.config/desk/shell.conf` (and `fuzzel.ini` beside it, which the launcher
binds load), or add its lines to your own.

## Stacks (workspaces)

There are ten stacks of paper. A new window joins the stack the desk is on.
Changing stack hides the other stacks' sheets and their folded packets, and
brings the new stack's back exactly as they were. Sheets put away in the
drawer belong to every stack, and so does a sheet that is **pinned**.

Stacks are global, not per screen: changing stack changes every screen
together.

## Tiling

With `tiling = yes`, the open sheets of the current stack are laid out on
each screen in the order they arrived, and glide into place. Four layouts:

| layout | |
| --- | --- |
| `master` | one large sheet on the left (on top when the screen is portrait), the rest stacked beside it |
| `dwindle` | each sheet takes half of what is left, the split turning as it goes (as Hyprland's dwindle) |
| `columns` | side by side, all the same width |
| `monocle` | every sheet over the whole area, the front one seen |

These stay out of the tiling and float:
- dialogs and fixed-size windows;
- the desk's things (calculator, phone, notebook);
- picture-in-picture windows;
- folded, fullscreen and opened-out (maximised) sheets;
- sheets you float yourself.

A program that asks for its own window maximised (kitty does, when the size it remembers is too big) is tiled instead; the maximise key still opens a sheet out. A tiled sheet floats if you drag it away or pull one of its edges. A plain click on its heading leaves it tiled. In `monocle`, focus left/up and right/down go through the sheets in order. A window that asks for attention (a link opening in the browser) takes the desk to its stack.
`tiling off` puts every sheet back where it was before tiling took it.

## Keys

`bind = KEYS ACTION`, one line per bind. A later line for the same keys
replaces an earlier one. Binds are tried before the desk's own keys, so a
bind can take over Super+L, Super+V and so on, and `none` turns a key off.

    bind = SUPER+T exec kitty
    bind = SUPER+SHIFT+H move left
    bind = SUPER SHIFT, H, movewindow, l      # Hyprland's form works too
    bind = SUPER+V none

Modifiers are `SUPER`, `SHIFT`, `CTRL` and `ALT`; a key with no modifier can be bound only if it is a function key, a media key or Print, so ordinary typing is never taken. The key is any xkb name:
`Return`, `grave`, `minus`, `F4`, `1`. Super+Shift+1 is written as `1`, not
`exclam`.

`bind_defaults = redronin` adds the Red Ronin set underneath your own binds;
yours always win. The set matches `~/.config/hypr/ronin-binds.lua`:

| keys | action |
| --- | --- |
| Super+T, Super+Return / Super+E / Super+B | terminal / file manager / browser (the desk's standard programs) |
| Super+D, Super+Shift+D | the bookcase (apps), the drawer |
| Super+Q, Super+F, Super+Shift+F | close, fullscreen, maximise |
| Super+H J K L (or the arrows) | focus that way |
| Super+Shift+H J K L (or the arrows), Super+Alt+H J K L | move that way: swap places when tiled; left half, right half, maximise or restore when floating |
| Super+Shift+Return | promote to master |
| Super+Shift+V, Super+W, Super+V | float / tile this sheet; tiling on / off; next layout |
| Super+minus / equal, Super+Shift+minus / equal | master ratio −/+ 0.05, gaps −/+ 2 pt |
| Super+1…0, Super+Shift+1…0, Super+Alt+1…0 | go to a stack; send a sheet there and follow; send it there and stay |
| Super+grave, Super+Ctrl+Left / Right (H / L) | the previous stack; the stack before / after |
| Super+Tab, Super+Shift+Tab | next / previous sheet in tiling order |
| Super+P, Super+Alt+C, Super+M | pin to every stack, centre, fold |
| Super+Alt+V, Super+N, Super+C | clipboard, post box, Control Panel |
| Super+Shift+S, Super+Ctrl+Escape, Super+Escape | snip, lock, log out |
| Super+Shift+R | read shell.conf again |

### Actions

`exec CMD`, `none`, `close`, `fullscreen`, `maximize`, `fold`, `float`,
`pin`, `center`, `promote`, `tiling on|off|toggle`,
`layout master|dwindle|columns|monocle|next`, `ratio +0.05|0.6`,
`gap +2|8`, `focus left|right|up|down`, `move left|right|up|down`,
`cycle next|prev`, `workspace 1-10|next|prev|previous`,
`movetoworkspace N`, `movetoworkspacesilent N`, `terminal`, `browser`,
`files`, `editor`, `drawer`, `shelf`, `console`, `post`, `lock`,
`logout`, `snip`, `clipboard`, `switcher`, `reload`.

Hyprland's names are accepted where they mean the same thing:
`killactive`, `togglefloating`, `movefocus`, `movewindow`, `swapwindow`,
`togglesplit`, `splitratio`, `exit`, `cyclenext`, `swapmaster`, and others.

## desk-ctl

    desk-ctl do workspace 3
    desk-ctl do layout dwindle
    desk-ctl query            # stack, tiling and every window, as JSON
    desk-ctl binds            # every key bind in force, one a line

The control socket is `$XDG_RUNTIME_DIR/desk-$WAYLAND_DISPLAY.ctl`, mode
0600. `do` exits 0 when the action worked, 3 when it could not be done here
(for example, focus left from the leftmost sheet), and 1 when DEsk is not
running. A bad action prints why on stderr.

## shell.conf keys

| key | values | default |
| --- | --- | --- |
| `tiling` | `yes`, `no` | `no` |
| `tiling_layout` | `master`, `dwindle`, `columns`, `monocle` | `master` |
| `tiling_ratio` | `0.1` … `0.9` | `0.55` |
| `tiling_gap` | points, `0` … `64` | `6` |
| `workspaces_wrap` | `yes`, `no` (whether next from 10 goes to 1) | `yes` |
| `bind_defaults` | `desk`, `redronin` | `desk` |
| `bind` | `KEYS ACTION` (repeatable) | |

## The Red Ronin look

`ui_style = redronin` makes the desk a calligrapher's (書道の机):
- **Washi.** The sheets are washi: the kozo fibres show in their headings,
  and the rule under the heading is one stroke of sumi, pressed down at the
  left, lifted at the right, where the brush runs dry.
- **The seal.** The sheet that has the keys is stamped with a vermilion
  hanko at the left of its heading, and its stroke is in the same
  vermilion. The seal is 浪, the *rō* of 浪人 (rōnin), with the character
  cut out so the paper shows through (*hakubun*). Each window's seal is
  pressed a little differently, and the same way every time that window is
  drawn. The other sheets carry no seal: in this look, the seal *is* focus.
- **The stack on the clock.** The clock's TIME page shows which stack you
  are on, in the formal numerals of a ledger (*daiji*):
  壱 弐 参 肆 伍 陸 漆 捌 玖 拾. Its box is drawn whole while the sheets are
  tiled, and dashed while they lie loose.
- **An empty stack.** A stack with nothing open shows a sheet of *hanshi*,
  the practice paper of shodō, in the middle of the desk, with an *ensō*
  brushed on it: the circle drawn in one breath, open where the brush left
  the paper, dry at its end. The stack's numeral is inside it, and 浪人 and
  the seal are at its side. It goes as soon as anything is opened there.

With it, lay the desk with `desk_mat = shitajiki` and `desk_mat_size =
full` (the preset does): the indigo-black felt a calligrapher lays the
paper on, printed with faint guide squares and the corners of a hanshi
sheet.

The clock shows the stack as soon as you use any of these features, even
without `ui_style`.

## Bars, launchers and notices (layer-shell)

DEsk now speaks wlr-layer-shell, so rofi (Wayland), fuzzel, waybar, wlogout
and other layer-shell programs run on it:
- **background** and **bottom** surfaces sit over the traced room, under
  what stands on the desk;
- **top** surfaces sit over the windows, but under a fullscreen one;
- **overlay** surfaces sit over everything except the lock screen.

A bar's exclusive zone is kept clear of windows, and the tiling makes room
for it. Only the top and overlay layers take the pointer, and they get it
before the desk does. A surface that asks for the keys gets them while it
is up; when it goes, the keys return to the window in front. DEsk's post
box remains the notification daemon, so swaync and mako cannot claim the
notification bus beside it.

The preset binds **Super+A** to fuzzel. `packaging/redronin/fuzzel.ini`
themes it as a washi slip laid over the sheets: sumi ink, a vermilion
selection and 浪 as the prompt. **Super+/** shows every key in force (from
`desk-ctl binds`) in the same slip. The bookcase on Super+D stays the
desk's own way to every program.

## Desk wood: negoro

`desk_wood = negoro` is *negoro-nuri* (根来塗): black urushi lacquer over
vermilion, worn through where hands have rubbed it. It is the Red Ronin
palette's ink-black and crimson as a real material. Like ebony, it takes no
brown or dark stain.

## Tests

- `meson test` runs `tile`, which checks the layouts, directional focus,
  action and bind parsing, and the keys.
- `tests/redronin/run.sh` builds this tree in Docker, starts a headless desk
  with real `foot` windows, drives it with `desk-ctl`, checks the JSON and
  takes screenshots into `out/redronin/`. `run.sh layershell` does the
  same with waybar and fuzzel installed from Debian.

## Known limits

- Stacks are global across screens.
- There is no on-screen stack indicator yet; use `desk-ctl query`.
- The traced room is cached per wood, not per wood colour: after changing a
  wood's colours in `draw.c`, clear `~/.cache/desk/room-*`.
