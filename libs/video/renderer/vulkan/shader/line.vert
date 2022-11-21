#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;
layout (location = 0) in vec2 vertex;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 vcolor;

layout (location = 0) out vec4 color;

void
main (void)
{
	gl_Position = Projection2d * vec4 (vertex.xy, 0.0, 1.0);
	color = vcolor;
}
