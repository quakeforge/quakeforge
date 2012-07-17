//precision mediump float;
uniform sampler2D texture;
uniform vec4 fog;

varying vec4 color;
varying vec2 st;

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
	gl_FragColor = fogBlend (texture2D (texture, st) * color);
}
