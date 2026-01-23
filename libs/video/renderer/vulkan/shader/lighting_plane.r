#include "GLSL/texture.h"

#define SHADOW_SAMPLER @sampler(@image(float,2D,Array,Depth))
#include "lighting.h"

#include "normal_offset.r"

float
shadow (uint map_id, uint layer, uint mat_id, vec4 pos, vec3 norm, vec3 lpos)
{
	vec4 tp = shadow_mats[mat_id] * vec4 (pos.xyz, 1);
	float texel_size = lightmatdata[mat_id].texel_size;
	vec3 np = normal_offset (pos.xyz, norm, texel_size, tp.w / tp.z);

	vec4 p = shadow_mats[mat_id] * vec4 (np, 1);
	p = p / p.w;
	float depth = p.z;
	vec2 uv = (p.xy + vec2(1)) / 2;
	return texture (shadow_map[map_id], vec4 (uv, layer, depth));
}

#include "lighting_main.r"
