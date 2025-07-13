#version 450
#extension GL_GOOGLE_include_directive : enable
#include "lighting.h"

layout (set = 3, binding = 0) uniform samplerCubeArrayShadow shadow_map[32];

float
shadow (uint map_id, uint layer, uint mat_id, vec4 pos, vec3 lpos)
{
	vec3 dir = pos.xyz - lpos;
	vec3 adir = abs(dir);
	adir = max (adir.yzx, adir.zxy);
	uvec3 bits = mix (uvec3 (0), uvec3 (0x01, 0x02, 0x04), abs(dir) >= adir);
	uint  ind  = findMSB (bits.x | bits.y | bits.z);
	float depth = shadow_mats[mat_id][3][2] / abs(dir[ind]);
	// convert from the quake frame to the cubemap frame
	dir = dir.yzx * vec3 (-1, 1, 1);
	return texture (shadow_map[map_id], vec4 (dir, layer), depth);
}

#include "lighting_main.finc"
