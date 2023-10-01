#version 450
#extension GL_GOOGLE_include_directive : enable
#include "lighting.h"

layout (set = 3, binding = 0) uniform sampler2DArrayShadow shadow_map[32];

float
shadow (uint map_id, uint layer, uint mat_id, vec4 pos, vec3 lpos)
{
	float fd = pos.w;
	uint ind = fd > CascadeDepths[1]
				? fd > CascadeDepths[0] ? 0 : 1
				: fd > CascadeDepths[2] ? 2 : 3;

	vec4 p = shadow_mats[mat_id + ind] * vec4 (pos.xyz, 1);
	p = p / p.w;
	float depth = p.z;
	vec2 uv = (p.xy + vec2(1)) / 2;
	return texture (shadow_map[map_id], vec4 (uv, layer + ind, depth));
}

#include "lighting_main.finc"
