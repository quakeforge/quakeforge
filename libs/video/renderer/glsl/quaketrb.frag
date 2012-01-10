uniform sampler2D palette;
uniform sampler2D texture;
uniform float realtime;

varying vec2 tst;

const float SPEED = 20.0;
const float CYCLE = 128.0;
const float PI = 3.14159;
const float FACTOR = PI * 2.0 / CYCLE;

void
main (void)
{
	float       pix;
	vec2        rt = vec2 (realtime, realtime);
	vec2        st;

	st = tst + sin ((tst.ts * 64.0 + rt * SPEED) * FACTOR);
	pix = texture2D (texture, st).r;
	gl_FragColor = texture2D (palette, vec2 (pix, 0.5));
}
