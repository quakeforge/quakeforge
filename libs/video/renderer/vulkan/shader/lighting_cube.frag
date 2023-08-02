#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) buffer ShadowMatrices {
	mat4 shadow_mats[];
};
layout (set = 3, binding = 0) uniform samplerCubeArrayShadow shadow_map[32];

float
shadow (uint map_id, uint layer, uint mat_id, vec3 pos, vec3 lpos)
{
	vec3 dir = pos - lpos;
	vec3 adir = abs(dir);
	adir = max (adir.yzx, adir.zxy);
	uint ind = dir.x <= -adir.x ? 5
			 : dir.x >=  adir.x ? 4
			 : dir.y <= -adir.y ? 0
			 : dir.y >=  adir.y ? 1
			 : dir.z <= -adir.z ? 3
			 : dir.z >=  adir.z ? 2 : 0;
	vec4 p = shadow_mats[mat_id + ind] * vec4 (pos, 1);
	p = p / (p.w - 0.5);	//FIXME hard-coded bias
	float depth = p.z;
	dir = mat3(vec3( 0, 0, 1),
			   vec3(-1, 0, 0),
			   vec3( 0, 1, 0)) * dir;
	return texture (shadow_map[map_id], vec4 (dir, layer), depth);
}

#include "lighting_main.finc"
