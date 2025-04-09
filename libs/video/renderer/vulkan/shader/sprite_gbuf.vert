#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_multiview : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (set = 1, binding = 0) uniform Vertices {
	vec4 xyuv[4];
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
	int  frame;
};

layout (location = 0) out vec2 st;
layout (location = 1) out vec4 position;
layout (location = 2) out vec3 normal;

void
main (void)
{
	vec4        v = xyuv[frame * 4 + gl_VertexIndex];

	vec4        pos = Model[3];
	pos += v.x * Model[1] + v.y * Model[2];

	gl_Position = Projection3d * (View[gl_ViewIndex] * pos);
	st = v.zw;
	position = pos;
	normal = -vec3(Model[0]);
}
