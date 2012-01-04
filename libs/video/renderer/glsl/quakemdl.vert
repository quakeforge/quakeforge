uniform mat4 mvp_mat;
uniform mat3 norm_mat;
uniform vec2 skin_size;
uniform float blend;

attribute vec4 vcolora, vcolorb;
attribute vec2 vsta, vstb;
attribute vec3 vnormala, vnormalb;
attribute vec3 vertexa, vertexb;

varying vec3 normal;
varying vec2 st;
varying vec4 color;

void
main (void)
{
	vec3        vertex;
	vec3        vnormal;

	vertex = mix (vertexa, vertexb, blend);
	vnormal = mix (vnormala, vnormalb, blend);
	gl_Position = mvp_mat * vec4 (vertex, 1.0);
	st = mix (vsta, vstb, blend) / skin_size;
	normal = norm_mat * vnormal;
	color = mix (vcolora, vcolorb, blend);
}
