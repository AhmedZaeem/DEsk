/*
 * The pure parts of tiling, the stacks (workspaces) and key binds: see
 * include/tile.h. Nothing here touches wlroots; tests/tile-test.c checks it.
 */

#include "tile.h"

#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <xkbcommon/xkbcommon.h>

static const char *const LAYOUT_NAMES[TILE_LAYOUT_COUNT] = { "master", "dwindle", "columns",
	"monocle" };

const char *tile_layout_name(enum tile_layout l)
{
	return l >= 0 && l < TILE_LAYOUT_COUNT ? LAYOUT_NAMES[l] : "master";
}

bool tile_layout_parse(const char *s, enum tile_layout *out)
{
	for (int i = 0; s && i < TILE_LAYOUT_COUNT; i++) {
		if (!strcasecmp(s, LAYOUT_NAMES[i])) {
			*out = (enum tile_layout)i;
			return true;
		}
	}
	return false;
}

/* ---- laying them out ---- */

static int at_least_1(int v)
{
	return v < 1 ? 1 : v;
}

static double clamp_ratio(double r)
{
	return !(r >= 0.1) ? 0.1 : r > 0.9 ? 0.9 : r;
}

/* box cut in two across (side by side) or down (one over the other), gap
 * between, the first getting share of what is left after the gap. Too
 * small to cut with a gap, it is cut without; too small to cut at all, both
 * are the whole of it. */
static void split(struct tile_box b, bool across, double share, int gap, struct tile_box *first,
	struct tile_box *rest)
{
	int len = across ? b.w : b.h;
	if (len < 2) {
		*first = *rest = b;
		return;
	}
	int g = gap < len - 2 ? gap : len - 2;
	g = g < 0 ? 0 : g;
	int room = len - g;
	int a = (int)lround(room * share);
	a = a < 1 ? 1 : a > room - 1 ? room - 1 : a;
	if (across) {
		*first = (struct tile_box){ b.x, b.y, a, b.h };
		*rest = (struct tile_box){ b.x + a + g, b.y, len - a - g, b.h };
	} else {
		*first = (struct tile_box){ b.x, b.y, b.w, a };
		*rest = (struct tile_box){ b.x, b.y + a + g, b.w, len - a - g };
	}
}

/* n boxes as alike as they can be, in a row (across) or a column: what is
 * left after the gaps shared out, the first ones taking the odd pixels */
static void even(struct tile_box b, bool across, int n, int gap, struct tile_box *out)
{
	int len = across ? b.w : b.h;
	int g = n > 1 && gap * (n - 1) <= len - n ? gap : 0;
	int room = len - g * (n - 1);
	if (room < n) {
		for (int i = 0; i < n; i++) {
			out[i] = b;
		}
		return;
	}
	int each = room / n, odd = room % n, at = across ? b.x : b.y;
	for (int i = 0; i < n; i++) {
		int a = each + (i < odd ? 1 : 0);
		out[i] = across ? (struct tile_box){ at, b.y, a, b.h } : (struct tile_box){ b.x, at, b.w, a };
		at += a + g;
	}
}

void tile_arrange(enum tile_layout layout, struct tile_box area, int n, double ratio, int gap,
	struct tile_box *out)
{
	if (n <= 0) {
		return;
	}
	gap = gap < 0 ? 0 : gap;
	/* round the edge too, but never more than leaves the sheets nothing */
	int eg = gap;
	if (eg * 2 >= area.w) {
		eg = (area.w - 1) / 2;
	}
	if (eg * 2 >= area.h) {
		eg = (area.h - 1) / 2;
	}
	eg = eg < 0 ? 0 : eg;
	struct tile_box b = { area.x + eg, area.y + eg, at_least_1(area.w - 2 * eg),
		at_least_1(area.h - 2 * eg) };
	ratio = clamp_ratio(ratio);
	if (n == 1 || layout == TILE_LAYOUT_MONOCLE) {
		for (int i = 0; i < n; i++) {
			out[i] = b;
		}
		return;
	}
	switch (layout) {
	case TILE_LAYOUT_COLUMNS:
		even(b, true, n, gap, out);
		return;
	case TILE_LAYOUT_DWINDLE:
		for (int i = 0; i < n; i++) {
			if (i == n - 1) {
				out[i] = b;
				break;
			}
			/* across while the piece left is wider than tall, as Hyprland's
			 * dwindle turns, so that a wide screen's pieces stay readable */
			bool across = b.w >= b.h;
			struct tile_box first, rest;
			split(b, across, i == 0 ? ratio : 0.5, gap, &first, &rest);
			out[i] = first;
			b = rest;
		}
		return;
	case TILE_LAYOUT_MASTER:
	default: {
		struct tile_box master, stack;
		/* on a screen taller than wide (turned), the master is on top */
		split(b, b.w >= b.h, ratio, gap, &master, &stack);
		out[0] = master;
		even(stack, !(b.w >= b.h), n - 1, gap, out + 1);
		return;
	}
	}
}

/* ---- which way ---- */

bool tile_dir_parse(const char *s, enum tile_dir *out)
{
	static const struct { const char *name; enum tile_dir d; } NAMES[] = {
		{ "left", TILE_LEFT }, { "l", TILE_LEFT }, { "h", TILE_LEFT },
		{ "right", TILE_RIGHT }, { "r", TILE_RIGHT },
		{ "up", TILE_UP }, { "u", TILE_UP }, { "k", TILE_UP }, { "top", TILE_UP },
		{ "down", TILE_DOWN }, { "d", TILE_DOWN }, { "j", TILE_DOWN }, { "bottom", TILE_DOWN },
	};
	for (size_t i = 0; s && i < sizeof NAMES / sizeof NAMES[0]; i++) {
		if (!strcasecmp(s, NAMES[i].name)) {
			*out = NAMES[i].d;
			return true;
		}
	}
	/* (l alone is Hyprland's left, as in movefocus l; vim's l, right, is
	 * written right) */
	return false;
}

/* how far b is from f that way, and across: below 1 along, it is not that way.
 * edges: b must lie past f's edge (tiled sheets); else past its middle
 * (floating ones lying over each other) */
static bool way_of(const struct tile_box *f, const struct tile_box *b, enum tile_dir d, bool edges,
	double *along, double *across, bool *overlap)
{
	double fx = f->x + f->w / 2.0, fy = f->y + f->h / 2.0;
	double bx = b->x + b->w / 2.0, by = b->y + b->h / 2.0;
	bool lr = d == TILE_LEFT || d == TILE_RIGHT;
	if (edges) {
		/* (a pixel or two of slop, for gaps of nothing) */
		switch (d) {
		case TILE_LEFT:
			*along = (double)f->x - (b->x + b->w) + 2.0;
			break;
		case TILE_RIGHT:
			*along = (double)b->x - (f->x + f->w) + 2.0;
			break;
		case TILE_UP:
			*along = (double)f->y - (b->y + b->h) + 2.0;
			break;
		case TILE_DOWN:
		default:
			*along = (double)b->y - (f->y + f->h) + 2.0;
			break;
		}
		if (*along < 0.0) {
			return false;
		}
		*along += 1.0;
	} else {
		*along = d == TILE_LEFT ? fx - bx : d == TILE_RIGHT ? bx - fx : d == TILE_UP ? fy - by : by - fy;
		if (*along < 1.0) {
			return false;
		}
	}
	*across = lr ? fabs(by - fy) : fabs(bx - fx);
	*overlap = lr ? b->y < f->y + f->h && f->y < b->y + b->h : b->x < f->x + f->w && f->x < b->x + b->w;
	return true;
}

int tile_pick_dir(const struct tile_box *boxes, int n, int from, enum tile_dir d)
{
	if (from < 0 || from >= n) {
		return -1;
	}
	const struct tile_box *f = &boxes[from];
	for (int pass = 0; pass < 2; pass++) {
		int best = -1;
		double best_score = 0.0;
		for (int i = 0; i < n; i++) {
			double along, across;
			bool overlap;
			if (i == from || !way_of(f, &boxes[i], d, pass == 0, &along, &across, &overlap)) {
				continue;
			}
			/* past the middle only counts for one lying over this one */
			const struct tile_box *b = &boxes[i];
			if (pass == 1 && !(b->x < f->x + f->w && f->x < b->x + b->w && b->y < f->y + f->h
					&& f->y < b->y + b->h)) {
				continue;
			}
			/* overlapping across counts for nothing: a stack's sheets beside a
			 * master are all to its right, the nearest of them first */
			double score = along + (overlap ? 0.0 : across * 2.0) + across * 0.001;
			if (best < 0 || score < best_score) {
				best = i;
				best_score = score;
			}
		}
		if (best >= 0) {
			return best;
		}
	}
	return -1;
}

/* ---- actions ---- */

static const char *const ACT_NAMES[TILE_ACT_COUNT] = {
	[TILE_ACT_NONE] = "none",
	[TILE_ACT_EXEC] = "exec",
	[TILE_ACT_CLOSE] = "close",
	[TILE_ACT_FULLSCREEN] = "fullscreen",
	[TILE_ACT_MAXIMIZE] = "maximize",
	[TILE_ACT_FOLD] = "fold",
	[TILE_ACT_FLOAT] = "float",
	[TILE_ACT_TILING] = "tiling",
	[TILE_ACT_LAYOUT] = "layout",
	[TILE_ACT_RATIO] = "ratio",
	[TILE_ACT_GAP] = "gap",
	[TILE_ACT_FOCUS] = "focus",
	[TILE_ACT_MOVE] = "move",
	[TILE_ACT_PROMOTE] = "promote",
	[TILE_ACT_CYCLE] = "cycle",
	[TILE_ACT_WORKSPACE] = "workspace",
	[TILE_ACT_TO_WORKSPACE] = "movetoworkspace",
	[TILE_ACT_TO_WORKSPACE_SILENT] = "movetoworkspacesilent",
	[TILE_ACT_PIN] = "pin",
	[TILE_ACT_CENTER] = "center",
	[TILE_ACT_TERMINAL] = "terminal",
	[TILE_ACT_BROWSER] = "browser",
	[TILE_ACT_FILES] = "files",
	[TILE_ACT_EDITOR] = "editor",
	[TILE_ACT_DRAWER] = "drawer",
	[TILE_ACT_SHELF] = "shelf",
	[TILE_ACT_CONSOLE] = "console",
	[TILE_ACT_POST] = "post",
	[TILE_ACT_LOCK] = "lock",
	[TILE_ACT_LOGOUT] = "logout",
	[TILE_ACT_SNIP] = "snip",
	[TILE_ACT_CLIPBOARD] = "clipboard",
	[TILE_ACT_SWITCHER] = "switcher",
	[TILE_ACT_RELOAD] = "reload",
};

const char *tile_act_name(enum tile_act a)
{
	return a >= 0 && a < TILE_ACT_COUNT && ACT_NAMES[a] ? ACT_NAMES[a] : "?";
}

__attribute__((format(printf, 3, 4)))
static void say(char *err, size_t errn, const char *fmt, ...)
{
	if (err && errn) {
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(err, errn, fmt, ap);
		va_end(ap);
	}
}

/* a whole number from 1 to max, and nothing after it */
static bool whole(const char *s, int max, int *out)
{
	if (!s || !*s) {
		return false;
	}
	char *end;
	long v = strtol(s, &end, 10);
	if (*end || v < 1 || v > max) {
		return false;
	}
	*out = (int)v;
	return true;
}

static bool number(const char *s, double *out, bool *abs)
{
	if (!s || !*s) {
		return false;
	}
	*abs = !(s[0] == '+' || s[0] == '-');
	char *end;
	double v = strtod(s, &end);
	if (*end || !isfinite(v)) {
		return false;
	}
	*out = v;
	return true;
}

static bool ws_arg(const char *s, int *out)
{
	if (whole(s, TILE_WS_MAX, out)) {
		return true;
	}
	if (s && (!strcasecmp(s, "next") || !strcasecmp(s, "e+1") || !strcasecmp(s, "+1"))) {
		*out = TILE_WS_NEXT;
		return true;
	}
	if (s && (!strcasecmp(s, "prev") || !strcasecmp(s, "e-1") || !strcasecmp(s, "-1"))) {
		*out = TILE_WS_PREV;
		return true;
	}
	if (s && (!strcasecmp(s, "previous") || !strcasecmp(s, "back"))) {
		*out = TILE_WS_BACK;
		return true;
	}
	return false;
}

bool tile_action_parse(const char *s, struct tile_action *out, char *err, size_t errn)
{
	memset(out, 0, sizeof *out);
	if (err && errn) {
		err[0] = '\0';
	}
	while (s && isspace((unsigned char)*s)) {
		s++;
	}
	if (!s || !*s) {
		say(err, errn, "%s", "no action");
		return false;
	}
	char verb[40];
	size_t n = 0;
	while (s[n] && !isspace((unsigned char)s[n]) && n + 1 < sizeof verb) {
		verb[n] = s[n];
		n++;
	}
	verb[n] = '\0';
	const char *rest = s + n;
	while (isspace((unsigned char)*rest)) {
		rest++;
	}
	/* the argument, without the spaces after it */
	char arg[TILE_CMD_MAX];
	snprintf(arg, sizeof arg, "%s", rest);
	for (size_t k = strlen(arg); k > 0 && isspace((unsigned char)arg[k - 1]); k--) {
		arg[k - 1] = '\0';
	}
	int act = -1;
	for (int i = 0; i < TILE_ACT_COUNT; i++) {
		if (ACT_NAMES[i] && !strcasecmp(verb, ACT_NAMES[i])) {
			act = i;
			break;
		}
	}
	/* Hyprland's names for the same, so a bind can be copied across */
	if (act < 0) {
		static const struct { const char *name; enum tile_act a; } ALIAS[] = {
			{ "killactive", TILE_ACT_CLOSE }, { "togglefloating", TILE_ACT_FLOAT },
			{ "movefocus", TILE_ACT_FOCUS }, { "movewindow", TILE_ACT_MOVE },
			{ "swapwindow", TILE_ACT_MOVE }, { "swap", TILE_ACT_MOVE },
			{ "minimize", TILE_ACT_FOLD }, { "centerwindow", TILE_ACT_CENTER },
			{ "maximise", TILE_ACT_MAXIMIZE }, { "unfold", TILE_ACT_MAXIMIZE },
			{ "exit", TILE_ACT_LOGOUT }, { "screenshot", TILE_ACT_SNIP },
			{ "launcher", TILE_ACT_SHELF }, { "apps", TILE_ACT_SHELF },
			{ "notifications", TILE_ACT_POST }, { "controlpanel", TILE_ACT_CONSOLE },
			{ "dock", TILE_ACT_DRAWER }, { "togglesplit", TILE_ACT_LAYOUT },
			{ "layoutmsg", TILE_ACT_LAYOUT }, { "splitratio", TILE_ACT_RATIO },
			{ "cyclenext", TILE_ACT_CYCLE }, { "swapmaster", TILE_ACT_PROMOTE },
		};
		for (size_t i = 0; i < sizeof ALIAS / sizeof ALIAS[0]; i++) {
			if (!strcasecmp(verb, ALIAS[i].name)) {
				act = (int)ALIAS[i].a;
				break;
			}
		}
	}
	if (act < 0) {
		say(err, errn, "no action called \"%s\"", verb);
		return false;
	}
	out->act = (enum tile_act)act;
	switch (out->act) {
	case TILE_ACT_EXEC:
		if (strlen(rest) >= sizeof out->cmd) {
			say(err, errn, "the command is longer than %zu characters", sizeof out->cmd - 1);
			return false;
		}
		if (!arg[0]) {
			say(err, errn, "%s", "exec needs a command");
			return false;
		}
		snprintf(out->cmd, sizeof out->cmd, "%s", arg);
		return true;
	case TILE_ACT_TILING:
		out->arg = -1;
		if (!arg[0] || !strcasecmp(arg, "toggle")) {
			return true;
		}
		if (!strcasecmp(arg, "on") || !strcasecmp(arg, "yes")) {
			out->arg = 1;
			return true;
		}
		if (!strcasecmp(arg, "off") || !strcasecmp(arg, "no")) {
			out->arg = 0;
			return true;
		}
		say(err, errn, "tiling is on, off or toggle, not \"%s\"", arg);
		return false;
	case TILE_ACT_LAYOUT: {
		out->arg = -1;
		enum tile_layout l;
		if (!arg[0] || !strcasecmp(arg, "next") || !strcasecmp(arg, "togglesplit")) {
			return true;
		}
		if (tile_layout_parse(arg, &l)) {
			out->arg = (int)l;
			return true;
		}
		say(err, errn, "layout is master, dwindle, columns, monocle or next, not \"%s\"", arg);
		return false;
	}
	case TILE_ACT_RATIO:
	case TILE_ACT_GAP:
		if (!number(arg, &out->num, &out->abs)) {
			say(err, errn, "a number such as +0.05 or 0.6, not \"%s\"", arg);
			return false;
		}
		return true;
	case TILE_ACT_FOCUS:
	case TILE_ACT_MOVE:
		if (!tile_dir_parse(arg, &out->dir)) {
			say(err, errn, "a way: left, right, up or down, not \"%s\"", arg);
			return false;
		}
		return true;
	case TILE_ACT_CYCLE:
		out->arg = 1;
		if (!arg[0] || !strcasecmp(arg, "next")) {
			return true;
		}
		if (!strcasecmp(arg, "prev") || !strcasecmp(arg, "previous")) {
			out->arg = -1;
			return true;
		}
		say(err, errn, "cycle is next or prev, not \"%s\"", arg);
		return false;
	case TILE_ACT_WORKSPACE:
	case TILE_ACT_TO_WORKSPACE:
	case TILE_ACT_TO_WORKSPACE_SILENT:
		if (!ws_arg(arg, &out->arg)) {
			say(err, errn, "a workspace: 1 to 10, next, prev or previous, not \"%s\"", arg);
			return false;
		}
		return true;
	default:
		if (arg[0]) {
			say(err, errn, "%s takes nothing after it", ACT_NAMES[act]);
			return false;
		}
		return true;
	}
}

/* ---- the keys ---- */

bool tile_combo_parse(const char *s, uint32_t *mods, uint32_t *sym, char *err, size_t errn)
{
	*mods = 0;
	*sym = 0;
	char buf[96];
	snprintf(buf, sizeof buf, "%s", s ? s : "");
	char *save = NULL;
	bool key = false;
	for (char *tok = strtok_r(buf, "+ ", &save); tok; tok = strtok_r(NULL, "+ ", &save)) {
		if (!strcasecmp(tok, "SUPER") || !strcasecmp(tok, "LOGO") || !strcasecmp(tok, "WIN")
			|| !strcasecmp(tok, "MOD4") || !strcasecmp(tok, "META")) {
			*mods |= TILE_MOD_LOGO;
		} else if (!strcasecmp(tok, "SHIFT")) {
			*mods |= TILE_MOD_SHIFT;
		} else if (!strcasecmp(tok, "CTRL") || !strcasecmp(tok, "CONTROL")) {
			*mods |= TILE_MOD_CTRL;
		} else if (!strcasecmp(tok, "ALT") || !strcasecmp(tok, "MOD1")) {
			*mods |= TILE_MOD_ALT;
		} else {
			if (key) {
				say(err, errn, "two keys in one bind (\"%s\")", tok);
				return false;
			}
			/* the names xkb has, as Hyprland takes them: Return, grave, minus, F4 */
			xkb_keysym_t k = xkb_keysym_from_name(tok, XKB_KEYSYM_NO_FLAGS);
			if (k == XKB_KEY_NoSymbol) {
				k = xkb_keysym_from_name(tok, XKB_KEYSYM_CASE_INSENSITIVE);
			}
			if (k == XKB_KEY_NoSymbol) {
				say(err, errn, "no key called \"%s\"", tok);
				return false;
			}
			*sym = xkb_keysym_to_lower(k);
			key = true;
		}
	}
	if (!key) {
		say(err, errn, "%s", "a bind needs a key as well as its modifiers");
		return false;
	}
	/* a key alone would be taken from what is typed, and from the desk's own
	 * Escape and arrows: only the function keys and the media keys may be */
	bool bare_ok = (*sym >= XKB_KEY_F1 && *sym <= XKB_KEY_F35) || (*sym >= 0x1008FF00 && *sym <= 0x1008FFFF)
		|| *sym == XKB_KEY_Print;
	if (*mods == 0 && !bare_ok) {
		say(err, errn, "%s", "a bind on a key alone must be a function, media or Print key");
		return false;
	}
	return true;
}

/* ---- shell.conf ---- */

void tile_conf_defaults(struct tile_conf *c)
{
	memset(c, 0, sizeof *c);
	c->tiling = false;
	c->layout = TILE_LAYOUT_MASTER;
	c->ratio = 0.55;
	c->gap_pt = 6.0;
	c->ws_wrap = true;
}

static bool yes_no(const char *v, bool *out)
{
	if (!strcasecmp(v, "yes") || !strcasecmp(v, "on") || !strcasecmp(v, "true")) {
		*out = true;
		return true;
	}
	if (!strcasecmp(v, "no") || !strcasecmp(v, "off") || !strcasecmp(v, "false")) {
		*out = false;
		return true;
	}
	return false;
}

/* [s, end) is nothing but modifiers (or nothing): Hyprland's way of
 * writing a bind, "SUPER SHIFT, H, movewindow, l", and not a command of
 * ours with a comma in it */
static bool only_mods(const char *s, const char *end)
{
	char buf[96];
	size_t n = (size_t)(end - s) < sizeof buf - 1 ? (size_t)(end - s) : sizeof buf - 1;
	memcpy(buf, s, n);
	buf[n] = '\0';
	char *save = NULL;
	for (char *tok = strtok_r(buf, "+ \t", &save); tok; tok = strtok_r(NULL, "+ \t", &save)) {
		uint32_t m, k;
		char probe[40];
		snprintf(probe, sizeof probe, "%s+a", tok);
		if (!tile_combo_parse(probe, &m, &k, NULL, 0) || m == 0) {
			return false;
		}
	}
	return true;
}

static void bind_add(struct tile_conf *c, uint32_t mods, uint32_t sym, const struct tile_action *a)
{
	/* a later line for the same keys takes the place of the earlier */
	for (int i = 0; i < c->n_binds; i++) {
		if (c->binds[i].mods == mods && c->binds[i].sym == sym) {
			c->binds[i].action = *a;
			return;
		}
	}
	if (c->n_binds < TILE_BINDS_MAX) {
		c->binds[c->n_binds++] = (struct tile_bind){ mods, sym, *a };
	}
}

bool tile_conf_key(struct tile_conf *c, const char *key, const char *val, char *err, size_t errn)
{
	if (err && errn) {
		err[0] = '\0';
	}
	bool b;
	if (!strcmp(key, "tiling")) {
		if (!yes_no(val, &b)) {
			say(err, errn, "tiling is yes or no, not \"%s\"", val);
		} else {
			c->tiling = b;
		}
		return true;
	}
	if (!strcmp(key, "tiling_layout")) {
		if (!tile_layout_parse(val, &c->layout)) {
			say(err, errn, "tiling_layout is master, dwindle, columns or monocle, not \"%s\"", val);
		}
		return true;
	}
	if (!strcmp(key, "tiling_ratio")) {
		char *end;
		double r = strtod(val, &end);
		if (*end || !(r >= 0.1 && r <= 0.9)) {
			say(err, errn, "tiling_ratio is from 0.1 to 0.9, not \"%s\"", val);
		} else {
			c->ratio = r;
		}
		return true;
	}
	if (!strcmp(key, "tiling_gap")) {
		char *end;
		double g = strtod(val, &end);
		if (*end || !(g >= 0.0 && g <= 64.0)) {
			say(err, errn, "tiling_gap is points from 0 to 64, not \"%s\"", val);
		} else {
			c->gap_pt = g;
		}
		return true;
	}
	if (!strcmp(key, "workspaces_wrap")) {
		if (!yes_no(val, &b)) {
			say(err, errn, "workspaces_wrap is yes or no, not \"%s\"", val);
		} else {
			c->ws_wrap = b;
		}
		return true;
	}
	if (!strcmp(key, "ui_style")) {
		if (!strcasecmp(val, "redronin")) {
			c->style_redronin = true;
		} else if (!strcasecmp(val, "desk")) {
			c->style_redronin = false;
		} else {
			say(err, errn, "ui_style is desk or redronin, not \"%s\"", val);
		}
		return true;
	}
	if (!strcmp(key, "bind_defaults")) {
		if (!strcasecmp(val, "redronin")) {
			c->binds_default = true;
		} else if (!strcasecmp(val, "desk") || !strcasecmp(val, "none")) {
			c->binds_default = false;
		} else {
			say(err, errn, "bind_defaults is desk or redronin, not \"%s\"", val);
		}
		return true;
	}
	if (!strcmp(key, "bind")) {
		/* "SUPER+SHIFT+H move left", or Hyprland's "SUPER SHIFT, H, movewindow, l" */
		char line[TILE_CMD_MAX + 96];
		snprintf(line, sizeof line, "%s", val);
		char *combo = line, *action = NULL;
		char *comma = strchr(line, ',');
		if (comma && only_mods(line, comma)) {
			/* the keys up to the first comma or two: the modifiers, then the key */
			*comma = '\0';
			char *next = comma + 1;
			char *comma2 = strchr(next, ',');
			char keys[sizeof line + 2];
			if (comma2) {
				*comma2 = '\0';
				snprintf(keys, sizeof keys, "%s+%s", line, next);
				action = comma2 + 1;
			} else {
				snprintf(keys, sizeof keys, "%s", line);
				action = next;
			}
			/* and the action's own commas made spaces: "movefocus, l" */
			for (char *p = action; *p; p++) {
				if (*p == ',') {
					*p = ' ';
				}
			}
			uint32_t mods, sym;
			struct tile_action a;
			if (!tile_combo_parse(keys, &mods, &sym, err, errn)) {
				return true;
			}
			if (!tile_action_parse(action, &a, err, errn)) {
				return true;
			}
			bind_add(c, mods, sym, &a);
			return true;
		}
		while (*combo && isspace((unsigned char)*combo)) {
			combo++;
		}
		char *sp = combo;
		while (*sp && !isspace((unsigned char)*sp)) {
			sp++;
		}
		if (!*sp) {
			say(err, errn, "bind needs keys and an action (\"%s\")", val);
			return true;
		}
		*sp = '\0';
		action = sp + 1;
		uint32_t mods, sym;
		struct tile_action a;
		if (!tile_combo_parse(combo, &mods, &sym, err, errn)) {
			return true;
		}
		if (!tile_action_parse(action, &a, err, errn)) {
			return true;
		}
		bind_add(c, mods, sym, &a);
		return true;
	}
	return false;
}

void tile_bind_format(const struct tile_bind *b, char *out, size_t n)
{
	char key[64] = "";
	xkb_keysym_get_name(b->sym, key, sizeof key);
	/* a letter as it is printed on the key */
	if (key[0] && !key[1]) {
		key[0] = (char)toupper((unsigned char)key[0]);
	}
	const struct tile_action *a = &b->action;
	char act[TILE_CMD_MAX + 64];
	switch (a->act) {
	case TILE_ACT_EXEC:
		snprintf(act, sizeof act, "exec %s", a->cmd);
		break;
	case TILE_ACT_FOCUS:
	case TILE_ACT_MOVE: {
		static const char *const W[] = { "left", "right", "up", "down" };
		snprintf(act, sizeof act, "%s %s", tile_act_name(a->act), W[a->dir]);
		break;
	}
	case TILE_ACT_WORKSPACE:
	case TILE_ACT_TO_WORKSPACE:
	case TILE_ACT_TO_WORKSPACE_SILENT:
		if (a->arg > 0) {
			snprintf(act, sizeof act, "%s %d", tile_act_name(a->act), a->arg);
		} else {
			snprintf(act, sizeof act, "%s %s", tile_act_name(a->act),
				a->arg == TILE_WS_NEXT ? "next" : a->arg == TILE_WS_PREV ? "prev" : "previous");
		}
		break;
	case TILE_ACT_RATIO:
	case TILE_ACT_GAP:
		snprintf(act, sizeof act, a->abs ? "%s %g" : "%s %+g", tile_act_name(a->act), a->num);
		break;
	case TILE_ACT_LAYOUT:
		snprintf(act, sizeof act, "layout %s", a->arg < 0 ? "next" : tile_layout_name(a->arg));
		break;
	case TILE_ACT_TILING:
		snprintf(act, sizeof act, "tiling %s", a->arg < 0 ? "toggle" : a->arg ? "on" : "off");
		break;
	case TILE_ACT_CYCLE:
		snprintf(act, sizeof act, "cycle %s", a->arg < 0 ? "prev" : "next");
		break;
	default:
		snprintf(act, sizeof act, "%s", tile_act_name(a->act));
		break;
	}
	char combo[128];
	snprintf(combo, sizeof combo, "%s%s%s%s%s", b->mods & TILE_MOD_LOGO ? "SUPER+" : "",
		b->mods & TILE_MOD_CTRL ? "CTRL+" : "", b->mods & TILE_MOD_ALT ? "ALT+" : "",
		b->mods & TILE_MOD_SHIFT ? "SHIFT+" : "", key);
	snprintf(out, n, "%-24s %s", combo, act);
}

const struct tile_bind *tile_bind_find(const struct tile_conf *c, uint32_t mods,
	const uint32_t *syms, int nsyms)
{
	mods &= TILE_MOD_MASK;
	for (int k = 0; k < nsyms; k++) {
		uint32_t s = xkb_keysym_to_lower(syms[k]);
		for (int i = c->n_binds - 1; i >= 0; i--) {
			if (c->binds[i].mods == mods && c->binds[i].sym == s) {
				return &c->binds[i];
			}
		}
	}
	return NULL;
}

/* ---- the set Red Ronin OS comes with ----
 *
 * The keys of ~/.config/hypr/ronin-binds.lua, so that hands that know the
 * Hyprland session know this one: launching on T, E, B; focus and moving on
 * the vim keys; the stacks on the number row. Where the desk has a thing of
 * its own for the job (the bookcase for the launcher, the post box for
 * notices) the key opens that. */
void tile_binds_redronin(struct tile_conf *c)
{
	static const char *const SET[][2] = {
		{ "SUPER+T", "terminal" },
		{ "SUPER+Return", "terminal" },
		{ "SUPER+E", "files" },
		{ "SUPER+B", "browser" },
		{ "SUPER+D", "shelf" },
		{ "SUPER+SHIFT+D", "drawer" },
		{ "SUPER+Q", "close" },
		{ "SUPER+F", "fullscreen" },
		{ "SUPER+SHIFT+F", "maximize" },
		{ "SUPER+SHIFT+V", "float" },
		{ "SUPER+V", "layout next" },
		{ "SUPER+ALT+V", "clipboard" },
		{ "SUPER+ALT+C", "center" },
		{ "SUPER+P", "pin" },
		{ "SUPER+W", "tiling toggle" },
		{ "SUPER+M", "fold" },
		{ "SUPER+H", "focus left" },
		{ "SUPER+J", "focus down" },
		{ "SUPER+K", "focus up" },
		{ "SUPER+L", "focus right" },
		{ "SUPER+Left", "focus left" },
		{ "SUPER+Down", "focus down" },
		{ "SUPER+Up", "focus up" },
		{ "SUPER+Right", "focus right" },
		{ "SUPER+SHIFT+H", "move left" },
		{ "SUPER+SHIFT+J", "move down" },
		{ "SUPER+SHIFT+K", "move up" },
		{ "SUPER+SHIFT+L", "move right" },
		{ "SUPER+SHIFT+Left", "move left" },
		{ "SUPER+SHIFT+Down", "move down" },
		{ "SUPER+SHIFT+Up", "move up" },
		{ "SUPER+SHIFT+Right", "move right" },
		{ "SUPER+ALT+H", "move left" },
		{ "SUPER+ALT+J", "move down" },
		{ "SUPER+ALT+K", "move up" },
		{ "SUPER+ALT+L", "move right" },
		{ "SUPER+SHIFT+Return", "promote" },
		{ "SUPER+minus", "ratio -0.05" },
		{ "SUPER+equal", "ratio +0.05" },
		{ "SUPER+SHIFT+minus", "gap -2" },
		{ "SUPER+SHIFT+equal", "gap +2" },
		{ "SUPER+Tab", "cycle next" },
		{ "SUPER+SHIFT+Tab", "cycle prev" },
		{ "SUPER+grave", "workspace previous" },
		{ "SUPER+CTRL+Right", "workspace next" },
		{ "SUPER+CTRL+Left", "workspace prev" },
		{ "SUPER+CTRL+L", "workspace next" },
		{ "SUPER+CTRL+H", "workspace prev" },
		{ "SUPER+CTRL+Escape", "lock" },
		{ "SUPER+Escape", "logout" },
		{ "SUPER+N", "post" },
		{ "SUPER+C", "console" },
		{ "SUPER+SHIFT+S", "snip" },
		{ "SUPER+SHIFT+R", "reload" },
	};
	struct tile_conf have = *c;
	for (size_t i = 0; i < sizeof SET / sizeof SET[0]; i++) {
		uint32_t mods, sym;
		struct tile_action a;
		if (!tile_combo_parse(SET[i][0], &mods, &sym, NULL, 0)
			|| !tile_action_parse(SET[i][1], &a, NULL, 0)) {
			continue;
		}
		if (!tile_bind_find(&have, mods, &sym, 1)) {
			bind_add(c, mods, sym, &a);
		}
	}
	for (int n = 1; n <= TILE_WS_MAX; n++) {
		char keys[32], act[48];
		const char *k = n == 10 ? "0" : (char[]){ (char)('0' + n), '\0' };
		struct { const char *mod, *verb; } ROW[] = {
			{ "SUPER+", "workspace" },
			{ "SUPER+SHIFT+", "movetoworkspace" },
			{ "SUPER+ALT+", "movetoworkspacesilent" },
		};
		for (size_t r = 0; r < sizeof ROW / sizeof ROW[0]; r++) {
			snprintf(keys, sizeof keys, "%s%s", ROW[r].mod, k);
			snprintf(act, sizeof act, "%s %d", ROW[r].verb, n);
			uint32_t mods, sym;
			struct tile_action a;
			if (tile_combo_parse(keys, &mods, &sym, NULL, 0) && tile_action_parse(act, &a, NULL, 0)
				&& !tile_bind_find(&have, mods, &sym, 1)) {
				bind_add(c, mods, sym, &a);
			}
		}
	}
}
