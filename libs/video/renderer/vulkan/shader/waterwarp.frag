#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (set = 1, binding = 0) uniform sampler2D Input;

layout (push_constant) uniform PushConstants {
	float       time;
};

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 frag_color;

const float S = 0.15625;
const float F = 2.5;
const float A = 0.01;
const vec2 B = vec2 (1, 1);
const float PI = 3.14159265;

void
main (void)
{
	vec2 st;
	st = uv * (1.0 - 2.0*A) + A * (B + sin ((time * S + F * uv.yx) * 2.0*PI));
	vec4        c = texture (Input, st);
	frag_color = c;//vec4(uv, c.x, 1);
}
