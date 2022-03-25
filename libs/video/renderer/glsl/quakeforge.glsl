-- Math.const

const float PI = 3.14159265;
const float E = 2.71828183;

-- Math.quaternion

vec3
qmult (vec4 q, vec3 v)
{
	float       qs = q.w;
	vec3        qv = q.xyz;
	vec3        t = cross (qv, v);
	return (qs * qs) * v + 2.0 * qs * t + dot (qv, v) * qv + cross (qv, t);
}

vec3
dqtrans (vec4 q0, vec4 qe)
{//2.0 * (q0.w * qe.xyz - qe.w * q0.xyz - cross (qe.xyz, q0.xyz));
	float       qs = q0.w, Ts = qe.w;
	vec3        qv = -q0.xyz, Tv = qe.xyz;

	return 2.0 * (Ts * qv + qs * Tv + cross (Tv, qv));
}

-- Fragment.fog

uniform vec4 fog;

vec4
fogBlend (vec4 color)
{
	float       fog_factor;
	float       az = fog.a * gl_FragCoord.z / gl_FragCoord.w;
	vec4        fog_color = vec4 (fog.rgb, 1.0);

	fog_factor = exp (-az * az);
	return vec4 (mix (fog_color.rgb, color.rgb, fog_factor), color.a);
}

-- Fragment.palette

uniform sampler2D palette;

vec4
palettedColor (float pix)
{
	return texture2D (palette, vec2 (pix, 0.0));
}

-- Fragment.colormap

uniform sampler2D colormap;

vec4
mappedColor (float pix, float light)
{
	return texture2D (colormap, vec2 (pix, light));
}

-- env.warp.nop

vec2
warp_st (vec2 st, float time)
{
	return st;
}

-- env.warp.turb

const float SPEED = 20.0;
const float CYCLE = 128.0;
const float FACTOR = PI * 2.0 / CYCLE;
const vec2 BIAS = vec2 (1.0, 1.0);
const float SCALE = 8.0;

vec2
warp_st (vec2 st, float time)
{
	vec2        angle = st.ts * CYCLE / 2.0;
	vec2        phase = vec2 (time, time) * SPEED;
	return st + (sin ((angle + phase) * FACTOR) + BIAS) / SCALE;
}

-- env.sky.cube

uniform samplerCube sky;

vec4
sky_color (vec3 dir)
{
	// NOTE: quake's world and GL's world are rotated relative to each other
	// quake has x right, y in, z up. gl has x right, y up, z out
	// The textures are loaded with GL's z (quake's y) already negated, so
	// all that's needed here is to swizzle y and z.
	return textureCube(sky, dir.xzy);
}

-- env.sky.id

uniform sampler2D solid;
uniform sampler2D trans;
uniform float time;

const float SCALE = 189.0 / 64.0;

vec4
sky_color (vec3 dir)
{
	float       len;
	float       pix;
	vec2        flow = vec2 (1.0, 1.0);
	vec2        st, base;

	dir.z *= 3.0;
	len = dot (dir, dir);
	len = SCALE * inversesqrt (len);

	base = dir.yx * vec2(1.0, -1.0) * len;
	st = base + flow * time / 8.0;
	pix = texture2D (trans, st).r;
	if (pix == 0.0) {
		st = base + flow * time / 16.0;
		pix = texture2D (solid, st).r;
	}
	return palettedColor (pix);
}

-- Vertex.mdl

uniform mat4 mvp_mat;
uniform mat3 norm_mat;
uniform vec2 skin_size;
uniform float blend;

attribute vec3 vertexa, vertexb;
attribute vec3 vnormala, vnormalb;
attribute vec2 vsta, vstb;
attribute vec4 vcolora, vcolorb;

varying vec3 normal;
varying vec2 st;
varying vec4 color;

void
main (void)
{
    vec3        vertex;
	vec3        vnormal;

	vertex = mix (vertexa, vertexb, blend);
	vnormal = mix (vnormala, vnormalb, blend);
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	st = mix (vsta, vstb, blend) / skin_size;
	normal = norm_mat * vnormal;
	color = mix (vcolora, vcolorb, blend);
}

-- Fragment.mdl

uniform sampler2D skin;
uniform float ambient;
uniform float shadelight;
uniform vec3 lightvec;

varying vec3 normal;
varying vec2 st;
varying vec4 color;

void
main (void)
{
	float       pix = texture2D (skin, st).r;
	float       light = ambient;
	float       d, col;
	vec4        lit;

	d = dot (normal, lightvec);
	d = min (d, 0.0);
	light = 255.0 - light;
	light += d * shadelight;
	lit = mappedColor (pix, light / 255.0);
	gl_FragColor = fogBlend (lit * color);
}

-- Vertex.bsp

uniform mat4 mvp_mat;
uniform mat4 sky_mat;

attribute vec4 vertex;
attribute vec4 tlst;
attribute vec4 vcolor;

varying vec2 tst;
varying vec2 lst;
varying vec4 color;
varying vec3 direction;

void
main (void)
{
	gl_Position = mvp_mat * vertex;
	direction = (sky_mat * vertex).xyz;
	tst = tlst.st;
	lst = tlst.pq;
	color = vcolor;
}

-- Fragment.bsp.lit

uniform sampler2D texture;
uniform sampler2D lightmap;
uniform float time;

varying vec2 tst;
varying vec2 lst;
varying vec4 color;

void
main (void)
{
	float       pix;
	vec2        st;
	float       light = texture2D (lightmap, lst).r;
	vec4        c;

	st = warp_st (tst, time);
	pix = texture2D (texture, st).r;
	c = mappedColor (pix, light * 4.0) * color;
	gl_FragColor = fogBlend (c);
}

-- Fragment.bsp.unlit

uniform sampler2D texture;
uniform float time;

varying vec2 tst;
varying vec4 color;

void
main (void)
{
	float       pix;
	vec2        st;
	vec4        c;

	st = warp_st (tst, time);
	pix = texture2D (texture, st).r;
	c = palettedColor (pix) * color;
	gl_FragColor = fogBlend (c);
}


-- Fragment.bsp.sky

varying vec3 direction;

void
main (void)
{
	vec4        c = sky_color (direction);
	gl_FragColor = fogBlend (c);
}


-- Vertex.particle.point

uniform mat4 mvp_mat;
/** Vertex position.

	x, y, z, c

	c is the color of the point.
*/
attribute vec3 vertex;
attribute float vcolor;

varying float color;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	gl_PointSize = max (1.0, 1024.0 * abs (1.0 / gl_Position.z));
	color = vcolor;
}

-- Vertex.particle.textured

uniform mat4 mvp_mat;
/** Vertex position.

	x, y, z, c

	c is the color of the point.
*/
attribute vec3 vertex;
attribute vec2 vst;
attribute vec4 vcolor;

varying vec4 color;
varying vec2 st;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	color = vcolor;
	st = vst;
}

-- Fragment.particle.point

//precision mediump float;

varying float color;

void
main (void)
{
	if (color == 1.0)
		discard;
	gl_FragColor = fogBlend (palettedColor (color));
}

-- Fragment.particle.textured

//precision mediump float;
uniform sampler2D texture;

varying vec4 color;
varying vec2 st;

void
main (void)
{
	gl_FragColor = fogBlend (texture2D (texture, st) * color);
}

-- Vertex.sprite

uniform mat4 mvp_mat;
attribute vec3 vertexa, vertexb;
attribute vec4 uvab;	///< ua va ub vb
attribute float vblend;	//FIXME why is this not a uniform?
attribute vec4 vcolora, vcolorb;

varying float blend;
varying vec4 colora, colorb;
varying vec2 sta, stb;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (mix (vertexa, vertexb, vblend), 1.0);
	blend = vblend;
	colora = vcolora;
	colorb = vcolorb;
	sta = uvab.xy;
	stb = uvab.zw;
}

-- Fragment.sprite

uniform sampler2D spritea;
uniform sampler2D spriteb;

varying float blend;
varying vec4 colora, colorb;
varying vec2 sta, stb;

void
main (void)
{
	float       pixa, pixb;
	vec4        cola, colb;
	vec4        col;

	pixa = texture2D (spritea, sta).r;
	pixb = texture2D (spriteb, stb).r;
	if (pixa == 1.0 && pixb == 1.0)
		discard;
	cola = palettedColor (pixa) * colora;
	colb = palettedColor (pixb) * colorb;
	col = mix (cola, colb, blend);
	if (col.a == 0.0)
		discard;
	gl_FragColor = fogBlend (col);
}

-- Vertex.2d

uniform mat4 mvp_mat;
/** Vertex position.

	x, y, s, t

	\a vertex provides the onscreen location at which to draw the icon
	(\a x, \a y) and texture coordinate for the icon (\a s=z, \a t=w).
*/
attribute vec4 vertex;
attribute vec4 vcolor;

varying vec4 color;
varying vec2 st;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex.xy, 0.0, 1.0);
	st = vertex.zw;
	color = vcolor;
}

-- Fragment.2d

//precision mediump float;
uniform sampler2D   texture;
varying vec4 color;
varying vec2 st;

void
main (void)
{
	float       pix;

	pix = texture2D (texture, st).r;
	if (pix == 1.0)
		discard;
	gl_FragColor = palettedColor (pix) * color;
}

-- Vertex.iqm

uniform mat4 mvp_mat;
uniform mat3 norm_mat;
uniform mat4 bonemats[80];
attribute vec4 vcolor;
attribute vec4 vweights;
attribute vec4 vbones;
attribute vec4 vtangent;
attribute vec3 vnormal;
attribute vec2 texcoord;
attribute vec3 vposition;

varying vec3 position;
varying vec3 bitangent;
varying vec3 tangent;
varying vec3 normal;
varying vec2 st;
varying vec4 color;

void
main (void)
{
	mat4        m;
	vec4        q0, qe;
	vec3        sh, sc, tr, v, n, t;

	m  = bonemats[int (vbones.x)] * vweights.x;
	m += bonemats[int (vbones.y)] * vweights.y;
	m += bonemats[int (vbones.z)] * vweights.z;
	m += bonemats[int (vbones.w)] * vweights.w;
#if 0
	q0 = m[0];
	qe = m[1];
	sh = m[2].xyz;
	sc = m[3].xyz;

	// extract translation from dual quaternion
	tr = dqtrans (q0, qe);
	// apply rotation and translation
	v = qmult (q0, vposition) + tr;
	// apply shear
	v.z += v.y * sh.z + v.x * sh.y;
	v.y += v.x * sh.x;
	// apply scale
	v *= sc;
	// rotate normal (won't bother with shear or scale: not super accurate,
	// but probably good enough)
	n = qmult (q0, vnormal);
	// rotate tangent (won't bother with shear or scale: not super accurate,
	// but probably good enough)
	t = qmult (q0, vtangent.xyz);
#else
	mat3 nm = mat3 (m[0].xyz, m[1].xyz, m[2].xyz);
	v = (m * vec4 (vposition, 1.0)).xyz;
	n = nm * vnormal;
	t = nm * vtangent.xyz;
#endif
	position = v;
	normal = norm_mat * n;
	tangent = norm_mat * t;
	bitangent = cross (normal, tangent) * vtangent.w;
	color = vcolor;
	st = texcoord;
	gl_Position = mvp_mat * vec4 (position, 1.0);
}

-- Fragment.iqm

struct light {
	vec4        position;		// xyz = pos, w = strength
	vec4        color;			// rgb. a = ?
};
uniform sampler2D texture;
uniform sampler2D normalmap;
uniform vec3 ambient;
uniform light lights[8];

varying vec3 position;
varying vec3 bitangent;
varying vec3 tangent;
varying vec3 normal;
varying vec2 st;
varying vec4 color;

vec3
calc_light (vec3 n, int ind)
{
	vec3        d;
	light       l = lights[ind];
	float       mag;

	d = l.position.xyz - position;
	mag = dot (d, n);
	mag = max (0.0, mag);
	return l.color.rgb * (l.position.w * mag / dot (d, d));
}

void
main (void)
{
	mat3        tbn = mat3 (tangent, bitangent, normal);
	vec3        norm, l;
	vec4        col;

	norm = (texture2D (normalmap, st).xyz - vec3(0.5)) * 2.0;
	norm = tbn * norm;
	l = ambient;
	l += calc_light (norm, 0);
	l += calc_light (norm, 1);
	l += calc_light (norm, 2);
	l += calc_light (norm, 3);
	l += calc_light (norm, 4);
	l += calc_light (norm, 5);
	l += calc_light (norm, 6);
	l += calc_light (norm, 7);
	col = texture2D (texture, st) * color * vec4 (l, 1.0);
	gl_FragColor = fogBlend (col);
}

-- Vertex.fstri
#version 130

out vec2 uv;

void
main ()
{
	// quake uses clockwise triangle order
	float       x = (gl_VertexID & 2);
	float       y = (gl_VertexID & 1);
	uv = vec2(x, y*2);
	gl_Position = vec4 (2, 4, 0, 0) * vec4 (x, y, 0, 0) - vec4 (1, 1, 0, -1);
}

-- Fragment.screen.warp

uniform sampler2D screenTex;
uniform float time;

in vec2 uv;

const float S = 0.15625;
const float F = 2.5;
const float A = 0.01;
const vec2 B = vec2 (1, 1);

void
main ()
{
	uv = uv * (1.0 - 2.0*A) + A * (B + sin ((time * S + F * uv.yx) * 2.0*PI));
	vec4        c = texture2D (screenTex, uv);
	gl_FragColor = c;//vec4(uv, c.x, 1);
}

-- Fragment.screen.fisheye

uniform samplerCube screenTex;
uniform float fov;
uniform float aspect;

in vec2 uv;

void
main ()
{
	// slight offset on y is to avoid the singularity straight aheat
	vec2        xy = (2.0 * uv - vec2 (1, 1.00002)) * (vec2(1, -aspect));
	float       r = sqrt (dot (xy, xy));
	vec2        cs = vec2 (cos (r * fov), sin (r * fov));
	vec3        dir = vec3 (cs.y * xy / r, cs.x);

	vec4        c = textureCube(screenTex, dir);
	gl_FragColor = c;// * 0.001 + vec4(dir, 1);
}
