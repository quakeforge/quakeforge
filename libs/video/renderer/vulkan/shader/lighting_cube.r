#define SHADOW_SAMPLER @sampler(@image(float,Cube,Array,Depth))
#include "lighting.h"
#include "general.h"
#include "integer.h"
#include "texture.h"

#include "normal_offset.r"

float
shadow (uint map_id, uint layer, uint mat_id, vec4 pos, vec3 norm, vec3 lpos)
{
	float texel_size = lightmatdata[mat_id].texel_size;
	vec3 np = normal_offset (pos.xyz, norm, texel_size, 1 / pos.w);

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

#include "lighting_main.r"
