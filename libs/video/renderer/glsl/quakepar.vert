uniform mat4 mvp_mat;
attribute vec4 vcolor;
attribute vec2 vst;
/** Vertex position.

	x, y, z, c

	c is the color of the point.
*/
attribute vec3 vertex;

varying vec4 color;
varying vec2 st;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	color = vcolor;
	st = vst;
}
