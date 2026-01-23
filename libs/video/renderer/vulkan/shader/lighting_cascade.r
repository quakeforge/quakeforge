#include "GLSL/texture.h"
#include "GLSL/general.h"

#define SHADOW_SAMPLER @sampler(@image(float,2D,Array,Depth))
#include "lighting.h"
#include "normal_offset.r"

bool
find_cascade (float fd, uint mat_id, @out uint ind, @out float texel_size)
{
	for (uint i = 0; i < num_cascades; i++) {
		if (fd > lightmatdata[mat_id + i].cascade_distance) {
			texel_size = lightmatdata[mat_id + i].texel_size;
			ind = i;
			return true;
		}
	}
	texel_size = 1;
	ind = 0;
	return false;
}

float
shadow (uint map_id, uint layer, uint mat_id, vec4 pos, vec3 norm, vec3 lpos)
{
	float fd = pos.w;
	float texel_size = 1;
	uint ind;
	if (!find_cascade (fd, mat_id, ind, texel_size)) {
		return 1;
	}

	vec3 np = normal_offset (pos.xyz, norm, texel_size, 1);
	vec3 dir = -lpos;

	vec4 p = shadow_mats[mat_id + ind] * vec4 (np, 1);
	p = p / p.w;
	float depth = max(0, p.z);
	vec2 uv = (p.xy + vec2(1)) / 2;
	return texture (shadow_map[map_id], vec4 (uv, layer + ind, depth));
}

const vec3 debug[] = {
	vec3 (1, 0, 0),
	vec3 (1, 1, 0),
	vec3 (0, 1, 0),
	vec3 (0, 1, 1),
	vec3 (1, 1, 1),
	vec3 (1, 0, 1),
	vec3 (0, 0, 1),
};

vec4
debug_shadow (vec4 pos, uint mat_id)
{
	float fd = pos.w;
	float texel_size = 1;
	uint ind;
	if (!find_cascade (fd, mat_id, ind, texel_size)) {
		return vec4 (0.25, 0.25, 0.25, 1);
	}
	return vec4 (debug[ind], 1);
}

//#define DEBUG_SHADOW(p,lp,m) debug_shadow(p,m)

#include "lighting_main.r"
