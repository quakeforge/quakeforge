uniform mat4 mvp_mat;
uniform mat4 sky_mat;
uniform vec3 origin;

attribute vec4 tlst;
attribute vec4 vertex;

varying vec3 direction;

void
main (void)
{
	gl_Position = mvp_mat * vertex;
	direction = vertex.xyz - origin;
}
