uniform mat4 mvp_mat;
attribute float vblend;
attribute vec4 vcolora, vcolorb;
attribute vec4 uvab;	///< ua va ub vb
attribute vec3 vertexa, vertexb;

varying float blend;
varying vec4 colora, colorb;
varying vec2 sta, stb;

void
main (void)
{
	gl_Position = mvp_mat * vec4 (mix (vertexa, vertexb, vblend), 1.0);
	blend = vblend;
	colora = vcolora;
	colorb = vcolorb;
	sta = uvab.xy;
	stb = uvab.zw;
}
