#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (push_constant) uniform PushConstants {
	mat4 Model;
	float blend;
};

layout (location = 0) in vec4 vertexa;
layout (location = 1) in vec3 normala;
layout (location = 2) in vec4 vertexb;
layout (location = 3) in vec3 normalb;

void
main (void)
{
	vec4        vertex;
	vec4        pos;

	vertex = mix (vertexa, vertexb, blend);
	pos = (Model * vertex);
	gl_Position = Projection3d * (View * pos);
}
