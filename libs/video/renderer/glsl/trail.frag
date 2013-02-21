uniform sampler2D smoke;

varying vec2 texcoord;
varying vec3 vBC;

#if 0
void
main (void)
{
	gl_FragColor = texture2D (smoke, texcoord) * vec4 (1.0, 1.0, 1.0, 0.7);
}
#else
float
edgeFactor (void)
{
	vec3        d = fwidth (vBC);
	vec3        a3 = smoothstep (vec3 (0.0), d * 1.5, vBC);
	return min (min (a3.x, a3.y), a3.z);
}

void
main (void)
{
	gl_FragColor = vec4 (vec3 (edgeFactor ()), 0.5);
}
#endif
