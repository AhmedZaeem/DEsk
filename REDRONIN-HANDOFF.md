# DEsk × Red Ronin: status

Worktree `~/Projects/DEsk-redronin`, branch `redronin`, based on upstream
`afa666c` (v0.2.1). Full guide: `docs/TILING.md`.

## Built and tested in Docker
- **Tiling:** master, dwindle, columns and monocle layouts.
- **Workspaces:** ten stacks.
- **Key binds:** `bind` lines in `shell.conf`, plus the Red Ronin set.
- **Control socket:** `desk-ctl do`, `query` and `binds`.
- **Layer-shell:** waybar, fuzzel, rofi and other bar/launcher programs run
  on the desk; bars keep their exclusive zones, and the keyboard and
  pointer are routed to them correctly.
- **`ui_style = redronin`:**
  - a vermilion hanko (浪) seal on the focused sheet;
  - the current stack on the clock in daiji (壱 弐 参 …).
- **`negoro` desk wood**, black urushi lacquer worn through to vermilion.
- **Preset:** `packaging/redronin/shell.conf`, plus `fuzzel.ini`. Super+A
  opens the launcher; Super+/ opens the key cheat sheet.
- **Tests:**
  - `meson test`: 5 suites;
  - `tests/redronin/run.sh`: 27 checks;
  - `tests/redronin/run.sh layershell`: 9 checks.
- **Reviews:** two independent reviews; every crash and behaviour finding
  has been fixed.

## Installed on the workstation (2026-11-25)
- `DESK_SUMS_CHECKED=1 sh scripts/install_arch.sh --no-apps`, run from a
  terminal outside the Claude app (its sandbox blocks sudo). The checksum
  skip is needed because SHA256SUMS describes upstream's files.
- `/usr/local` (desk), `/opt/desk-wlroots` (wlroots 0.18.3), "DEsk" on the
  login screen; the preset and `fuzzel.ini` are in `~/.config/desk/`.
- Rebuild after a change: `ninja -C build-install` (with
  `/opt/desk-wlroots/bin` on PATH), then
  `sudo meson install -C build-install --no-rebuild`.
- Tried nested in Hyprland on the NVIDIA GPU with the real config:
  tiling, seal, daiji clock, stacks and fuzzel all work. That run found
  kitty asking to be maximised (its remembered size); while tiling is on,
  such a request is now declined and the sheet tiled.

## Not done
- A full login session on the three monitors, real mouse drags, and
  drag-and-drop icons over a bar.
- `android-studio` and `ronin-studio` (Super+Alt+A, Super+R) are not on
  PATH, in Hyprland either.
- Popups of a bottom-layer bar are stacked under the windows.
- The room cache is keyed per wood index.
