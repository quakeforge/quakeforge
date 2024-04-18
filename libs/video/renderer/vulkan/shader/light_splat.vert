#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

#include "lighting.h"

layout (location = 0) in uint light_index;
layout (location = 1) in float light_radius;
layout (location = 2) in vec3 splat_vert;
layout (location = 0) out uint light_index_out;

vec4	// assumes a and b are unit vectors
from_to_rotation (vec3 a, vec3 b)
{
	float d = dot (a + b, a + b);
	float qc = sqrt (d);
	vec3  qv = d > 1e-6 ? cross (a, b) / qc : vec3 (0, 0, 1);
	return vec4 (qv, qc * 0.5);
}

vec3
quat_mul (vec4 q, vec3 v)
{
	vec3 uv = cross (q.xyz, v);
	vec3 uuv = cross (q.xyz, uv);
	return v + ((uv * q.w) + uuv) * 2;
}

void
main (void)
{
	LightData l = lights[light_index];
	float sz = light_radius;
	float c = l.direction.w;
	float sxy = sz * (c < 0 ? (sqrt (1 - c*c) / -c) : 1);
	vec3 scale = vec3 (sxy, sxy, sz);

	vec4 q = from_to_rotation (vec3 (0, 0, -1), l.direction.xyz);
	vec4 pos = vec4 (quat_mul (q, splat_vert * scale), 0);

	gl_Position = Projection3d * (View[gl_ViewIndex] * (pos + l.position));
	light_index_out = light_index;
}
