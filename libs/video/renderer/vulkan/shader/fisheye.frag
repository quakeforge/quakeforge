#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (set = 1, binding = 0) uniform samplerCube Input;

layout (push_constant) uniform PushConstants {
	float       fov;
	float       aspect;
};

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 frag_color;

void
main ()
{
	// slight offset on y is to avoid the singularity straight ahead
	vec2        xy = (2.0 * uv - vec2 (1, 1.00002)) * (vec2(1, -aspect));
	float       r = sqrt (dot (xy, xy));
	vec2        cs = vec2 (cos (r * fov), sin (r * fov));
	vec3        dir = vec3 (cs.y * xy / r, cs.x);

	vec4        c = texture (Input, dir);
	frag_color = c;// * 0.001 + vec4(dir, 1);
}
