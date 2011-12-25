precision mediump float;
uniform sampler2D   charmap;
uniform sampler2D   palette;
varying sv;

void
main (void)
{
	int         pix;

	pix = texture2D (charmap, sv, 0);
	gl_FragColor = texture2D (palette, vec2 (pix, 0.5), 0);
}
