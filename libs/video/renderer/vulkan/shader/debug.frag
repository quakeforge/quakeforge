#version 450
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	frag_color = gl_FrontFacing ? vec4 (0, 1, 0, 1) : vec4 (1, 0, 0, 1);
}
