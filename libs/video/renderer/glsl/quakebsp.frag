uniform sampler2D colormap;
uniform sampler2D texture;
uniform sampler2D lightmap;
uniform vec4 fog;

varying vec2 tst;
varying vec2 lst;
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

	f = exp (-sqr (fog.a * gl_FragCoord.z / gl_FragCoord.w));
	return vec4 (mix (fog_color.rgb, color.rgb, f), color.a);
}

void
main (void)
{
	float       pix = texture2D (texture, tst).r;
	float       light = texture2D (lightmap, lst).r;
	float       col;
	vec4        c;

	c = texture2D (colormap, vec2 (pix, light * 4.0)) * color;
	gl_FragColor = fogBlend (c);
}
