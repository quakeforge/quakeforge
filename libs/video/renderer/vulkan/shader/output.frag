#version 450
#extension GL_GOOGLE_include_directive : enable

layout (set = 0, binding = 0) uniform
#include "matrices.h"
;

layout (set = 1, binding = 0) uniform sampler2D Input;
layout (location = 0) out vec4 frag_color;

void
main (void)
{
	frag_color = texture (Input, gl_FragCoord.xy * ScreenSize);
}
