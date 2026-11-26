// The night and the storm over the desk (storm.c).
//
// One pass into a buffer laid over the desk, under every window. The scene
// lays it with premultiplied "over": out = src.rgb + dst * (1 - src.a). So
// darkening is alpha with a dark tint, and light is added with alpha 0 --
// the night, then the window's light, the lightning, the drops and the cord
// are all one colour and one alpha here.
//
// p is in screen heights, x across [0, A], y down [0, 1] (row 0 of the
// buffer is its top).

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform vec2 u_size;
uniform float u_time;     // seconds, for the rain's streaks
uniform float u_open;     // the window: 0 shut .. 1 slid open
uniform float u_flash;    // lightning, now: 0 .. 1 and a little over
uniform float u_night;    // 0 day .. 1 deep night
uniform vec4 u_cord;      // x (heights), pull 0..1, swing (radians), under the pointer
uniform vec4 u_drops[12]; // x, y, age (s; < 0 still falling), seed
uniform int u_ndrops;
uniform vec4 u_bolt;      // this strike: how near (0 far .. 1 overhead), where across the sky (-1 .. 1), its seed

float A;
const float PI = 3.14159265;
const float TAU = 6.2831853;
const vec3 MOON = vec3(0.55, 0.68, 1.0);

float sq(float x) { return x * x; }

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float noise(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
		mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

// the window up on the wall behind the desk, as its light falls on the desk:
// a trapezoid from the top edge down, wider as it comes; u across it 0..1,
// v down it 0..1, and how far inside its edge (soft, more so further out)
const float PY = 0.66;
float window_at(vec2 p, float shear, out vec2 uv)
{
	float v = p.y / PY;
	// (a strike's light falls the other way from where in the sky it is)
	float cx = A * 0.58 - shear * 0.22 * v;
	float hw = mix(0.34, 0.52, clamp(v, 0.0, 1.0));
	uv = vec2((p.x - (cx - hw)) / (2.0 * hw), v);
	float soft = 0.006 + 0.03 * v;
	float ex = min(p.x - (cx - hw), (cx + hw) - p.x);
	float ey = PY - p.y;
	return smoothstep(-soft, soft, min(ex, ey)) * step(0.0, p.y);
}

// the shoji's shadow in it: 1 where paper or open air lets light through,
// and how much of it is open (the left pane slides behind the right)
float lattice(vec2 uv, float v, out float open_air)
{
	float blur = 0.004 + 0.012 * v;
	float slid = u_open * 0.5;
	open_air = smoothstep(slid + blur, slid - blur, uv.x) * step(0.001, u_open);
	// a pane's own lattice: four across and five down, a wider frame round it
	float px = uv.x < 0.5 ? uv.x + slid : uv.x;   // the left pane, moved with its slide
	float fx = abs(fract(px * 8.0) - 0.5);         // two panes of four
	float fy = abs(fract(uv.y * 5.0) - 0.5);
	float bar = smoothstep(0.5 - 0.06 - blur * 8.0, 0.5 - 0.06 + blur * 8.0, fx)
		+ smoothstep(0.5 - 0.05 - blur * 5.0, 0.5 - 0.05 + blur * 5.0, fy);
	float frame = smoothstep(0.035 + blur, 0.035 - blur, min(min(uv.x, 1.0 - uv.x), uv.y));
	float through = 1.0 - clamp(bar + frame, 0.0, 1.0) * 0.85;
	return mix(through, 1.0 - frame * 0.85, open_air);
}

// A pine branch outside, against the window: its shadow, in the patch's own
// u (across) and v (down). A bough coming in from the left, needles in
// fans along it; it stirs a little from one strike to the next. Sharper
// the nearer the light that throws it.
float pine(vec2 uv, float soft)
{
	float sway = (u_bolt.z - 0.5) * 0.03;
	float m = 0.0;
	// the bough, thinning as it goes
	float by = 0.34 + 0.16 * uv.x + 0.05 * sin(uv.x * 6.0 + 1.3) + sway * uv.x;
	float bw = mix(0.03, 0.008, clamp(uv.x / 0.8, 0.0, 1.0));
	if (uv.x < 0.82) {
		m = max(m, 1.0 - smoothstep(bw - soft, bw + soft, abs(uv.y - by)));
	}
	// twigs and their needles, fanned
	for (int k = 0; k < 7; k++) {
		float fk = float(k);
		float t = 0.06 + fk * 0.115;
		vec2 c = vec2(t, 0.34 + 0.16 * t + 0.05 * sin(t * 6.0 + 1.3) + sway * t);
		c += vec2(0.035, (mod(fk, 2.0) < 0.5 ? -1.0 : 1.0) * 0.085);
		vec2 d = (uv - c) * vec2(1.0, 1.15);
		float r = length(d);
		float rad = 0.115 - fk * 0.008;
		if (r < rad + soft * 2.0 && r > 1e-5) {
			float ang = atan(d.y, d.x);
			// needles: fine spokes, with a ragged edge to the fan
			float needles = smoothstep(0.45, 0.9, abs(sin(ang * 17.0 + fk * 1.7)));
			float ragged = rad * (0.85 + 0.15 * sin(ang * 5.0 + fk));
			float body = 1.0 - smoothstep(rad * 0.25, rad * 0.55 + soft, r);
			float tips = (1.0 - smoothstep(ragged * 0.5, ragged + soft, r)) * needles;
			m = max(m, max(body * 0.85, tips));
		}
	}
	return clamp(m, 0.0, 1.0);
}

vec4 night_and_light(vec2 p)
{
	// the night: darker away from the lamp's pool, a cool tint in the dark
	float d = length((p - vec2(A * 0.5, 0.66)) / vec2(1.15, 0.85));
	float dark = u_night * mix(0.16, 0.6, smoothstep(0.22, 1.1, d));
	vec3 rgb = vec3(0.004, 0.007, 0.02) * dark;
	float a = dark;
	// and the lamp itself, warm, a little
	rgb += vec3(0.05, 0.03, 0.012) * u_night * (1.0 - smoothstep(0.0, 0.55, d));

	vec2 uv, uvb;
	float inside = window_at(p, 0.0, uv);           // the moon's, where it always is
	float inside_b = window_at(p, u_bolt.y, uvb);   // this strike's, leaning
	if (inside_b > 0.0 && u_flash > 0.0) {
		float open_b;
		float thru_b = lattice(uvb, uvb.y, open_b);
		float near = u_bolt.x;
		// what is outside, between the sky and the paper: sharp from a strike
		// overhead, soft from far off
		float shade_b = pine(uvb, mix(0.018, 0.004, near) + 0.01 * uvb.y);
		float bolt = u_flash * mix(0.22, 0.75, near) * mix(0.55, 1.0, open_b) * (1.0 - 0.9 * shade_b);
		// a strike far off is violet and dim, one near is white-blue
		vec3 boltc = mix(vec3(0.62, 0.58, 1.0), vec3(0.86, 0.92, 1.0), near);
		rgb += boltc * bolt * (1.0 - 0.35 * uvb.y) * inside_b * thru_b;
	}
	if (inside > 0.0) {
		float open_air;
		float thru = lattice(uv, uv.y, open_air);
		// rain going past the open half: streaks running down the light
		float streak = 0.0;
		if (open_air > 0.0) {
			vec2 q = vec2(uv.x * 70.0 + uv.y * 9.0, uv.y * 5.0 - u_time * 6.0);
			streak = smoothstep(0.55, 0.95, noise(q)) * smoothstep(0.45, 0.95, noise(q * vec2(0.3, 1.7) + 3.1));
		}
		// the pine's shadow in the moonlight, soft
		float shade_m = pine(uv, 0.02 + 0.01 * uv.y);
		float moon = mix(0.05, 0.16, open_air) * u_night * (1.0 - 0.6 * shade_m);
		float fall = (1.0 - 0.35 * uv.y) * inside * thru * (1.0 - 0.55 * streak * open_air);
		rgb += MOON * moon * fall;
	}
	// and only a little of it further out, near the window most
	float far = length((p - vec2(A * 0.58, 0.0)) / vec2(1.1, 0.9));
	rgb += vec3(0.75, 0.82, 1.0) * u_flash * mix(0.03, 0.07, u_bolt.x) * (1.0 - smoothstep(0.0, 1.0, far));
	return vec4(rgb, a);
}

// ---- drops blown in at the open window ----
vec4 drops(vec2 p, vec4 col)
{
	for (int i = 0; i < 12; i++) {
		if (i >= u_ndrops) {
			break;
		}
		vec4 dr = u_drops[i];
		vec2 c = dr.xy;
		float age = dr.z, sd = dr.w;
		if (age < 0.0) {
			// falling, slanted by the wind: a short bright streak
			float k = -age / 0.3;
			vec2 at = c + vec2(-0.05, -0.32) * k;
			vec2 dir = normalize(vec2(-0.05, -0.32));
			vec2 q = p - at;
			float along = dot(q, dir), across = dot(q, vec2(-dir.y, dir.x));
			float s = exp(-sq(across / 0.0016)) * smoothstep(0.026, 0.0, abs(along));
			col.rgb += vec3(0.7, 0.8, 1.0) * 0.55 * s;
			continue;
		}
		vec2 q = p - c;
		float r0 = 0.011 * (1.0 + 0.35 * sd);
		// the wet spot: darker wood, an uneven edge, drying away
		if (age < 30.0) {
			float ang = atan(q.y, q.x);
			float edge = r0 * (1.0 + 0.18 * (noise(vec2(ang * 1.6, sd * 9.0)) - 0.5) * 2.0);
			float wet = (1.0 - smoothstep(edge * 0.85, edge, length(q))) * (1.0 - age / 30.0);
			float dk = 0.42 * wet;
			col = vec4(col.rgb * (1.0 - dk), col.a + dk * (1.0 - col.a));
			// the bead standing on it catches the window
			float hl = exp(-sq(length(q - vec2(-0.3, -0.35) * r0) / (r0 * 0.28))) * wet;
			col.rgb += MOON * (0.18 + 0.8 * u_flash) * hl;
			// beads thrown round it
			for (int k = 0; k < 3; k++) {
				float fk = float(k);
				vec2 bp = c + vec2(cos(sd * 20.0 + fk * 2.1), sin(sd * 20.0 + fk * 2.1)) * r0 * (1.6 + fk * 0.5);
				float bd = length(p - bp);
				float bead = (1.0 - smoothstep(0.0011, 0.0016, bd)) * (1.0 - age / 30.0);
				col = vec4(col.rgb * (1.0 - 0.3 * bead), col.a + 0.3 * bead * (1.0 - col.a));
				col.rgb += MOON * 0.25 * bead * smoothstep(0.0016, 0.0, length(p - bp + vec2(0.0004)));
			}
		}
		// the crown: a ring going out and droplets thrown up from it
		if (age < 0.45) {
			float u = age / 0.45;
			float rr = 0.006 + 0.032 * sqrt(u);
			float ring = exp(-sq((length(q) - rr) / 0.0016)) * (1.0 - u);
			col.rgb += vec3(0.75, 0.85, 1.0) * 0.45 * ring;
			for (int k = 0; k < 7; k++) {
				float fk = float(k);
				float ang = fk * TAU / 7.0 + sd * 6.0;
				float h = hash(vec2(fk, sd * 31.0));
				vec2 dp = c + vec2(cos(ang), sin(ang) * 0.6) * 0.04 * u * (0.6 + 0.4 * h)
					- vec2(0.0, 0.045 * sin(PI * u) * (0.5 + 0.5 * h));
				float drop = 1.0 - smoothstep(0.0014, 0.0026, length(p - dp));
				col.rgb += vec3(0.8, 0.88, 1.0) * 0.6 * drop * (1.0 - u * 0.6);
			}
		}
	}
	return col;
}

// ---- the cord the window is opened with, hanging from the top edge ----
vec4 cord(vec2 p, vec4 col)
{
	float cx = u_cord.x, pull = u_cord.y, sw = u_cord.z, hover = u_cord.w;
	float len = 0.15 + 0.03 * pull;
	vec2 axis = vec2(sin(sw), cos(sw));
	vec2 n = vec2(axis.y, -axis.x);
	// its shadow on the desk first, soft and off to the side
	// (in a flash it is thrown long and hard down the desk, away from the
	// window, leaning as the strike is across the sky)
	float fl = clamp(u_flash, 0.0, 1.0);
	vec2 ps = p - vec2(0.007 - u_bolt.y * 0.05 * fl, 0.004);
	vec2 qs = ps - vec2(cx, 0.0);
	vec2 saxis = normalize(axis + vec2(-u_bolt.y * 0.35 * fl, 0.9 * fl));
	float slen = (len + 0.08) * (1.0 + 1.6 * fl);
	float ss = clamp(dot(qs, saxis), 0.0, slen);
	float sdist = length(qs - saxis * ss);
	float spread = mix(0.012, 0.006, fl * u_bolt.x) + 0.01 * ss;
	float sh = (1.0 - smoothstep(0.002, spread, sdist)) * mix(0.32, 0.5, fl);
	col = vec4(col.rgb * (1.0 - sh), col.a + sh * (1.0 - col.a));

	vec2 q = p - vec2(cx, 0.0);
	float s = dot(q, axis), across = dot(q, n);
	float lit = 0.75 + 0.25 * hover;
	// the braid
	float r = 0.0046;
	if (s > -0.002 && s < len && abs(across) < r) {
		float c = across / r;
		float round_ = sqrt(max(1.0 - c * c, 0.0));
		float braid = 0.6 + 0.4 * sin(s * 820.0 + c * 2.6);
		vec3 red = vec3(0.6, 0.07, 0.05) * (0.35 + 0.75 * round_) * braid * lit;
		return vec4(red + vec3(0.25, 0.12, 0.08) * pow(round_, 12.0) * 0.4, 1.0);
	}
	// the toggle: a turned piece of lacquered wood
	float ts = s - (len + 0.018);
	float tr = 0.0082 * (1.0 - 0.25 * (ts / 0.018) * (ts / 0.018));
	if (abs(ts) < 0.018 && abs(across) < tr) {
		float c = across / tr;
		float round_ = sqrt(max(1.0 - c * c, 0.0));
		vec3 wood = mix(vec3(0.16, 0.07, 0.03), vec3(0.3, 0.14, 0.06), noise(vec2(s * 400.0, c * 3.0)));
		vec3 rgb = wood * (0.3 + 0.8 * round_) * lit + vec3(1.0, 0.85, 0.7) * pow(round_, 30.0) * 0.35;
		return vec4(rgb, 1.0);
	}
	// and its tassel, the threads fanning out below
	float fs = s - (len + 0.036);
	if (fs > 0.0 && fs < 0.045) {
		float wdt = 0.0035 + fs * 0.24;
		if (abs(across) < wdt) {
			float th = 0.55 + 0.45 * sin(across / wdt * 26.0 + fs * 40.0);
			float a = (1.0 - smoothstep(0.034, 0.045, fs)) * th;
			vec3 red = vec3(0.55, 0.06, 0.05) * lit * (0.5 + 0.5 * th);
			return vec4(col.rgb * (1.0 - a) + red * a, col.a + a * (1.0 - col.a));
		}
	}
	return col;
}

void main()
{
	A = u_size.x / u_size.y;
	vec2 p = gl_FragCoord.xy / u_size.y;
	vec4 col = night_and_light(p);
	col = drops(p, col);
	col = cord(p, col);
	gl_FragColor = col;
}
