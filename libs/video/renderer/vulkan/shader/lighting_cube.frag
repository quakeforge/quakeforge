#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) buffer ShadowMatrices {
	mat4 shadow_mats[];
};
layout (set = 3, binding = 0) uniform samplerCubeArrayShadow shadow_map[32];

float
shadow (uint map_id, uint layer, uint mat_id, vec3 pos)
{
	vec4 p = shadow_mats[mat_id] * vec4 (pos, 1);
	float depth = (p / p.w).z;
	return texture (shadow_map[map_id], vec4 (p.xyz, layer), depth);
}

#include "lighting_main.finc"
