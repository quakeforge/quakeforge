-- Math.const

const float PI = 3.14159265;
const float E = 2.71828183;

-- Math.quaternion

vec3
qmult (vec4 q, vec3 v)
{
	float       qc = q.w;
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

-- Fragment.colormap

uniform sampler2D colormap;

vec4
mappedColor (float pix, float light)
{
	return texture2D (colormap, vec2 (pix, light));
}

-- Fragment.turb

const float SPEED = 20.0;
const float CYCLE = 128.0;
const float FACTOR = PI * 2.0 / CYCLE;
const vec2 BIAS = vec2 (1.0, 1.0)
const float SCALE = 8.0;

vec2
turb_st (vec2 st, float time)
{
	vec2        angle = st.ts * CYCLE / 2.0;
	vec2        phase = vec2 (time, time) * SPEED;
	return st + (sin ((angle + phase) * FACTOR) + BIAS) / SCALE;
}

-- Fragment.skydome

uniform sampler2D palette;
uniform sampler2D solid;
uniform sampler2D trans;
uniform float time
varying vec3 direction

const float SCALE = 189.0 / 64.0;

vec4
skydome (dir, time)
{
	float       len;
	float       pix;
	vec2        flow = vec2 (1.0, 1.0);
	vec2        st, base;
	vec3        dir = direction;

	dir.z *= 3.0;
	len = dot (dir, dir);
	len = SCALE * inversesqrt (len);

	base = dir.yx * vec2(1.0, -1.0) * len;
	st = base + flow * time / 8.0;
	pix = texture2D (trans, st).r;
	if (pix == 0.0) {
		st = base + flow * realtime / 16.0;
		pix = texture2D (solid, st).r;
	}
	return texture2D (palette, pix);
}

-- Vertex.mdl
uniform mat4 mvp_mat;
uniform mat3 norm_mat;
uniform vec2 skin_size;
uniform float blend;

attribute vec4 vcolora, vcolorb;
attribute vec2 vsta, vstb;
attribute vec3 vnormala, vnormalb;
attribute vec3 vertexa, vertexb;

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
