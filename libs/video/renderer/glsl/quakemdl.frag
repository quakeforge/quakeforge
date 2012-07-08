uniform sampler2D colormap;
uniform sampler2D skin;
uniform float ambient;
uniform float shadelight;
uniform vec3 lightvec;
uniform vec4 fog;

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

void
main (void)
{
	float       pix = texture2D (skin, st).r;
	float       light = ambient;
	float       d, col;
	vec4        lit;

	d = dot (normal, lightvec);
	d = min (d, 0.0);
	light = 255.0 - light;
	light += d * shadelight;
	lit = texture2D (colormap, vec2 (pix, light / 255.0));
	gl_FragColor = fogBlend (lit * color);
}
