uniform sampler2D palette;
uniform sampler2D colormap;
uniform sampler2D skin;
uniform float ambient;
uniform float shadelight;
uniform vec3 lightvec;

varying vec3 normal;
varying vec2 st;
varying vec4 color;

void
main (void)
{
	float       pix = texture2D (skin, st).r;
	float       light = ambient;
	float       d, col;

	d = dot (normal, lightvec);
	d = min (d, 0.0);
	light = 255.0 - light;
	light += d * shadelight;
	col = texture2D (colormap, vec2 (pix, light / 255.0)).r;
	gl_FragColor = texture2D (palette, vec2 (col, 0.0));
}
