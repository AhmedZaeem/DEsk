/* meson test: tiling's layouts, the way to the next sheet, actions, binds
 * and shell.conf's keys for them (src/tile.c). */

#include "tile.h"

#include <stdio.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

static int g_failed;

#define CHECK(cond, ...) do { \
	if (!(cond)) { \
		g_failed++; \
		fprintf(stderr, "tile-test:%d: ", __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		fputc('\n', stderr); \
	} \
} while (0)

static bool overlap(struct tile_box a, struct tile_box b)
{
	return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static bool inside(struct tile_box in, struct tile_box out)
{
	return in.x >= out.x && in.y >= out.y && in.x + in.w <= out.x + out.w
		&& in.y + in.h <= out.y + out.h;
}

/* every layout, for 1 .. 12 sheets on a wide and a tall screen: inside the
 * area, clear of the gap at the edge, none on another (but monocle's) */
static void layouts(void)
{
	struct tile_box areas[] = { { 0, 0, 1920, 1080 }, { 1920, 40, 1080, 1880 }, { 0, 0, 300, 200 } };
	for (size_t a = 0; a < sizeof areas / sizeof areas[0]; a++) {
		for (int l = 0; l < TILE_LAYOUT_COUNT; l++) {
			for (int n = 1; n <= 12; n++) {
				struct tile_box out[12];
				int gap = 8;
				tile_arrange((enum tile_layout)l, areas[a], n, 0.55, gap, out);
				struct tile_box in = { areas[a].x + gap, areas[a].y + gap, areas[a].w - 2 * gap,
					areas[a].h - 2 * gap };
				for (int i = 0; i < n; i++) {
					CHECK(out[i].w >= 1 && out[i].h >= 1, "%s n=%d box %d empty",
						tile_layout_name(l), n, i);
					CHECK(inside(out[i], in), "%s n=%d box %d (%d,%d %dx%d) outside",
						tile_layout_name(l), n, i, out[i].x, out[i].y, out[i].w, out[i].h);
					for (int j = 0; j < i && l != TILE_LAYOUT_MONOCLE && a < 2; j++) {
						CHECK(!overlap(out[i], out[j]), "%s n=%d: %d over %d", tile_layout_name(l),
							n, i, j);
					}
				}
			}
		}
	}
	/* master: the first takes the ratio, the rest share the other side evenly */
	struct tile_box out[4];
	tile_arrange(TILE_LAYOUT_MASTER, (struct tile_box){ 0, 0, 1000, 600 }, 3, 0.6, 0, out);
	CHECK(out[0].x == 0 && out[0].w == 600 && out[0].h == 600, "master %d %d", out[0].w, out[0].h);
	CHECK(out[1].x == 600 && out[1].w == 400 && out[1].h == 300, "stack 1 %d,%d %dx%d", out[1].x,
		out[1].y, out[1].w, out[1].h);
	CHECK(out[2].y == 300 && out[2].h == 300, "stack 2 %d %d", out[2].y, out[2].h);
	/* one alone has all of it */
	tile_arrange(TILE_LAYOUT_DWINDLE, (struct tile_box){ 10, 20, 800, 600 }, 1, 0.5, 4, out);
	CHECK(out[0].x == 14 && out[0].y == 24 && out[0].w == 792 && out[0].h == 592, "alone");
	/* columns: as wide as each other, give or take the odd pixel */
	tile_arrange(TILE_LAYOUT_COLUMNS, (struct tile_box){ 0, 0, 1000, 500 }, 3, 0.5, 10, out);
	CHECK(out[0].w == 320 && out[1].w == 320 && out[2].x + out[2].w == 990, "columns %d %d",
		out[0].w, out[2].x + out[2].w);
	/* a ratio past the ends is held to them */
	tile_arrange(TILE_LAYOUT_MASTER, (struct tile_box){ 0, 0, 1000, 500 }, 2, 5.0, 0, out);
	CHECK(out[0].w == 900, "ratio held %d", out[0].w);
}

static void ways(void)
{
	/* a master on the left, two stacked on the right */
	struct tile_box b[3] = { { 0, 0, 600, 600 }, { 600, 0, 400, 300 }, { 600, 300, 400, 300 } };
	CHECK(tile_pick_dir(b, 3, 0, TILE_RIGHT) == 1, "master right -> top of stack");
	CHECK(tile_pick_dir(b, 3, 1, TILE_LEFT) == 0, "stack left -> master");
	CHECK(tile_pick_dir(b, 3, 2, TILE_LEFT) == 0, "lower stack left -> master");
	CHECK(tile_pick_dir(b, 3, 1, TILE_DOWN) == 2, "stack down");
	CHECK(tile_pick_dir(b, 3, 2, TILE_UP) == 1, "stack up");
	CHECK(tile_pick_dir(b, 3, 0, TILE_LEFT) == -1, "nothing left of the master");
	CHECK(tile_pick_dir(b, 3, 0, TILE_UP) == -1, "nothing over the master");
	enum tile_dir d;
	CHECK(tile_dir_parse("l", &d) && d == TILE_LEFT, "l is left, as Hyprland has it");
	CHECK(tile_dir_parse("Right", &d) && d == TILE_RIGHT, "Right");
	CHECK(!tile_dir_parse("sideways", &d), "sideways");
}

static void actions(void)
{
	struct tile_action a;
	char err[160];
	CHECK(tile_action_parse("focus left", &a, err, sizeof err) && a.act == TILE_ACT_FOCUS
		&& a.dir == TILE_LEFT, "focus left: %s", err);
	CHECK(tile_action_parse("movefocus l", &a, err, sizeof err) && a.act == TILE_ACT_FOCUS
		&& a.dir == TILE_LEFT, "Hyprland's movefocus l: %s", err);
	CHECK(tile_action_parse("workspace 10", &a, err, sizeof err) && a.arg == 10, "workspace 10");
	CHECK(!tile_action_parse("workspace 11", &a, err, sizeof err), "workspace 11");
	CHECK(tile_action_parse("workspace previous", &a, err, sizeof err)
		&& a.arg == TILE_WS_BACK, "previous");
	CHECK(tile_action_parse("exec kitty -e btop  ", &a, err, sizeof err)
		&& a.act == TILE_ACT_EXEC && !strcmp(a.cmd, "kitty -e btop"), "exec: [%s]", a.cmd);
	CHECK(!tile_action_parse("exec", &a, err, sizeof err), "exec alone");
	CHECK(tile_action_parse("ratio +0.05", &a, err, sizeof err) && !a.abs && a.num > 0.049,
		"ratio step");
	CHECK(tile_action_parse("ratio 0.6", &a, err, sizeof err) && a.abs && a.num > 0.59, "ratio");
	CHECK(tile_action_parse("layout dwindle", &a, err, sizeof err)
		&& a.arg == TILE_LAYOUT_DWINDLE, "layout");
	CHECK(tile_action_parse("tiling", &a, err, sizeof err) && a.arg == -1, "tiling toggles");
	CHECK(!tile_action_parse("close now", &a, err, sizeof err), "close takes nothing");
	CHECK(!tile_action_parse("fly away", &a, err, sizeof err) && strstr(err, "fly"), "unknown");
}

static void binds(void)
{
	uint32_t mods, sym;
	char err[160];
	CHECK(tile_combo_parse("SUPER+SHIFT+H", &mods, &sym, err, sizeof err)
		&& mods == (TILE_MOD_LOGO | TILE_MOD_SHIFT) && sym == XKB_KEY_h, "super+shift+h");
	CHECK(tile_combo_parse("super+Return", &mods, &sym, err, sizeof err) && sym == XKB_KEY_Return,
		"return");
	CHECK(tile_combo_parse("ALT+F4", &mods, &sym, err, sizeof err) && mods == TILE_MOD_ALT
		&& sym == XKB_KEY_F4, "alt+f4");
	CHECK(!tile_combo_parse("SUPER+SHIFT", &mods, &sym, err, sizeof err), "no key");
	CHECK(!tile_combo_parse("SUPER+nosuchkey", &mods, &sym, err, sizeof err), "no such key");
	CHECK(!tile_combo_parse("Escape", &mods, &sym, err, sizeof err), "a bare Escape is refused");
	CHECK(tile_combo_parse("F9", &mods, &sym, err, sizeof err) && mods == 0, "a bare F9 is fine");
	CHECK(tile_combo_parse("XF86AudioPlay", &mods, &sym, err, sizeof err), "a media key alone");
	{
		char longcmd[400] = "exec ";
		memset(longcmd + 5, 'x', 380);
		longcmd[385] = '\0';
		struct tile_action la;
		CHECK(!tile_action_parse(longcmd, &la, err, sizeof err), "an over-long command is refused");
	}

	struct tile_conf c;
	tile_conf_defaults(&c);
	CHECK(!c.tiling && c.n_binds == 0, "off as made");
	CHECK(tile_conf_key(&c, "bind", "SUPER+T exec kitty", err, sizeof err) && !err[0], "%s", err);
	CHECK(tile_conf_key(&c, "bind", "SUPER SHIFT, H, movewindow, l", err, sizeof err) && !err[0],
		"Hyprland's form: %s", err);
	CHECK(tile_conf_key(&c, "bind", "SUPER+E exec foot -e sh -c 'a, b'", err, sizeof err)
		&& !err[0], "a comma in a command: %s", err);
	CHECK(c.n_binds == 3, "three binds (%d)", c.n_binds);
	CHECK(!strcmp(c.binds[2].action.cmd, "foot -e sh -c 'a, b'"), "comma kept: [%s]",
		c.binds[2].action.cmd);
	/* the same keys again: the later wins, in the same place */
	tile_conf_key(&c, "bind", "SUPER+T exec foot", err, sizeof err);
	CHECK(c.n_binds == 3 && !strcmp(c.binds[0].action.cmd, "foot"), "later wins");
	/* bad ones say why and add nothing */
	CHECK(tile_conf_key(&c, "bind", "SUPER+T", err, sizeof err) && err[0], "no action");
	CHECK(tile_conf_key(&c, "bind", "SUPER+Y teleport", err, sizeof err) && err[0], "bad action");
	CHECK(c.n_binds == 3, "nothing added (%d)", c.n_binds);

	/* found by the key as typed, or as written: Super+Shift+1 types exclam */
	tile_conf_key(&c, "bind", "SUPER+SHIFT+1 movetoworkspace 1", err, sizeof err);
	uint32_t typed[] = { XKB_KEY_exclam, XKB_KEY_1 };
	const struct tile_bind *b = tile_bind_find(&c, TILE_MOD_LOGO | TILE_MOD_SHIFT, typed, 2);
	CHECK(b && b->action.act == TILE_ACT_TO_WORKSPACE, "super+shift+1 by its first level");
	/* capitals as lower case, and Caps Lock (2) and Num Lock (16) not counted */
	uint32_t big_h[] = { XKB_KEY_H };
	b = tile_bind_find(&c, TILE_MOD_LOGO | TILE_MOD_SHIFT | 2 | 16, big_h, 1);
	CHECK(b && b->action.act == TILE_ACT_MOVE, "H with caps and num lock");
	uint32_t t[] = { XKB_KEY_t };
	CHECK(!tile_bind_find(&c, TILE_MOD_LOGO | TILE_MOD_CTRL, t, 1), "other modifiers");

	/* the Red Ronin set under the file's own: Super+T stays the file's */
	tile_binds_redronin(&c);
	b = tile_bind_find(&c, TILE_MOD_LOGO, t, 1);
	CHECK(b && b->action.act == TILE_ACT_EXEC && !strcmp(b->action.cmd, "foot"), "file's own wins");
	uint32_t l[] = { XKB_KEY_l };
	b = tile_bind_find(&c, TILE_MOD_LOGO, l, 1);
	CHECK(b && b->action.act == TILE_ACT_FOCUS && b->action.dir == TILE_RIGHT, "super+l right");
	uint32_t zero[] = { XKB_KEY_0 };
	b = tile_bind_find(&c, TILE_MOD_LOGO, zero, 1);
	CHECK(b && b->action.act == TILE_ACT_WORKSPACE && b->action.arg == 10, "super+0 is 10");
	CHECK(c.n_binds < TILE_BINDS_MAX, "room left (%d)", c.n_binds);
	/* the keys as Hyprland had them */
	uint32_t dkey[] = { XKB_KEY_d }, vkey[] = { XKB_KEY_v }, mkey[] = { XKB_KEY_minus },
		akey[] = { XKB_KEY_a };
	b = tile_bind_find(&c, TILE_MOD_LOGO, dkey, 1);
	CHECK(b && b->action.act == TILE_ACT_LAUNCHER, "super+d launcher");
	b = tile_bind_find(&c, TILE_MOD_LOGO, akey, 1);
	CHECK(b && b->action.act == TILE_ACT_SHELF, "super+a the bookcase");
	b = tile_bind_find(&c, TILE_MOD_LOGO, vkey, 1);
	CHECK(b && b->action.act == TILE_ACT_SPLIT, "super+v split");
	b = tile_bind_find(&c, TILE_MOD_LOGO | TILE_MOD_CTRL, vkey, 1);
	CHECK(b && b->action.act == TILE_ACT_LAYOUT, "super+ctrl+v layout");
	b = tile_bind_find(&c, TILE_MOD_LOGO, mkey, 1);
	CHECK(b && b->action.act == TILE_ACT_RESIZE && b->action.num < 0 && b->action.num2 == 0,
		"super+minus narrower");
	b = tile_bind_find(&c, TILE_MOD_LOGO | TILE_MOD_SHIFT, mkey, 1);
	CHECK(b && b->action.act == TILE_ACT_RESIZE && b->action.num == 0 && b->action.num2 < 0,
		"super+shift+minus shorter");
	b = tile_bind_find(&c, TILE_MOD_LOGO | TILE_MOD_CTRL, mkey, 1);
	CHECK(b && b->action.act == TILE_ACT_RATIO, "super+ctrl+minus ratio");
	CHECK(c.super_tap_set && c.super_tap.act == TILE_ACT_LAUNCHER, "a tap of super: launcher");

	/* written back as shell.conf has it */
	char line[400];
	b = tile_bind_find(&c, TILE_MOD_LOGO, zero, 1);
	tile_bind_format(b, line, sizeof line);
	CHECK(strstr(line, "SUPER+") && strstr(line, "workspace 10"), "format: [%s]", line);

	/* the settings */
	CHECK(tile_conf_key(&c, "tiling", "yes", err, sizeof err) && c.tiling, "tiling");
	CHECK(tile_conf_key(&c, "tiling_layout", "dwindle", err, sizeof err)
		&& c.layout == TILE_LAYOUT_DWINDLE, "dwindle");
	CHECK(tile_conf_key(&c, "tiling_ratio", "1.5", err, sizeof err) && err[0] && c.ratio < 0.9,
		"ratio out of range said");
	CHECK(tile_conf_key(&c, "tiling_gap", "10", err, sizeof err) && c.gap_pt == 10.0, "gap");
	CHECK(!tile_conf_key(&c, "desk_wood", "negoro", err, sizeof err), "not ours");
}

static void test_turned(void)
{
	struct tile_box out[3], area = { 0, 0, 1600, 1000 };
	tile_arrange_turned(TILE_LAYOUT_MASTER, area, 3, 0.6, 0, true, out);
	CHECK(out[0].w == 1600 && out[0].h == 600 && out[1].y == 600 && out[2].y == 600,
		"turned master on top: %d,%d %dx%d", out[0].x, out[0].y, out[0].w, out[0].h);
	tile_arrange_turned(TILE_LAYOUT_COLUMNS, area, 2, 0.5, 0, true, out);
	CHECK(out[0].w == 1600 && out[1].y >= 500, "turned columns are rows");
	tile_arrange_turned(TILE_LAYOUT_MASTER, area, 3, 0.6, 0, false, out);
	CHECK(out[0].h == 1000 && out[0].w == 960, "unturned master at the side");
}

static void test_hypr_actions(void)
{
	struct tile_action a;
	char err[160];
	CHECK(tile_action_parse("resize 80 0", &a, err, sizeof err) && a.act == TILE_ACT_RESIZE
		&& a.num == 80 && a.num2 == 0, "resize 80 0: %s", err);
	CHECK(tile_action_parse("resizeactive -40,20", &a, err, sizeof err) && a.num == -40
		&& a.num2 == 20, "resizeactive -40,20: %s", err);
	CHECK(!tile_action_parse("resize", &a, err, sizeof err), "resize needs numbers");
	CHECK(!tile_action_parse("resize 0 0", &a, err, sizeof err), "resize 0 0 is nothing");
	CHECK(!tile_action_parse("resize 10 x", &a, err, sizeof err), "resize 10 x refused");
	CHECK(!tile_action_parse("resize 1e9 0", &a, err, sizeof err), "resize too far refused");
	CHECK(tile_action_parse("togglesplit", &a, err, sizeof err) && a.act == TILE_ACT_SPLIT,
		"togglesplit is split");
	CHECK(tile_action_parse("launcher", &a, err, sizeof err) && a.act == TILE_ACT_LAUNCHER,
		"launcher");
	struct tile_conf c;
	tile_conf_defaults(&c);
	CHECK(!c.super_tap_set && !c.launcher[0] && !c.split_flip, "defaults: none of it");
	err[0] = '\0';
	CHECK(tile_conf_key(&c, "launcher", "fuzzel --config x", err, sizeof err) && !err[0]
		&& !strcmp(c.launcher, "fuzzel --config x"), "launcher key");
	CHECK(tile_conf_key(&c, "super_tap", "none", err, sizeof err) && !err[0]
		&& c.super_tap_set && c.super_tap.act == TILE_ACT_NONE, "super_tap none");
	tile_conf_key(&c, "super_tap", "teleport", err, sizeof err);
	CHECK(err[0], "super_tap teleport complains");
	err[0] = '\0';
	CHECK(tile_conf_key(&c, "tiling_split", "flipped", err, sizeof err) && !err[0] && c.split_flip,
		"tiling_split flipped");
	tile_binds_redronin(&c);
	CHECK(c.super_tap.act == TILE_ACT_NONE, "the file's super_tap kept");
}

int main(void)
{
	layouts();
	ways();
	actions();
	binds();
	test_hypr_actions();
	test_turned();
	if (g_failed) {
		fprintf(stderr, "tile-test: %d failed\n", g_failed);
		return 1;
	}
	printf("tile-test: all passed\n");
	return 0;
}
