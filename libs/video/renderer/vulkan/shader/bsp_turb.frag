#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

#include "fog.finc"

#include "oit_store.finc"

layout (set = 3, binding = 0) uniform sampler2DArray Texture;

layout (push_constant) uniform PushConstants {
	vec4        fog;
	float       time;
	float       alpha;
	float       turb_scale;
};

layout (location = 0) in vec4 tl_st;
layout (location = 1) in vec3 direction;
layout (location = 2) in vec4 color;

layout(early_fragment_tests) in;

const float PI = 3.14159265;
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
	return st + turb_scale * (sin ((angle + phase) * FACTOR) + BIAS) / SCALE;
}

void
main (void)
{
	vec2        st = warp_st (tl_st.xy, time);
	vec4        c = texture (Texture, vec3(st, 0));
	vec4        e = texture (Texture, vec3(st, 1));
	float       a = c.a * e.a * alpha;
	c += e;
	c.a = a;
	c = FogBlend (c * color, fog);
	StoreFrag (c, gl_FragCoord.z);
}
