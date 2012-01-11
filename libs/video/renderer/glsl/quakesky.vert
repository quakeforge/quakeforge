uniform mat4 mvp_mat;
uniform mat4 sky_mat;

attribute vec4 vertex;

varying vec3 direction;

void
main (void)
{
	vec3        dir;
	float       len;
	vec2        offset = vec2 (1.0, 1.0);

	gl_Position = mvp_mat * vertex;
	direction = (sky_mat * vertex).xyz;
}
