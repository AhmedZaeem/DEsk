/* The night over the desk, and a storm outside the window. See storm.h.
 *
 * The same way of drawing as the stacks changing (wstrans.c): a shader in
 * the compositor's own GL context, inside a wlroots render pass, into the
 * next buffer of a little swapchain that a scene buffer shows as it is.
 * Drawn only while something moves -- the rain through the open window
 * (thirty times a second), a flash, a drop, the cord swinging -- or once a
 * second while a wet spot dries; at rest the last picture simply stays. */
#define _POSIX_C_SOURCE 200809L
#include "storm.h"

#include <GLES2/gl2.h>
#include <drm_fourcc.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "sound.h"
#include "storm_glsl.h"

#define DROPS 12
#define CORD_X 0.30     /* the cord, across the screen as a share of its width */

struct drop {
	double at;          /* when it lands */
	float x, y, seed;
};

struct desk_storm {
	struct wlr_renderer *renderer;
	struct wlr_swapchain *sc;
	struct wlr_scene_buffer *node;
	struct wlr_scene_output *so;
	struct wl_event_source *timer;
	void (*wake)(void *data);
	void *wake_data;
	int w, h;
	struct desk_storm_conf conf;

	bool open;              /* where the window is going */
	double open_t;          /* where it is, 0 .. 1 */
	double last;            /* the last tick */
	double pull, pull_v;    /* the cord: pulled down, springing back */
	double swing, swing_v;
	bool hover;

	double next_flash;      /* when the next one comes on its own */
	double flash_at;        /* when the last began; its pulses */
	double pulse_t[4], pulse_a[4], pulse_w[4];
	float bolt_near, bolt_across, bolt_seed;   /* this strike: how near, where across the sky */

	struct drop drop[DROPS];
	int ndrops;
	double next_drop;

	int rain;               /* the loop's handle, while it plays */
	int thunder;            /* thunder on its way (heard a moment after the flash) */
	double rain_set;        /* when it was last kept up, real time: a loop not set for a second stops */
	bool broken;            /* the shader would not build: nothing more is tried */
	double ox, oy, oscale;  /* where and how big it was laid, to see when that changes */
	int otransform;
	double rain_gain;       /* where its loudness has got to */

	double drawn_at;        /* when it was last drawn */
	bool dirty;
	unsigned rng;
};

static double real_now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double frand(struct desk_storm *s)
{
	s->rng = s->rng * 1664525u + 1013904223u;
	return (s->rng >> 8) / 16777216.0;
}

/* ---- the program ---- */

struct prog {
	GLint id;
	GLint pos, size, time, open, flash, night, cord, drops, ndrops, bolt;
};
static struct prog g_prog;

static GLuint compile(GLenum type, const char *src)
{
	GLuint sh = glCreateShader(type);
	glShaderSource(sh, 1, &src, NULL);
	glCompileShader(sh);
	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[2048] = "";
		glGetShaderInfoLog(sh, sizeof log, NULL, log);
		wlr_log(WLR_ERROR, "storm: the shader does not compile: %s", log);
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

static struct prog *prog_get(void)
{
	struct prog *p = &g_prog;
	if (p->id != 0) {
		return p->id > 0 ? p : NULL;
	}
	p->id = -1;
	GLuint vs = compile(GL_VERTEX_SHADER,
		"attribute vec2 pos;\nvoid main() { gl_Position = vec4(pos, 0.0, 1.0); }\n");
	GLuint fs = compile(GL_FRAGMENT_SHADER, STORM_GLSL);
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
		wlr_log(WLR_ERROR, "storm: the shader does not link");
		glDeleteProgram(id);
		return NULL;
	}
	p->id = (GLint)id;
	p->pos = glGetAttribLocation(id, "pos");
	p->size = glGetUniformLocation(id, "u_size");
	p->time = glGetUniformLocation(id, "u_time");
	p->open = glGetUniformLocation(id, "u_open");
	p->flash = glGetUniformLocation(id, "u_flash");
	p->night = glGetUniformLocation(id, "u_night");
	p->cord = glGetUniformLocation(id, "u_cord");
	p->drops = glGetUniformLocation(id, "u_drops");
	p->ndrops = glGetUniformLocation(id, "u_ndrops");
	p->bolt = glGetUniformLocation(id, "u_bolt");
	return p;
}

/* ---- lightning ---- */

static double flash_now(const struct desk_storm *s, double now)
{
	double t = now - s->flash_at, f = 0.0;
	if (s->flash_at <= 0.0 || t < 0.0 || t > 2.0) {
		return 0.0;
	}
	for (int k = 0; k < 4; k++) {
		/* each flicker comes up over a few frames and dies away over a
		 * quarter of a second: a light outside, not the screen blinking */
		double u = t - s->pulse_t[k];
		if (u < 0.0 || s->pulse_a[k] <= 0.0) {
			continue;
		}
		double up = u < 0.045 ? u / 0.045 : 1.0;
		up = up * up * (3.0 - 2.0 * up);
		f += s->pulse_a[k] * up * exp(-fmax(u - 0.045, 0.0) / s->pulse_w[k]);
	}
	return fmin(f, 1.0);
}

void desk_storm_flash(struct desk_storm *s, double now)
{
	/* one at a time, till the last has quite gone: a new one over it would
	 * cut it off, or throw its light from somewhere else all at once */
	if (!s || s->flash_at > 0.0) {
		return;
	}
	s->flash_at = now;
	/* far off, behind the paper of the window: one flash, sometimes a
	 * second fainter one after it */
	int n = frand(s) < 0.45 ? 2 : 1;
	double t = 0.0;
	for (int k = 0; k < 4; k++) {
		s->pulse_t[k] = t;
		s->pulse_a[k] = k < n ? (k == 0 ? 0.7 + 0.3 * frand(s) : 0.35 + 0.3 * frand(s)) : 0.0;
		s->pulse_w[k] = 0.18 + 0.12 * frand(s);
		t += 0.16 + 0.14 * frand(s);
	}
	/* how far off it struck, and where across the sky: the light, its
	 * shadows and the thunder all go by it */
	double dist = frand(s);
	static const char *force;   /* DESK_STORM_NEAR, for trying it: 0 far .. 1 overhead */
	static bool looked;
	if (!looked) {
		looked = true;
		force = getenv("DESK_STORM_NEAR");
	}
	if (force && *force) {
		dist = 1.0 - fmin(fmax(atof(force), 0.0), 1.0);
	}
	s->bolt_near = (float)(1.0 - dist);
	s->bolt_across = (float)(frand(s) * 2.0 - 1.0);
	s->bolt_seed = (float)frand(s);
	/* near, it is brighter and comes on harder */
	for (int k = 0; k < 4; k++) {
		s->pulse_a[k] *= 0.6 + 0.4 * (1.0 - dist);
	}
	double delay = 0.25 + 3.0 * dist;
	double gain = 1.0 - 0.6 * dist;
	double rate = 1.0 - 0.25 * dist;
	/* heard only through the open window: shut, the storm is seen, not heard */
	if (s->thunder) {
		desk_sound_cancel(s->thunder);   /* (the last one's, if it has not begun) */
	}
	s->thunder = 0;
	if (s->open) {
		s->thunder = desk_sound_play_in(DESK_SND_THUNDER, (CORD_X * 2.0 - 1.0) * 0.3 + 0.16, gain,
			rate, delay);
	}
	s->dirty = true;
}

/* ---- drops ---- */

static void drops_spawn(struct desk_storm *s, double now)
{
	double a = (double)s->w / s->h;
	int n = 1 + (int)(frand(s) * 2.5);
	for (int k = 0; k < n; k++) {
		/* the oldest gives way */
		if (s->ndrops == DROPS) {
			memmove(&s->drop[0], &s->drop[1], (DROPS - 1) * sizeof s->drop[0]);
			s->ndrops--;
		}
		int i = s->ndrops++;
		/* on the open half of the window's light, not far from the wall */
		double v = 0.12 + 0.6 * frand(s);
		double hw = 0.34 + (0.52 - 0.34) * v;
		double u = 0.05 + 0.4 * frand(s);
		s->drop[i].x = (float)(a * 0.58 - hw + 2.0 * hw * u);
		s->drop[i].y = (float)(v * 0.66);
		s->drop[i].seed = (float)frand(s);
		s->drop[i].at = now + 0.3 + 0.25 * k + 0.2 * frand(s);
	}
	s->dirty = true;
}

static void drops_age(struct desk_storm *s, double now)
{
	int j = 0;
	for (int i = 0; i < s->ndrops; i++) {
		if (now - s->drop[i].at < 30.0) {
			s->drop[j++] = s->drop[i];
		}
	}
	if (j != s->ndrops) {
		s->dirty = true;
	}
	s->ndrops = j;
}

/* ---- drawing ---- */

static bool draw(struct desk_storm *s, double now)
{
	struct prog *p;
	struct wlr_buffer *buf = wlr_swapchain_acquire(s->sc, NULL);
	if (!buf) {
		return false;
	}
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(s->renderer, buf, NULL);
	if (!pass) {
		wlr_buffer_unlock(buf);
		return false;
	}
	p = prog_get();
	if (!p) {
		s->broken = true;
	}
	if (p) {
		static const GLfloat tri[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
		GLfloat d[DROPS * 4];
		for (int i = 0; i < s->ndrops; i++) {
			d[i * 4 + 0] = s->drop[i].x;
			d[i * 4 + 1] = s->drop[i].y;
			d[i * 4 + 2] = (GLfloat)(now - s->drop[i].at);
			d[i * 4 + 3] = s->drop[i].seed;
		}
		glViewport(0, 0, s->w, s->h);
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
		glUseProgram((GLuint)p->id);
		glUniform2f(p->size, (GLfloat)s->w, (GLfloat)s->h);
		glUniform1f(p->time, (GLfloat)fmod(now, 3600.0));
		glUniform1f(p->open, (GLfloat)s->open_t);
		glUniform1f(p->flash, (GLfloat)flash_now(s, now));
		glUniform1f(p->night, (GLfloat)s->conf.night);
		glUniform4f(p->cord, (GLfloat)(CORD_X * s->w / s->h), (GLfloat)s->pull,
			(GLfloat)s->swing, s->hover ? 1.0f : 0.0f);
		if (s->ndrops > 0) {
			glUniform4fv(p->drops, s->ndrops, d);
		}
		glUniform1i(p->ndrops, s->ndrops);
		glUniform4f(p->bolt, s->bolt_near, s->bolt_across, s->bolt_seed, 0.0f);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glVertexAttribPointer((GLuint)p->pos, 2, GL_FLOAT, GL_FALSE, 0, tri);
		glEnableVertexAttribArray((GLuint)p->pos);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glDisableVertexAttribArray((GLuint)p->pos);
		glUseProgram(0);
	}
	bool ok = wlr_render_pass_submit(pass) && p;
	if (ok) {
		wlr_scene_buffer_set_buffer(s->node, buf);
		s->drawn_at = now;
		s->dirty = false;
	}
	wlr_buffer_unlock(buf);
	return ok;
}

/* ---- the clock it keeps ---- */

static int timer_fired(void *data)
{
	struct desk_storm *s = data;
	if (s->wake) {
		s->wake(s->wake_data);
	}
	return 0;
}

static void arm(struct desk_storm *s, double now, double when)
{
	if (!s->timer) {
		return;
	}
	double in = when - now;
	int ms = in <= 0.0 ? 1 : in > 3600.0 ? 3600000 : (int)ceil(in * 1000.0);
	wl_event_source_timer_update(s->timer, ms);
}

static bool passes_through(struct wlr_scene_buffer *b, double *sx, double *sy)
{
	(void)b;
	(void)sx;
	(void)sy;
	return false;
}

struct desk_storm *desk_storm_create(struct wlr_renderer *renderer,
	struct wlr_allocator *allocator, struct wlr_scene_output *so,
	struct wlr_scene_tree *parent, struct wl_event_loop *loop,
	void (*wake)(void *data), void *data)
{
	if (!renderer || !allocator || !so || !wlr_renderer_is_gles2(renderer)) {
		return NULL;
	}
	struct wlr_output *out = so->output;
	if (out->width < 64 || out->height < 64) {
		return NULL;
	}
	struct desk_storm *s = calloc(1, sizeof *s);
	if (!s) {
		return NULL;
	}
	s->renderer = renderer;
	s->so = so;
	s->w = out->width;
	s->h = out->height;
	s->wake = wake;
	s->wake_data = data;
	s->rng = 0x9E3779B9u ^ (unsigned)(uintptr_t)s;
	s->conf = (struct desk_storm_conf){ .lightning = true, .night = 0.6, .rain_gain = 0.6 };
	/* ARGB with the output's own buffers' modifiers (the GPU renders to
	 * those), else the allocator's own choice */
	uint64_t mods[64], invalid = DRM_FORMAT_MOD_INVALID;
	struct wlr_drm_format fmt = { .format = DRM_FORMAT_ARGB8888, .len = 1, .capacity = 1,
		.modifiers = &invalid };
	if (out->swapchain && out->swapchain->format.len > 0) {
		size_t n = out->swapchain->format.len < 64 ? out->swapchain->format.len : 64;
		memcpy(mods, out->swapchain->format.modifiers, n * sizeof *mods);
		fmt.len = fmt.capacity = n;
		fmt.modifiers = mods;
	}
	s->sc = wlr_swapchain_create(allocator, s->w, s->h, &fmt);
	s->node = s->sc ? wlr_scene_buffer_create(parent, NULL) : NULL;
	if (!s->node) {
		wlr_swapchain_destroy(s->sc);
		free(s);
		return NULL;
	}
	int lw, lh;
	wlr_output_effective_resolution(out, &lw, &lh);
	wlr_scene_node_set_position(&s->node->node, so->x, so->y);
	wlr_scene_buffer_set_dest_size(s->node, lw, lh);
	wlr_scene_buffer_set_transform(s->node, out->transform);
	s->node->point_accepts_input = passes_through;
	s->ox = so->x;
	s->oy = so->y;
	s->oscale = out->scale;
	s->otransform = out->transform;
	if (loop) {
		s->timer = wl_event_loop_add_timer(loop, timer_fired, s);
	}
	s->dirty = true;
	return s;
}

void desk_storm_lost(struct desk_storm *s)
{
	if (s) {
		s->so = NULL;
	}
}

void desk_storm_destroy(struct desk_storm *s)
{
	if (!s) {
		return;
	}
	if (s->rain) {
		desk_sound_stop(s->rain, 0.4);
	}
	if (s->thunder) {
		desk_sound_cancel(s->thunder);
		desk_sound_stop(s->thunder, 0.3);
	}
	if (s->timer) {
		wl_event_source_remove(s->timer);
	}
	wlr_scene_node_destroy(&s->node->node);
	wlr_swapchain_destroy(s->sc);
	free(s);
}

struct wlr_output *desk_storm_output(const struct desk_storm *s)
{
	return s && s->so ? s->so->output : NULL;
}

bool desk_storm_stale(const struct desk_storm *s)
{
	if (!s || !s->so) {
		return true;
	}
	struct wlr_output *o = s->so->output;
	return o->width != s->w || o->height != s->h || o->scale != s->oscale
		|| (int)o->transform != s->otransform || s->so->x != s->ox || s->so->y != s->oy;
}

void desk_storm_configure(struct desk_storm *s, const struct desk_storm_conf *conf)
{
	if (!s || !conf) {
		return;
	}
	if (memcmp(&s->conf, conf, sizeof *conf) != 0) {
		s->conf = *conf;
		s->next_flash = 0.0;   /* chosen again */
		s->dirty = true;
	}
}

bool desk_storm_cord_hit(const struct desk_storm *s, double lx, double ly)
{
	if (!s || !s->so) {
		return false;
	}
	int lw, lh;
	wlr_output_effective_resolution(s->so->output, &lw, &lh);
	double x = (lx - s->so->x) / lh, y = (ly - s->so->y) / lh;
	double cx = CORD_X * lw / lh;
	double len = 0.15 + 0.03 * s->pull + 0.085;   /* the cord, its toggle and tassel */
	return y >= 0.0 && y <= len && fabs(x - cx - sin(s->swing) * y) < 0.018;
}

void desk_storm_hover(struct desk_storm *s, bool over)
{
	if (s && s->hover != over) {
		s->hover = over;
		s->dirty = true;
		if (s->wake) {
			s->wake(s->wake_data);
		}
	}
}

bool desk_storm_window_open(const struct desk_storm *s)
{
	return s && s->open;
}

void desk_storm_pull(struct desk_storm *s, double now)
{
	if (!s) {
		return;
	}
	s->open = !s->open;
	if (!s->open && s->thunder) {
		/* shut: thunder not yet heard is not heard, and what is rolling fades */
		desk_sound_cancel(s->thunder);
		desk_sound_stop(s->thunder, 0.35);
		s->thunder = 0;
	}
	s->pull_v = 2.6;
	s->swing_v += (frand(s) - 0.5) * 1.4;
	desk_sound_play(DESK_SND_KEY_PRESS, CORD_X * 2.0 - 1.0, 0.5, 0.8);
	desk_sound_play_in(DESK_SND_SLIDE_WOOD_QUICK, 0.16, 0.7, 1.15, 0.12);
	s->next_drop = s->open ? now + 1.0 + 2.0 * frand(s) : 0.0;
	s->dirty = true;
	if (s->wake) {
		s->wake(s->wake_data);
	}
}

bool desk_storm_tick(struct desk_storm *s, double now, bool paused)
{
	if (!s || !s->so || s->broken) {
		return false;
	}
	double dt = s->last > 0.0 ? fmin(fmax(now - s->last, 0.0), 0.05) : 0.016;
	s->last = now;
	bool moving = false;

	/* the window sliding: two thirds of a second end to end */
	double to = s->open ? 1.0 : 0.0;
	if (fabs(s->open_t - to) > 1e-4) {
		double step = dt / 0.65;
		s->open_t = s->open_t < to ? fmin(s->open_t + step, to) : fmax(s->open_t - step, to);
		moving = true;
	}
	/* the cord: a spring back up, and a pendulum */
	s->pull_v += (-90.0 * s->pull - 9.0 * s->pull_v) * dt;
	s->pull += s->pull_v * dt;
	s->swing_v += (-26.0 * s->swing - 4.5 * s->swing_v) * dt;   /* settled in about two seconds */
	s->swing += s->swing_v * dt;
	if (fabs(s->pull) + fabs(s->pull_v) + fabs(s->swing) + fabs(s->swing_v) > 4e-3) {
		moving = true;
	} else {
		s->pull = s->pull_v = s->swing = s->swing_v = 0.0;
	}

	/* lightning on its own, somewhere between half a minute and a minute and a half */
	if (paused || !s->conf.lightning) {
		s->next_flash = 0.0;   /* chosen afresh when it may come */
	} else if (s->next_flash <= 0.0) {
		s->next_flash = now + (s->conf.test ? 3.0 + 4.0 * frand(s) : 25.0 + 65.0 * frand(s));
	}
	if (s->next_flash > 0.0 && now >= s->next_flash) {
		desk_storm_flash(s, now);
		s->next_flash = 0.0;
	}
	bool flashing = flash_now(s, now) > 0.002 || (s->flash_at > 0.0 && now - s->flash_at < 2.0);
	if (!flashing && s->flash_at > 0.0) {
		/* over: drawn once more, with none of it left */
		s->flash_at = 0.0;
		s->dirty = true;
	}

	/* drops, only through an open window */
	if (s->open && s->open_t > 0.5 && !paused) {
		if (s->next_drop <= 0.0) {
			s->next_drop = now + (s->conf.test ? 1.5 : 3.0) + (s->conf.test ? 2.0 : 8.0) * frand(s);
		}
		if (now >= s->next_drop) {
			drops_spawn(s, now);
			s->next_drop = 0.0;
		}
	} else {
		s->next_drop = 0.0;
	}
	drops_age(s, now);
	bool splashing = false, drying = false;
	for (int i = 0; i < s->ndrops; i++) {
		double age = now - s->drop[i].at;
		splashing = splashing || age < 0.5;
		drying = true;
	}

	/* the rain heard through the open window, faded in and out */
	double want = !paused && s->open ? s->conf.rain_gain * s->open_t : 0.0;
	s->rain_gain += (want - s->rain_gain) * fmin(dt * 2.5, 1.0);
	double rt = real_now();
	if (s->rain && rt - s->rain_set > 0.9) {
		/* not kept up for a second (the screen was dark, the loop stalled):
		 * it has stopped by itself, so start it again */
		desk_sound_stop(s->rain, 0.0);
		s->rain = 0;
	}
	if (s->rain_gain > 0.01) {
		s->rain_set = rt;
		if (!s->rain) {
			s->rain = desk_sound_loop(DESK_SND_RAIN, 0.16, s->rain_gain);
		} else {
			desk_sound_set(s->rain, 0.16, s->rain_gain);   /* (a loop not set for a second stops) */
		}
	} else if (s->rain) {
		desk_sound_stop(s->rain, 0.3);
		s->rain = 0;
	}

	/* drawn: every frame while something moves or flashes, thirty times a
	 * second for the rain through the window, once a second while a spot dries */
	bool raining = s->open_t > 0.0 && !paused;
	double period = moving || flashing || splashing ? 0.0 : raining ? 1.0 / 30.0 : drying ? 1.0 : -1.0;
	if (s->dirty || (period >= 0.0 && now - s->drawn_at >= period - 0.002)) {
		draw(s, now);
	}

	/* the next time anything happens with nobody asking: the rain's next
	 * picture by the timer, so the screens are not all asked for a frame
	 * every vblank for it */
	double next = s->next_flash > 0.0 ? s->next_flash : now + 3600.0;
	if (raining && !(moving || flashing || splashing) && s->drawn_at + 1.0 / 30.0 < next) {
		next = s->drawn_at + 1.0 / 30.0;
	}
	if (s->next_drop > 0.0 && s->next_drop < next) {
		next = s->next_drop;
	}
	if (s->rain && now + 0.5 < next) {
		next = now + 0.5;   /* the loop kept up */
	}
	if (drying && now + 1.0 < next) {
		next = now + 1.0;
	}
	arm(s, now, next);
	return moving || flashing || splashing;
}
