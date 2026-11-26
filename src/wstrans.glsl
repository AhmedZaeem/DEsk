// Going from one stack to the next, in three dimensions (wstrans.c).
//
// One full-screen pass a frame, into a buffer the scene lays over the
// screen. u_old is the screen as it stood; what is drawn transparent shows
// the live desk underneath, which is the new stack already. Coordinates:
// p is in screen heights, x across [0, A], y down [0, 1] (row 0 of a
// wlroots buffer is its top, for the picture read and the one written
// alike). Colours are premultiplied, as the scene takes them.
//
// GLSL ES 1.00. wstrans.c puts SAMPLER (sampler2D, or samplerExternalOES
// with its extension) in front of this.

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform SAMPLER u_old;
uniform vec2 u_size;    // the buffer, in pixels
uniform float u_t;      // 0 .. 1 through the change
uniform int u_kind;     // 1 katana, 2 byobu, 3 scroll, 4 shoji
uniform float u_dir;    // +1 to a later stack, -1 to an earlier (mirrored)
uniform float u_seed;

float A;   // the screen's width in heights

const float PI = 3.14159265;
const float TAU = 6.2831853;
const vec3 L = vec3(-0.37, -0.55, -0.75);   // the lamp: up, left, in front

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float noise(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
		mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float fbm(vec2 p)
{
	float v = 0.0, a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * noise(p);
		p *= 2.03;
		a *= 0.5;
	}
	return v;
}

// the old screen at q (mirrored back when going the other way)
vec3 old_at(vec2 q)
{
	if (u_dir < 0.0) {
		q.x = A - q.x;
	}
	return texture2D(u_old, vec2(q.x / A, q.y)).rgb;
}

vec3 rot(vec3 v, vec3 k, float a)
{
	float c = cos(a), s = sin(a);
	return v * c + cross(k, v) * s + k * dot(k, v) * (1.0 - c);
}

// a ray against the plane through O spanned by U and V: where on it, and how far
bool plane_hit(vec3 ro, vec3 rd, vec3 O, vec3 U, vec3 V, out vec2 uv, out float dist)
{
	vec3 N = cross(U, V);
	float den = dot(rd, N);
	uv = vec2(0.0);
	dist = 0.0;
	if (abs(den) < 1e-6) {
		return false;
	}
	float tt = dot(O - ro, N) / den;
	if (tt <= 0.0) {
		return false;
	}
	vec3 h = ro + rd * tt - O;
	uv = vec2(dot(h, U), dot(h, V));
	dist = tt;
	return true;
}

float lit(vec3 N) { return 0.55 + 0.45 * abs(dot(normalize(N), normalize(L))); }

float ease(float x) { x = clamp(x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }

// ---- katana: one cut across, and the two halves fall away -------------
vec4 katana(vec2 p)
{
	vec2 c = vec2(A * 0.5, 0.5);
	float ang = -0.42 + (u_seed - 0.5) * 0.25;
	vec2 d = vec2(cos(ang), sin(ang));      // along the cut
	vec2 n = vec2(-d.y, d.x);               // across it
	float slash = ease(u_t / 0.14);         // the blade going through
	float e = clamp((u_t - 0.14) / 0.86, 0.0, 1.0);
	float ee = ease(e);
	vec3 ro = vec3(c, -1.8), rd = vec3(p, 0.0) - ro;
	vec4 col = vec4(0.0);
	float best = 1e9;
	for (int i = 0; i < 2; i++) {
		float s = i == 0 ? 1.0 : -1.0;
		vec3 axis = vec3(d, 0.0);
		float th = s * 1.05 * ee, tw = s * 0.16 * ee;
		vec3 U = rot(rot(vec3(1.0, 0.0, 0.0), axis, th), vec3(0.0, 0.0, 1.0), tw);
		vec3 V = rot(rot(vec3(0.0, 1.0, 0.0), axis, th), vec3(0.0, 0.0, 1.0), tw);
		// apart, down (y is down), and back from the eye
		vec3 O = vec3(c, 0.0) + vec3(n * s * 0.12 * ee, 0.0) + vec3(0.0, 1.25 * e * e, 0.7 * ee);
		vec2 uv;
		float dist;
		if (!plane_hit(ro, rd, O, U, V, uv, dist)) {
			continue;
		}
		vec2 q = c + uv;
		float side = dot(q - c, n);
		if (side * s < 0.0 || q.x < 0.0 || q.x > A || q.y < 0.0 || q.y > 1.0 || dist >= best) {
			continue;
		}
		best = dist;
		vec3 rgb = old_at(q) * lit(cross(U, V)) * (1.0 - 0.45 * ee);
		// the cut face: white-hot, cooling to an ember as it falls
		float glow = exp(-abs(side) * 220.0) * step(0.0001, e) * (1.0 - 0.8 * e);
		rgb = mix(rgb, mix(vec3(1.0, 0.9, 0.8), vec3(0.85, 0.12, 0.06), ease(e * 2.0)), clamp(glow * 1.3, 0.0, 1.0));
		col = vec4(rgb, 1.0);
	}
	// the stroke of the blade: a bright line drawn across, a red bloom round it
	float along = dot(p - c, d), across = dot(p - c, n);
	float reach = mix(-1.4, 1.4, slash);
	float on = step(along, reach) * (1.0 - smoothstep(0.0, 0.4, e));
	float ac = across / 0.0024;
	float core = exp(-ac * ac) * on;
	float halo = exp(-abs(across) * 55.0) * on * 0.55;
	float tip = exp(-length(vec2(along - reach, across)) * 35.0) * (1.0 - step(0.999, slash));
	float fa = clamp(core + halo + tip, 0.0, 1.0);
	vec3 flash = vec3(1.0, 0.97, 0.92) * (core + tip) + vec3(0.78, 0.06, 0.04) * halo;
	col = vec4(col.rgb * (1.0 - fa) + flash, max(col.a, fa));
	// sumi spattered off the blade
	if (e > 0.0) {
		for (int k = 0; k < 16; k++) {
			float fk = float(k);
			float h1 = hash(vec2(fk, u_seed * 13.0)), h2 = hash(vec2(u_seed * 7.0, fk * 1.7));
			float sg = hash(vec2(fk * 3.1, 7.0 + u_seed)) < 0.5 ? -1.0 : 1.0;
			vec2 pos = c + d * (h1 - 0.5) * 1.7 + n * sg * (0.02 + 0.22 * h2) * ease(e * 2.2)
				+ vec2(0.0, 0.4 * e * e);
			float r = (0.003 + 0.011 * h2) * (1.0 - smoothstep(0.45, 0.95, e));
			float blot = 1.0 - smoothstep(r * 0.55, r, length((p - pos) * vec2(1.0, 1.0 + 0.6 * h1)));
			col = mix(col, vec4(0.025, 0.018, 0.02, 1.0), blot * 0.92);
		}
	}
	return col;
}

// ---- byobu: the screen folds shut, six panels zig-zag to the side -------
vec4 byobu(vec2 p)
{
	float e = ease(u_t);
	float phi = 1.46 * e;
	vec3 ro = vec3(A * 0.5, 0.5, -1.8), rd = vec3(p, 0.0) - ro;
	float w = A / 6.0;
	vec3 P = vec3(-0.12 * A * e * e, 0.0, 0.5 * e * e);
	vec4 col = vec4(0.0);
	float best = 1e9;
	float k = ease(e * 5.0);
	for (int i = 0; i < 6; i++) {
		float a = mod(float(i), 2.0) < 0.5 ? phi : -phi;
		vec3 U = vec3(cos(a), 0.0, sin(a)), V = vec3(0.0, 1.0, 0.0);
		vec2 uv;
		float dist;
		if (plane_hit(ro, rd, P, U, V, uv, dist) && uv.x >= 0.0 && uv.x <= w
				&& uv.y >= 0.0 && uv.y <= 1.0 && dist < best) {
			best = dist;
			vec2 q = vec2(float(i) * w + uv.x, uv.y);
			float l = lit(cross(U, V));
			vec3 rgb = old_at(q) * l;
			// gold leaf coming up through the picture as it folds
			rgb = mix(rgb, rgb * vec3(1.08, 0.94, 0.62) + vec3(0.07, 0.045, 0.0), 0.3 * k);
			// black lacquer round each panel, brass at the hinges
			float fx = min(uv.x, w - uv.x), fy = min(uv.y, 1.0 - uv.y);
			float frame = 1.0 - smoothstep(0.0045, 0.0075, min(fx, fy));
			rgb = mix(rgb, vec3(0.05, 0.018, 0.016) * (0.6 + l), frame * k);
			float hy = min(abs(uv.y - 0.18), abs(uv.y - 0.82));
			float hinge = (1.0 - smoothstep(0.016, 0.02, hy)) * (1.0 - smoothstep(0.005, 0.008, fx));
			rgb = mix(rgb, vec3(0.78, 0.58, 0.24) * l, hinge * k);
			col = vec4(rgb, 1.0);
		}
		P += U * w;
	}
	return col * (1.0 - ease((u_t - 0.86) / 0.14));
}

// ---- scroll: rolled up as an emakimono, the silk backing outside --------
vec3 silk(float s, float y, float shade)
{
	// indigo brocade, a small gold asanoha-ish lattice
	vec2 g = vec2(s, y) * 55.0;
	float lat = abs(sin(g.x + g.y)) * abs(sin(g.x - g.y)) * abs(sin(g.x * 2.0));
	vec3 c = mix(vec3(0.07, 0.08, 0.2), vec3(0.72, 0.56, 0.22), smoothstep(0.82, 0.95, 1.0 - lat) * 0.6);
	return c * shade;
}

vec4 scroll(vec2 p)
{
	float e = ease(u_t);
	float R = 0.03 + 0.04 * e;   // thicker as it takes up more of the picture
	float X = A + R - (A + 3.0 * R) * e;   // the roll, travelling right to left
	float x = p.x;
	if (x > X + R) {
		// the roll's shadow on the desk it uncovers
		float sh = 0.42 * exp(-(x - X - R) / 0.035);
		return vec4(0.0, 0.0, 0.0, sh * (1.0 - ease((u_t - 0.9) / 0.1)));
	}
	float d = (x - X) / R;
	if (abs(d) <= 1.0) {
		float a1 = asin(clamp(d, -1.0, 1.0));
		// the outermost layer on the near side of the roll
		float af = PI - a1;
		float kf = floor((A - X - R * af) / (R * TAU));
		if (kf >= 0.0) {
			float al = af + TAU * kf;
			float shade = 0.24 + 0.76 * sqrt(max(-cos(al), 0.0));
			// the lamp's glint running along the roll, up and to the left
			float spec = 0.38 * pow(max(-cos(al + 0.55), 0.0), 28.0);
			// each turn of the silk shows as a fine seam
			float seam = 1.0 - 0.35 * exp(-mod(R * al, R * TAU) / 0.0015);
			return vec4(silk(X + R * al, p.y, shade * seam) + vec3(1.0, 0.93, 0.8) * spec, 1.0);
		}
		// the paper's end is round the back: what lies under the roll
		float ab = a1 < 0.0 ? a1 + TAU : a1;
		float kb = floor((A - X - R * ab) / (R * TAU));
		if (kb >= 0.0) {
			return vec4(silk(X + R * (ab + TAU * kb), p.y, 0.25), 1.0);
		}
		if (x > X) {
			return vec4(0.0);
		}
	}
	// still flat: the picture, in the roll's shadow close to it
	vec3 rgb = old_at(p) * (1.0 - 0.4 * exp(-(X - x) / (R * 1.4)));
	return vec4(rgb, 1.0);
}

// ---- shoji: the doors slide shut, and open on the other stack -----------
vec3 washi(vec2 q, float light)
{
	float f = fbm(q * 34.0);
	float fib = noise(q * vec2(260.0, 22.0)) * noise(q * vec2(31.0, 190.0));
	vec3 paper = vec3(0.95, 0.9, 0.8) * (0.9 + 0.1 * f) - 0.05 * fib;
	return paper * light;
}

// a door, x across [0, dw], y down [0, 1]: cedar frame, kumiko, washi, and
// the koshi-ita board along its foot
vec4 door(vec2 u, float dw, float light)
{
	float grain = 0.85 + 0.15 * noise(vec2(u.x * 9.0, u.y * 160.0));
	vec3 cedar = vec3(0.2, 0.11, 0.06) * grain;
	float fx = min(u.x, dw - u.x), fy = min(u.y, 1.0 - u.y);
	if (fx < 0.013 || fy < 0.018) {
		return vec4(cedar * 0.8, 1.0);
	}
	if (u.y > 0.83) {
		return vec4(cedar * (0.9 + 0.1 * smoothstep(0.84, 0.85, u.y)), 1.0);
	}
	float cx = abs(fract(u.x / dw * 3.0 + 0.5) - 0.5) * dw / 3.0;
	float cy = abs(fract((u.y - 0.018) / (0.812 / 6.0) + 0.5) - 0.5) * (0.812 / 6.0);
	if (cx < 0.0026 || cy < 0.0026) {
		return vec4(cedar * 1.15, 1.0);
	}
	return vec4(washi(u, light), 1.0);
}

vec4 shoji(vec2 p)
{
	float t = u_t;
	float c = t < 0.42 ? ease(t / 0.42) : t < 0.58 ? 1.0 : 1.0 - ease((t - 0.58) / 0.42);
	float cr = t < 0.42 ? ease((t - 0.05) / 0.37) : t < 0.58 ? 1.0 : 1.0 - ease((t - 0.58) / 0.36);
	float dw = A / 4.0;
	// a lantern behind the paper, flickering while they are shut
	float flick = 0.92 + 0.08 * noise(vec2(t * 30.0, u_seed * 9.0));
	float glow = (0.62 + 0.5 * exp(-pow(length((p - vec2(A * 0.5, 0.7)) * vec2(0.8, 1.2)), 2.0) * 1.4))
		* mix(1.0, flick, c);
	// the front track: the two inner doors
	float xl = p.x - (dw - (1.0 - c) * 2.0 * dw);   // the left one's own x
	float xr = p.x - (2.0 * dw + (1.0 - c) * 2.0 * dw);
	if (xl >= 0.0 && xl <= dw) {
		return door(vec2(xl, p.y), dw, glow);
	}
	if (xr >= 0.0 && xr <= dw) {
		return door(vec2(xr, p.y), dw, glow);
	}
	// the back track, a little behind (smaller, darker) and late
	vec2 pb = (p - vec2(A * 0.5, 0.5)) / 0.985 + vec2(A * 0.5, 0.5);
	float bl = pb.x - (0.0 - (1.0 - cr) * 2.0 * dw);
	float br = pb.x - (3.0 * dw + (1.0 - cr) * 2.0 * dw);
	vec4 back = vec4(0.0);
	if (pb.y >= 0.0 && pb.y <= 1.0 && bl >= 0.0 && bl <= dw) {
		back = door(vec2(bl, pb.y), dw, glow * 0.82);
	} else if (pb.y >= 0.0 && pb.y <= 1.0 && br >= 0.0 && br <= dw) {
		back = door(vec2(br, pb.y), dw, glow * 0.82);
	}
	// the front doors' shadow on what is behind them
	float edge = min(abs(xl - dw), abs(xr));
	float shadow = 0.45 * exp(-edge / 0.012) * step(0.02, c);
	if (back.a > 0.0) {
		return vec4(back.rgb * (1.0 - shadow), 1.0);
	}
	// shutting, the old stack is still behind them; opening, the new one is
	if (t < 0.5) {
		return vec4(old_at(p) * (1.0 - shadow), 1.0);
	}
	return vec4(0.0, 0.0, 0.0, shadow);
}

void main()
{
	A = u_size.x / u_size.y;
	vec2 p = gl_FragCoord.xy / u_size.y;
	if (u_dir < 0.0) {
		p.x = A - p.x;
	}
	vec4 col;
	if (u_kind == 1) {
		col = katana(p);
	} else if (u_kind == 2) {
		col = byobu(p);
	} else if (u_kind == 3) {
		col = scroll(p);
	} else {
		col = shoji(p);
	}
	col.rgb = min(col.rgb, vec3(col.a));
	gl_FragColor = col;
}
