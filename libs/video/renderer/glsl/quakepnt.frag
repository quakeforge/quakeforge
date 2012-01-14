//precision mediump float;
varying float color;

void
main (void)
{
	if (color == 1.0)
		discard;
	gl_FragColor = texture2D (palette, vec2 (pix, 0.0));
}
