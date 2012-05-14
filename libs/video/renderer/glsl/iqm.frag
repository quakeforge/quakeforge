struct light {
	vec4        position;		// xyz = pos, w = strength
	vec4        color;			// rgb. a = ?
};
uniform sampler2D texture;
uniform sampler2D normalmap;
uniform light lights[8];
uniform vec4 fog;

varying vec3 position;
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
	light       l = lights[ind];
	float       mag;

	d = position - l.position.xyz;
	mag = dot (d, n);
	mag = max (0.0, mag);
	return l.color.rgb * (l.position.w * mag / dot (d, d));
}

void
main (void)
{
	mat3        tbn = mat3 (tangent, bitangent, normal);
	vec3        norm, l;
	vec4        col;

	norm = texture2D (normalmap, st).xyz;
	norm = tbn * norm;
	l  = calc_light (norm, 0);
	l += calc_light (norm, 1);
	l += calc_light (norm, 2);
	l += calc_light (norm, 3);
	l += calc_light (norm, 4);
	l += calc_light (norm, 5);
	l += calc_light (norm, 6);
	l += calc_light (norm, 7);
	col = texture2D (texture, st) * color * vec4 (l, 1.0);
	gl_FragColor = fogBlend (col);
}
