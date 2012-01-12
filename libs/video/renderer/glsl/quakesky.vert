uniform mat4 mvp_mat;
uniform mat4 sky_mat;

attribute vec4 tlst;
attribute vec4 vertex;

varying vec3 direction;

void
main (void)
{
	gl_Position = mvp_mat * vertex;
	direction = (sky_mat * vertex).xyz;
}
