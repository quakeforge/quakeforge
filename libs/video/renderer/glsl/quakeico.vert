uniform mat4 mvp_mat;
/** Vertex position.

	x, y, s, t

	\a vertex provides the onscreen location at which to draw the icon
	(\a x, \a y) and texture coordinate for the icon (\a s=z, \a t=w).
*/
attribute vec4 vertex;
attribute vec4 vcolor;

varying vec4 color;
varying vec2 st;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex.xy, 0.0, 1.0);
	st = vertex.zw;
	color = vcolor;
}
