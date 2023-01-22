#version 450

layout (location = 0) out vec2 st;

void
main ()
{
	float       x = (gl_VertexIndex & 2);
	float       y = (gl_VertexIndex & 1);
	gl_Position = vec4 (2, 4, 0, 1) * vec4 (x, y, 0, 1) - vec4 (1, 1, 0, 0);
	st = vec2(1, 2) * vec2(x, y);
}
