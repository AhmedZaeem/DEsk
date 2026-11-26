#pragma once
/*
 * The pure parts of laying sheets out side by side (tiling), the desk's
 * stacks of paper (workspaces), and key binds read from shell.conf --
 * knowing nothing of wlroots or the scene, so that tests/tile-test.c can
 * check them. wm.c, section "laying the sheets out", says what they mean
 * on the desk.
 *
 * Added for Red Ronin OS (docs/TILING.md); everything here is off until
 * shell.conf asks for it, so a desk without those keys is the desk as made.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* the stacks: Super+1 .. Super+0 */
#define TILE_WS_MAX 10
/* binds read from shell.conf, at most */
#define TILE_BINDS_MAX 128
#define TILE_CMD_MAX 320

/* How the sheets on one screen are laid out when tiling is on. */
enum tile_layout {
	TILE_LAYOUT_MASTER = 0,  /* one large on the left, the rest stacked on the right */
	TILE_LAYOUT_DWINDLE,     /* each halving what the one before left, turning each time */
	TILE_LAYOUT_COLUMNS,     /* side by side, all as wide */
	TILE_LAYOUT_MONOCLE,     /* every one over all of it, the front one seen */
	TILE_LAYOUT_COUNT,
};

const char *tile_layout_name(enum tile_layout l);
bool tile_layout_parse(const char *s, enum tile_layout *out);

struct tile_box {
	int x, y, w, h;
};

/* n boxes for n sheets, in order, inside area: gap pixels between them and
 * round the edge; ratio the share of the first split (0.1 .. 0.9). Nothing
 * comes out narrower or shorter than 1 pixel. */
/* The same, each way it turns made the other (Super+V, tiling_split):
 * the master on top of a wide screen, columns as rows, dwindle's first
 * split the other way. */
void tile_arrange_turned(enum tile_layout layout, struct tile_box area, int n, double ratio,
	int gap, bool turned, struct tile_box *out);
void tile_arrange(enum tile_layout layout, struct tile_box area, int n, double ratio, int gap,
	struct tile_box *out);

enum tile_dir {
	TILE_LEFT = 0,
	TILE_RIGHT,
	TILE_UP,
	TILE_DOWN,
};
bool tile_dir_parse(const char *s, enum tile_dir *out);

/* Of boxes[0..n), the one nearest boxes[from] that way: its middle past
 * from's, nearer straight that way than across. -1 for none. */
int tile_pick_dir(const struct tile_box *boxes, int n, int from, enum tile_dir d);

/* Modifiers, numbered as wlroots numbers them (WLR_MODIFIER_*), so this
 * file needs nothing of wlroots. */
#define TILE_MOD_SHIFT 1u
#define TILE_MOD_CTRL 4u
#define TILE_MOD_ALT 8u
#define TILE_MOD_LOGO 64u
#define TILE_MOD_MASK (TILE_MOD_SHIFT | TILE_MOD_CTRL | TILE_MOD_ALT | TILE_MOD_LOGO)

enum tile_act {
	TILE_ACT_NONE = 0,       /* the keys do nothing (a default of the desk's taken off) */
	TILE_ACT_EXEC,           /* cmd, through /bin/sh */
	TILE_ACT_CLOSE,
	TILE_ACT_FULLSCREEN,
	TILE_ACT_MAXIMIZE,       /* opened out, or back */
	TILE_ACT_FOLD,           /* folded, or opened */
	TILE_ACT_FLOAT,          /* the sheet in front out of the tiling, or back in */
	TILE_ACT_TILING,         /* arg: 1 on, 0 off, -1 the other way */
	TILE_ACT_LAYOUT,         /* arg: enum tile_layout, or -1 the next */
	TILE_ACT_RATIO,          /* num: what to add, or (abs) what to make it */
	TILE_ACT_GAP,            /* num: points to add, or (abs) what to make it */
	TILE_ACT_FOCUS,          /* dir */
	TILE_ACT_MOVE,           /* dir: swapped with the one there, or laid that way */
	TILE_ACT_PROMOTE,        /* swapped with the first (the master) */
	TILE_ACT_CYCLE,          /* arg: 1 next, -1 previous, in the tiling's order */
	TILE_ACT_WORKSPACE,      /* arg: 1..10, or TILE_WS_NEXT / _PREV / _BACK */
	TILE_ACT_TO_WORKSPACE,   /* the same, the sheet in front taken there and followed */
	TILE_ACT_TO_WORKSPACE_SILENT, /* taken there, the desk staying */
	TILE_ACT_PIN,            /* the sheet in front on every stack */
	TILE_ACT_CENTER,
	TILE_ACT_TERMINAL,
	TILE_ACT_BROWSER,        /* the standard programs (README, "Standard programs") */
	TILE_ACT_FILES,
	TILE_ACT_EDITOR,
	TILE_ACT_DRAWER,
	TILE_ACT_SHELF,
	TILE_ACT_CONSOLE,
	TILE_ACT_POST,
	TILE_ACT_LOCK,
	TILE_ACT_LOGOUT,
	TILE_ACT_SNIP,
	TILE_ACT_CLIPBOARD,
	TILE_ACT_SWITCHER,       /* Alt+Tab's spread */
	TILE_ACT_RELOAD,         /* shell.conf read again */
	TILE_ACT_RESIZE,         /* num, num2: points wider and taller (Hyprland's resizeactive);
	                          * a tiled sheet moves the split it is on */
	TILE_ACT_SPLIT,          /* the tiling turned a quarter (master on top, not at the side) */
	TILE_ACT_LAUNCHER,       /* tile_conf.launcher, or fuzzel, or the bookcase */
	TILE_ACT_WINDOW,         /* Red Ronin: the shoji window behind the desk, open or shut (storm.c) */
	TILE_ACT_LIGHTNING,      /* and lightning outside it, now */
	TILE_ACT_POWER,          /* the power menu (desk-power-menu): nothing happens till one is chosen */
	TILE_ACT_SUSPEND,        /* the machine to sleep */
	TILE_ACT_REBOOT,         /* restart, the windows asked to close first */
	TILE_ACT_POWEROFF,       /* shut down, likewise */
	TILE_ACT_SCREEN,         /* the keys and pointer to the screen that way */
	TILE_ACT_TO_SCREEN,      /* the front sheet to the screen that way, into the stack shown there */
	TILE_ACT_POINTER,        /* the pointer to X Y on the layout, as a mouse would take it (for tests) */
	TILE_ACT_COUNT,
};

#define TILE_WS_NEXT (-1)
#define TILE_WS_PREV (-2)
#define TILE_WS_BACK (-3)

struct tile_action {
	enum tile_act act;
	int arg;
	bool abs;                /* num is a value, not a step */
	double num;
	double num2;             /* resize: the second number (taller) */
	enum tile_dir dir;
	char cmd[TILE_CMD_MAX];
};

/* "focus left", "workspace 3", "exec kitty -e btop", "ratio +0.05" ...
 * false, with why in err, for anything else. */
bool tile_action_parse(const char *s, struct tile_action *out, char *err, size_t errn);
const char *tile_act_name(enum tile_act a);

/* The keys of a bind: "SUPER+SHIFT+H", "ALT+F4", "SUPER+grave". mods as
 * above; sym the key's keysym, made lower case. */
bool tile_combo_parse(const char *s, uint32_t *mods, uint32_t *sym, char *err, size_t errn);

struct tile_bind {
	uint32_t mods, sym;
	struct tile_action action;
};

/* What shell.conf says of these things: the keys tiling, tiling_layout,
 * tiling_ratio, tiling_gap, workspaces_wrap and bind. */
struct tile_conf {
	bool tiling;
	enum tile_layout layout;
	double ratio;
	double gap_pt;           /* in points, between sheets and round the edge */
	bool ws_wrap;            /* next from the last goes to the first */
	bool ws_per_screen;      /* Red Ronin: each screen shows a stack of its own (Hyprland's way) */
	/* workspace_screen = N TEXT: stack N belongs on the screen whose make,
	 * model, serial or connector has TEXT in it ("" none) */
	char ws_screen[TILE_WS_MAX + 1][64];
	bool binds_default;      /* the Red Ronin set under the binds given (bind_defaults) */
	bool style_redronin;     /* ui_style = redronin: the seal, the stack on the clock */
	bool split_flip;         /* tiling_split = flipped: the layouts turned a quarter */
	char launcher[TILE_CMD_MAX];   /* launcher = CMD: what the launcher action runs */
	bool super_tap_set;      /* super_tap = ACTION given: Super pressed and let go alone */
	struct tile_action super_tap;
	int n_binds;
	struct tile_bind binds[TILE_BINDS_MAX];
};

void tile_conf_defaults(struct tile_conf *c);
/* One shell.conf line's key and value, if it is one of these: true when it
 * was (and then any complaint is in err, empty when there is none). */
bool tile_conf_key(struct tile_conf *c, const char *key, const char *val, char *err, size_t errn);

/* The bind for these keys: the last one given for them wins, as a later line
 * of a file does. NULL when there is none. syms are what the key gives at
 * the level held, and the key's own at the first level (Super+Shift+1 is
 * Super+Shift+exclam as typed and Super+Shift+1 as written). */
const struct tile_bind *tile_bind_find(const struct tile_conf *c, uint32_t mods,
	const uint32_t *syms, int nsyms);

/* The set Red Ronin OS comes with (bind_defaults = redronin): appended
 * under what the file gave, so the file's own win. */
void tile_binds_redronin(struct tile_conf *c);

/* A bind written back as shell.conf has it: "SUPER+SHIFT+H  move left" */
void tile_bind_format(const struct tile_bind *b, char *out, size_t n);
