#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 3, binding = 0) uniform sampler2DArrayShadow shadow_map[32];

float
shadow (uint map_id, uint layer, uint mat_id, vec3 pos)
{
	return 1;
}

#include "lighting_main.finc"
