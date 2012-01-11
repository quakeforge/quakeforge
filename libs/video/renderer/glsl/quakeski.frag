uniform sampler2D palette;
uniform sampler2D solid;
uniform sampler2D trans;
uniform float realtime;

varying vec3 direction;

const float SCALE = 189.0 / 64.0;

void
main (void)
{
	float       len;
	float       pix;
	vec2        flow = vec2 (1.0, 1.0);
	vec2        st;
	vec3        dir = direction;

	dir.z *= 3.0;
	len = dot (dir, dir);
	len = SCALE * inversesqrt (len);

	st = direction.xy * len + flow * realtime / 8.0;
	pix = texture2D (trans, st).r;
	if (pix == 0.0) {
		st = direction.xy * len + flow * realtime / 16.0;
		pix = texture2D (solid, st).r;
	}
	gl_FragColor = texture2D (palette, vec2 (pix, 0.5));
}
