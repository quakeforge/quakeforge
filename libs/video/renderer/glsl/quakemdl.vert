uniform sampler2D normals;
uniform mat4 mvp_mat;
uniform mat3 norm_mat;

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
	st = stn.st;
	nind = stn.p;
	norma = texture2D (normals, vec2 (nind, 0.0)).xyz;
	normb = texture2D (normals, vec2 (nind, 1.0)).xyz;
	normal = norm_mat * (norma + normb / 256.0);
	color = vcolor;
}
