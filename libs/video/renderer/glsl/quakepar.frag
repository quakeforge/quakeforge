//precision mediump float;
uniform sampler2D texture;

varying vec4 color;
varying vec2 st;

void
main (void)
{
	gl_FragColor = texture2D (texture, st) * color;
}
