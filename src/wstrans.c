/* Going from one stack to another, in three dimensions. See wstrans.h.
 *
 * The picture of the screen as it stood is rendered with the scene into a
 * buffer of our own (wlr_scene_output_build_state with our swapchain, all
 * of it damaged first so all of it is drawn), and kept as a texture. Each
 * frame the shader (wstrans.glsl) draws into the next of a few buffers of
 * a swapchain, inside a wlroots render pass so the compositor's own GL
 * context is current with that buffer bound, and the buffer is handed to
 * a scene buffer laid over the output. Nothing is read back. */
#define _POSIX_C_SOURCE 200809L
#include "wstrans.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "wstrans_glsl.h"

static const char *const NAMES[DESK_WSTRANS_N] = {
	[DESK_WSTRANS_NONE] = "none",
	[DESK_WSTRANS_KATANA] = "katana",
	[DESK_WSTRANS_BYOBU] = "byobu",
	[DESK_WSTRANS_SCROLL] = "scroll",
	[DESK_WSTRANS_SHOJI] = "shoji",
};

/* how long each takes, in seconds of the motion clock */
static const double LENGTH[DESK_WSTRANS_N] = {
	[DESK_WSTRANS_KATANA] = 0.85,
	[DESK_WSTRANS_BYOBU] = 0.8,
	[DESK_WSTRANS_SCROLL] = 0.9,
	[DESK_WSTRANS_SHOJI] = 1.1,
};

int desk_wstrans_kind_of(const char *word)
{
	for (int i = 0; i < DESK_WSTRANS_N; i++) {
		if (word && !strcasecmp(word, NAMES[i])) {
			return i;
		}
	}
	return -1;
}

const char *desk_wstrans_name(enum desk_wstrans_kind kind)
{
	return kind >= 0 && kind < DESK_WSTRANS_N ? NAMES[kind] : "?";
}

/* The programs, one for each kind of texture the picture can come as;
 * made once, in the compositor's context, and kept (0 not yet, -1 failed). */
struct prog {
	GLint id;
	GLint pos, old, size, t, kind, dir, seed;
};
static struct prog g_prog[2];

static GLuint shader(GLenum type, const char *const *src, int n)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, n, src, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[2048] = "";
		glGetShaderInfoLog(s, sizeof log, NULL, log);
		wlr_log(WLR_ERROR, "stack change: the shader does not compile: %s", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

static struct prog *prog_for(GLenum target)
{
	int ext = target == GL_TEXTURE_EXTERNAL_OES;
	struct prog *p = &g_prog[ext];
	if (p->id != 0) {
		return p->id > 0 ? p : NULL;
	}
	p->id = -1;
	static const char *vs_src =
		"attribute vec2 pos;\n"
		"void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
	const char *fs_src[2] = {
		ext ? "#extension GL_OES_EGL_image_external : require\n#define SAMPLER samplerExternalOES\n"
			: "#define SAMPLER sampler2D\n",
		WSTRANS_GLSL,
	};
	GLuint vs = shader(GL_VERTEX_SHADER, &vs_src, 1);
	GLuint fs = shader(GL_FRAGMENT_SHADER, fs_src, 2);
	if (!vs || !fs) {
		return NULL;
	}
	GLuint id = glCreateProgram();
	glAttachShader(id, vs);
	glAttachShader(id, fs);
	glLinkProgram(id);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint ok = 0;
	glGetProgramiv(id, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[2048] = "";
		glGetProgramInfoLog(id, sizeof log, NULL, log);
		wlr_log(WLR_ERROR, "stack change: the shader does not link: %s", log);
		glDeleteProgram(id);
		return NULL;
	}
	p->id = (GLint)id;
	p->pos = glGetAttribLocation(id, "pos");
	p->old = glGetUniformLocation(id, "u_old");
	p->size = glGetUniformLocation(id, "u_size");
	p->t = glGetUniformLocation(id, "u_t");
	p->kind = glGetUniformLocation(id, "u_kind");
	p->dir = glGetUniformLocation(id, "u_dir");
	p->seed = glGetUniformLocation(id, "u_seed");
	return p;
}

struct desk_wstrans {
	struct wlr_renderer *renderer;
	struct wlr_swapchain *sc;        /* what is drawn each frame */
	struct wlr_swapchain *snap_sc;   /* the picture's */
	struct wlr_buffer *snap;
	struct wlr_texture *tex;
	struct wlr_scene_buffer *node;
	struct wlr_scene_output *so;
	enum desk_wstrans_kind kind;
	int dir;
	double t0, len;
	float seed;
	int w, h;
};

/* ARGB with the modifiers the output's own buffers are made with (the GPU
 * renders to those), else none named, which the allocator takes as its
 * own choice. 0.18 keeps the renderer's list of formats to itself. */
static void argb(struct wlr_output *out, struct wlr_drm_format *fmt, uint64_t *mods, size_t cap)
{
	static uint64_t invalid = DRM_FORMAT_MOD_INVALID;
	fmt->format = DRM_FORMAT_ARGB8888;
	fmt->len = 1;
	fmt->capacity = 1;
	fmt->modifiers = &invalid;
	if (out->swapchain && out->swapchain->format.len > 0) {
		size_t n = out->swapchain->format.len < cap ? out->swapchain->format.len : cap;
		memcpy(mods, out->swapchain->format.modifiers, n * sizeof *mods);
		fmt->len = n;
		fmt->capacity = n;
		fmt->modifiers = mods;
	}
}

void desk_wstrans_lost(struct desk_wstrans *t)
{
	if (t) {
		t->so = NULL;
	}
}

/* the picture going is not something to click: the desk under it is */
static bool passes_through(struct wlr_scene_buffer *buffer, double *sx, double *sy)
{
	(void)buffer;
	(void)sx;
	(void)sy;
	return false;
}

void desk_wstrans_end(struct desk_wstrans *t)
{
	if (!t) {
		return;
	}
	if (t->node) {
		wlr_scene_node_destroy(&t->node->node);
	}
	if (t->so) {
		/* all of it drawn again from the scene, as it is now */
		wlr_damage_ring_add_whole(&t->so->damage_ring);
		wlr_output_schedule_frame(t->so->output);
	}
	wlr_texture_destroy(t->tex);
	wlr_buffer_unlock(t->snap);
	wlr_swapchain_destroy(t->snap_sc);
	wlr_swapchain_destroy(t->sc);
	free(t);
}

struct desk_wstrans *desk_wstrans_begin(struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, struct wlr_scene_output *so,
	struct wlr_scene_tree *parent, enum desk_wstrans_kind kind, int dir, double now)
{
	if (kind <= DESK_WSTRANS_NONE || kind >= DESK_WSTRANS_N || !so || !so->output->enabled
			|| !renderer || !allocator || !wlr_renderer_is_gles2(renderer)) {
		return NULL;
	}
	struct wlr_output *out = so->output;
	struct wlr_drm_format fmtv;
	uint64_t mods[64];
	argb(out, &fmtv, mods, sizeof mods / sizeof mods[0]);
	const struct wlr_drm_format *fmt = &fmtv;
	struct desk_wstrans *t = calloc(1, sizeof *t);
	if (!t) {
		return NULL;
	}
	t->renderer = renderer;
	t->kind = kind;
	t->dir = dir < 0 ? -1 : 1;
	t->t0 = now;
	t->len = LENGTH[kind];
	t->seed = (float)((now * 1000.0) - (long)(now * 1000.0 / 997.0) * 997.0) / 997.0f;
	t->w = out->width;
	t->h = out->height;
	t->snap_sc = wlr_swapchain_create(allocator, t->w, t->h, fmt);
	t->sc = wlr_swapchain_create(allocator, t->w, t->h, fmt);
	if (!t->snap_sc || !t->sc) {
		goto fail;
	}

	/* the screen as it stands: all of it, into a buffer of our own */
	wlr_damage_ring_add_whole(&so->damage_ring);
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	struct wlr_scene_output_state_options opts = { .swapchain = t->snap_sc };
	bool ok = wlr_scene_output_build_state(so, &st, &opts)
		&& (st.committed & WLR_OUTPUT_STATE_BUFFER) && st.buffer
		&& st.buffer->width == t->w && st.buffer->height == t->h;
	if (ok) {
		t->snap = wlr_buffer_lock(st.buffer);
	}
	wlr_output_state_finish(&st);
	if (!ok) {
		wlr_log(WLR_DEBUG, "stack change: no picture of %s to be had", out->name);
		goto fail;
	}
	t->tex = wlr_texture_from_buffer(renderer, t->snap);
	if (!t->tex || !wlr_texture_is_gles2(t->tex)) {
		goto fail;
	}

	t->node = wlr_scene_buffer_create(parent, NULL);
	if (!t->node) {
		goto fail;
	}
	int lw, lh;
	wlr_output_effective_resolution(out, &lw, &lh);
	wlr_scene_node_set_position(&t->node->node, so->x, so->y);
	wlr_scene_buffer_set_dest_size(t->node, lw, lh);
	/* the pictures are in the output's own buffer space, as a scanned-out
	 * buffer is: the scene undoes the output's turn with this one */
	wlr_scene_buffer_set_transform(t->node, out->transform);
	wlr_scene_node_raise_to_top(&t->node->node);
	t->node->point_accepts_input = passes_through;
	t->so = so;
	if (!desk_wstrans_frame(t, now)) {
		goto fail;
	}
	return t;

fail:
	t->so = NULL;   /* nothing of ours was shown */
	desk_wstrans_end(t);
	return NULL;
}

bool desk_wstrans_frame(struct desk_wstrans *t, double now)
{
	double u = (now - t->t0) / t->len;
	if (u >= 1.0 || u < -0.5) {
		return false;
	}
	if (u < 0.0) {
		u = 0.0;
	}
	struct wlr_gles2_texture_attribs at;
	wlr_gles2_texture_get_attribs(t->tex, &at);
	struct wlr_buffer *buf = wlr_swapchain_acquire(t->sc, NULL);
	if (!buf) {
		return false;
	}
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(t->renderer, buf, NULL);
	if (!pass) {
		wlr_buffer_unlock(buf);
		return false;
	}
	/* the pass has the context current and this buffer bound */
	struct prog *p = prog_for(at.target);
	bool drawn = false;
	if (p) {
		static const GLfloat tri[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
		glViewport(0, 0, t->w, t->h);
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
		glUseProgram((GLuint)p->id);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(at.target, at.tex);
		glTexParameteri(at.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(at.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(at.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(at.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glUniform1i(p->old, 0);
		glUniform2f(p->size, (GLfloat)t->w, (GLfloat)t->h);
		glUniform1f(p->t, (GLfloat)u);
		glUniform1i(p->kind, t->kind);
		glUniform1f(p->dir, (GLfloat)t->dir);
		glUniform1f(p->seed, t->seed);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glVertexAttribPointer((GLuint)p->pos, 2, GL_FLOAT, GL_FALSE, 0, tri);
		glEnableVertexAttribArray((GLuint)p->pos);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glDisableVertexAttribArray((GLuint)p->pos);
		glBindTexture(at.target, 0);
		glUseProgram(0);
		drawn = true;
	}
	bool ok = wlr_render_pass_submit(pass) && drawn;
	if (ok) {
		wlr_scene_buffer_set_buffer(t->node, buf);
	}
	wlr_buffer_unlock(buf);
	return ok;
}
