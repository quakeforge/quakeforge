uniform mat4 mvp_mat;
attribute float vcolor;
/** Vertex position.

	x, y, z, c

	c is the color of the point.
*/
attribute vec3 vertex;

varying float color;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	gl_PointSize = max (1, 1024.0 * abs (1.0 / gl_Position.z));
	color = vcolor;
}
