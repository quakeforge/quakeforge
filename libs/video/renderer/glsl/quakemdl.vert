uniform mat4 mvp_mat;
uniform mat3 norm_mat;
uniform vec2 skin_size;

attribute vec4 vcolor;
attribute vec2 vst;
attribute vec3 vnormal;
attribute vec3 vertex;

varying vec3 normal;
varying vec2 st;
varying vec4 color;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	st = vst / skin_size;
	normal = norm_mat * vnormal;
	color = vcolor;
}
