#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection;
	mat4 View;
	mat4 Model;
};
/** Vertex position.

	x, y, s, t

	\a vertex provides the onscreen location at which to draw the icon
	(\a x, \a y) and texture coordinate for the icon (\a s=z, \a t=w).
*/
layout (location = 0) in vec4 vertex;
layout (location = 1) in vec4 vcolor;

layout (location = 0) out vec2 st;
layout (location = 1) out vec4 color;

void
main (void)
{
	gl_Position = Projection * vec4 (vertex.xy, 0.0, 1.0);
	st = vertex.zw;
	color = vcolor;
}
