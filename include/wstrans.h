/* Going from one stack to another, in three dimensions (wstrans.c).
 *
 * The screen as it stood is taken as a picture, and a shader on the GPU
 * draws it going away over the live desk underneath, which is the new
 * stack already: cut through by a katana, folded shut as a byobu screen,
 * rolled up as an emakimono, or shut behind shoji doors that then open.
 * Everything is drawn in the compositor's own GL context into buffers the
 * scene shows as they are, so nothing goes through the CPU. */
#ifndef DESK_WSTRANS_H
#define DESK_WSTRANS_H

#include <stdbool.h>

struct wlr_renderer;
struct wlr_allocator;
struct wlr_scene_output;
struct wlr_scene_tree;

enum desk_wstrans_kind {
	DESK_WSTRANS_NONE = 0,
	DESK_WSTRANS_KATANA,
	DESK_WSTRANS_BYOBU,
	DESK_WSTRANS_SCROLL,
	DESK_WSTRANS_SHOJI,
	DESK_WSTRANS_N,
};

struct desk_wstrans;

/* "katana", "byobu", "scroll", "shoji", "none"; -1 for anything else */
int desk_wstrans_kind_of(const char *word);
const char *desk_wstrans_name(enum desk_wstrans_kind kind);

/* Takes the picture of the screen as it stands now and lays the sheet
 * that shows it at the top of parent, over the output. dir is +1 going to
 * a later stack, -1 an earlier one (the effects go the other way round).
 * NULL when it cannot be done here (no GLES2, no GPU buffers), and the
 * stack is then simply changed. now in seconds, on the motion clock. */
struct desk_wstrans *desk_wstrans_begin(struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, struct wlr_scene_output *so,
	struct wlr_scene_tree *parent, enum desk_wstrans_kind kind, int dir, double now);

/* A frame: false once it is over, when it has taken itself away (the
 * caller then frees it with desk_wstrans_end). */
bool desk_wstrans_frame(struct desk_wstrans *t, double now);

void desk_wstrans_end(struct desk_wstrans *t);

/* Its screen is going: end must not touch the scene output any more. */
void desk_wstrans_lost(struct desk_wstrans *t);

#endif
