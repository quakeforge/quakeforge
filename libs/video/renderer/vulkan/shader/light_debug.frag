#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) flat in uint light_index;
layout (location = 0) out vec4 frag_color;

void
main (void)
{
	frag_color = gl_FrontFacing ? vec4 (1, 0, 1, 1) : vec4 (0, 1, 1, 1);
}
