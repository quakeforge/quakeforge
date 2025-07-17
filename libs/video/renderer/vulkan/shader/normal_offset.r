#include "lighting.h"

vec3 normal_offset (vec3 pos, vec3 n, float texel_size, float depth_scale)
{
	float shadowMapTexelSize = texel_size * depth_scale;
	return pos + shadowMapTexelSize * n;
}
