#version 450
#extension GL_GOOGLE_include_directive : enable
#include "lighting.h"

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
	p = p / p.w;
	float depth = p.z;
	// convert from the quake frame to the cubemap frame
	dir = dir.yzx * vec3 (-1, 1, 1);
	return texture (shadow_map[map_id], vec4 (dir, layer), depth);
}

#include "lighting_main.finc"
