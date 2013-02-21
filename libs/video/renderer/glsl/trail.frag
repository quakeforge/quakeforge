uniform sampler2D smoke;

varying vec2 texcoord;
varying vec3 vBC;

void
main (void)
{
	gl_FragColor = texture2D (smoke, texcoord) * vec4 (1.0, 1.0, 1.0, 0.7);
}
