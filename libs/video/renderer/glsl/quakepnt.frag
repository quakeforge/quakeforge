//precision mediump float;
uniform sampler2D palette;

varying float color;

void
main (void)
{
	if (color == 1.0)
		discard;
	gl_FragColor = texture2D (palette, vec2 (color, 0.0));
}
