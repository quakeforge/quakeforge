uniform sampler2D palette;
uniform sampler2D solid;
uniform sampler2D trans;
uniform float realtime;
uniform vec4 fog;

varying vec3 direction;

const float SCALE = 189.0 / 64.0;

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
	gl_FragColor = fogBlend (texture2D (palette, vec2 (pix, 0.0)));
}
