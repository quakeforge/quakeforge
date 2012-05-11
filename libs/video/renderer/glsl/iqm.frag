uniform sampler2D texture;
uniform sampler2D normalmap;
uniform vec4 lights[8];
uniform vec4 fog;

varying vec3 bitangent;
varying vec3 tangent;
varying vec3 normal;
varying vec2 st;
varying vec4 color;

float
sqr (float x)
{
    return x * x;
}

vec4
fogBlend (vec4 color)
{
	float       f;
	vec4        fog_color = vec4 (fog.rgb, 1.0);

	f = exp (-sqr (fog.a / gl_FragCoord.w));
	return mix (fog_color, color, f);
}

vec3
calc_light (vec3 n, int ind)
{
	vec3        d;

	d = lights[ind].xyz - gl_FragCoord.xyz;
	return vec3 (lights[ind].w * dot (d, n) / dot (d, d));
}

void
main (void)
{
	mat3        tbn = mat3 (tangent, bitangent, normal);
	vec3        norm, light;
	vec4        col;

	norm = texture2D (normalmap, st).xyz;
	norm = tbn * norm;
	light += calc_light (norm, 0);
	light += calc_light (norm, 1);
	light += calc_light (norm, 2);
	light += calc_light (norm, 3);
	light += calc_light (norm, 4);
	light += calc_light (norm, 5);
	light += calc_light (norm, 6);
	light += calc_light (norm, 7);
	col = texture2D (texture, st) * color * vec4 (light, 1.0);
	gl_FragColor = fogBlend (col);
}
