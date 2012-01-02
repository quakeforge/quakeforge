uniform sampler2D normals;
uniform mat4 mvp_mat;
uniform mat3 norm_mat;
uniform vec2 skin_size;

attribute vec4 vcolor;
attribute vec3 stn;
attribute vec3 vertex;

varying vec3 normal;
varying vec2 st;
varying vec4 color;

void
main (void)
{
	float nind;
	vec3  norma, normb;
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	st = stn.st / skin_size;
	nind = stn.p / 162.0;
	norma = texture2D (normals, vec2 (0.0, nind)).xyz;
	normb = texture2D (normals, vec2 (1.0, nind)).xyz;
	normal = norm_mat * (2.0 * norma + normb / 128.0 - vec3 (1.0, 1.0, 1.0));
	color = vcolor;
}
