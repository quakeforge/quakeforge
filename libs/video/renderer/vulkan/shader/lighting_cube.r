#include "GLSL/general.h"
#include "GLSL/texture.h"

#define SHADOW_SAMPLER @sampler(@image(float,Cube,Array,Depth))
#include "lighting.h"

#include "normal_offset.r"

uint
cube_ind (vec3 dir)
{
	vec3 adir = abs (dir);
	adir = max (adir.yzx, adir.zxy);
	uvec3 bits = mix (uvec3 (0), uvec3 (0x01, 0x02, 0x04), abs(dir) >= adir);
	return findMSB (bits.x | bits.y | bits.z);
}

float
shadow (uint map_id, uint layer, uint mat_id, vec4 pos, vec3 norm, vec3 lpos)
{
	vec3 np = pos.xyz;
	vec3 dir = np - lpos;
	uint ind = cube_ind (dir);
	float depth = abs(dir[ind]) / near_plane;

	float texel_size = lightmatdata[mat_id].texel_size;
	np = normal_offset (np, norm, texel_size, depth);

	dir = np - lpos;
	ind = cube_ind (dir);
	depth = near_plane / abs(dir[ind]);
	// convert from the quake frame to the cubemap frame
	dir = dir.yzx * vec3 (-1, 1, 1);
	return texture (shadow_map[map_id], vec4 (dir, layer), depth);
}

const vec3 debug[7] = {
	vec3 (1, 0, 0),
	vec3 (1, 1, 0),
	vec3 (0, 1, 0),
	vec3 (0, 1, 1),
	vec3 (1, 1, 1),
	vec3 (1, 0, 1),
	vec3 (0, 0, 1),
};

vec4
debug_shadow (vec4 pos, vec3 lpos, uint mat_id)
{
	vec3 np = pos.xyz;//normal_offset (pos.xyz, norm, texel_size, 1 / pos.w);

	vec3 dir = np - lpos;
	uint ind = cube_ind (dir);
	return vec4 (2*debug[ind], 1);
}

//#define DEBUG_SHADOW(p,lp,m) debug_shadow(p,lp,m)

#include "lighting_main.r"
