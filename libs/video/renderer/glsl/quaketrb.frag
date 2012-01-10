uniform sampler2D palette;
uniform sampler2D texture;
uniform float realtime;

varying vec2 tst;

const float SPEED = 20.0;
const float CYCLE = 128.0;
const float PI = 3.14159;
const float FACTOR = PI * 2.0 / CYCLE;
const vec2  BIAS = vec2 (1.0, 1.0);
const float SCALE = 8.0;

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
	gl_FragColor = texture2D (palette, vec2 (pix, 0.5));
}
