#version 450

layout (location = 0) in uint light_index;
layout (location = 0) out uint light_index_out;

void
main ()
{
	float       x = (gl_VertexIndex & 2);
	float       y = (gl_VertexIndex & 1);
	gl_Position = vec4 (2, 4, 0, 1) * vec4 (x, y, 0, 1) - vec4 (1, 1, 0, 0);
	light_index_out = light_index;
}
