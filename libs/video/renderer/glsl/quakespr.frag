uniform sampler2D spritea;
uniform sampler2D spriteb;
uniform sampler2D palette;
uniform vec4 fog;

varying float blend;
varying vec4 colora, colorb;
varying vec2 sta, stb;

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
	return vec4 (mix (fog_color.rgb, color.rgb, f), color.a);
}

void
main (void)
{
	float       pixa, pixb;
	vec4        cola, colb;
	vec4        col;

	pixa = texture2D (spritea, sta).r;
	pixb = texture2D (spriteb, stb).r;
	if (pixa == 1.0 && pixb == 1.0)
		discard;
	cola = texture2D (palette, vec2 (pixa, 0.0)) * colora;
	colb = texture2D (palette, vec2 (pixb, 0.0)) * colorb;
	col = mix (cola, colb, blend);
	if (col.a == 0.0)
		discard;
	gl_FragColor = fogBlend (col);
}
