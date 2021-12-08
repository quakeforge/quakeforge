#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};
/** Vertex position.

	x, y, s, t

	\a vertex provides the onscreen location at which to draw the icon
	(\a x, \a y) and texture coordinate for the icon (\a s=z, \a t=w).
*/
layout (location = 0) in vec2 vertex;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 vcolor;

layout (location = 0) out vec2 st;
layout (location = 1) out vec4 color;

void
main (void)
{
	gl_Position = Projection2d * vec4 (vertex.xy, 0.0, 1.0);
	st = uv;
	color = vcolor;
}
