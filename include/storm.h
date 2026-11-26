/* The night over the desk, and a storm outside the window (storm.c).
 *
 * A shoji window up on the wall behind the desk throws its light across
 * it: moonlight, faintly, and lightning now and then, the lattice's shadow
 * hard on the wood for an instant, thunder after it. A cord hangs from the
 * top edge; pulled, the window slides open: the rain is heard, its streaks
 * run through the light, and now and then a drop blows in and splashes on
 * the desk, leaving a wet spot that dries. All of it one sprite under the
 * windows, drawn by a shader on the GPU, and only while something moves. */
#ifndef DESK_STORM_H
#define DESK_STORM_H

#include <stdbool.h>

struct wlr_renderer;
struct wlr_allocator;
struct wlr_scene_output;
struct wlr_scene_tree;
struct wlr_output;
struct wl_event_loop;

struct desk_storm_conf {
	bool lightning;     /* now and then, on its own */
	double night;       /* 0 .. 1, how dark the desk is */
	double rain_gain;   /* the rain's loudness when the window is open, 0 .. 1 */
	bool test;          /* DESK_STORM_TEST: lightning every few seconds, drops often */
};

struct desk_storm;

/* NULL where it cannot be drawn (no GLES2). wake is called when it wants
 * frames again (the next lightning, a drop): the compositor schedules them. */
struct desk_storm *desk_storm_create(struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, struct wlr_scene_output *so,
	struct wlr_scene_tree *parent, struct wl_event_loop *loop,
	void (*wake)(void *data), void *data);
void desk_storm_destroy(struct desk_storm *s);
/* its screen is going: destroy must not touch it */
void desk_storm_lost(struct desk_storm *s);
struct wlr_output *desk_storm_output(const struct desk_storm *s);
/* the size it was made for has changed (a new mode) */
bool desk_storm_stale(const struct desk_storm *s);

void desk_storm_configure(struct desk_storm *s, const struct desk_storm_conf *conf);

/* A frame: drawn again if anything moved. paused (locked, the screen dark)
 * holds the lightning back and lets the rain fade. True while it wants the
 * next frame too. now on the motion clock. */
bool desk_storm_tick(struct desk_storm *s, double now, bool paused);

/* the cord, at a point of the layout */
bool desk_storm_cord_hit(const struct desk_storm *s, double lx, double ly);
void desk_storm_hover(struct desk_storm *s, bool over);
/* pulled: the window slides open, or shut */
void desk_storm_pull(struct desk_storm *s, double now);
bool desk_storm_window_open(const struct desk_storm *s);
/* lightning, now (desk-ctl, and for trying it) */
void desk_storm_flash(struct desk_storm *s, double now);

#endif
