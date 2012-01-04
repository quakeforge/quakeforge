uniform mat4 mvp_mat;
/** Vertex position.

	x, y, z, c

	c is the color of the point.
*/
attribute vec4 vertex;

varying float color;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex.xyz, 1.0);
	gl_PointSize = max (1, 32768.0 * abs (1.0 / gl_Position.z));
	color = vertex.w;
}
