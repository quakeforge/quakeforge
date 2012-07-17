uniform sampler2D palette;
uniform sampler2D texture;
uniform float realtime;
uniform vec4 fog;

varying vec2 tst;
varying vec4 color;

const float SPEED = 20.0;
const float CYCLE = 128.0;
const float PI = 3.14159;
const float FACTOR = PI * 2.0 / CYCLE;
const vec2  BIAS = vec2 (1.0, 1.0);
const float SCALE = 8.0;

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

vec2
turb_st (vec2 st, float time)
{
	vec2        angle = st.ts * CYCLE / 2.0;
	vec2        phase = vec2 (time, time) * SPEED;
	return st + (sin ((angle + phase) * FACTOR) + BIAS) / SCALE;
}

void
main (void)
{
	float       pix;
	vec2        st;

	st = turb_st (tst, realtime);
	pix = texture2D (texture, st).r;
	gl_FragColor = fogBlend (texture2D (palette, vec2 (pix, 0.0)) * color);
}
