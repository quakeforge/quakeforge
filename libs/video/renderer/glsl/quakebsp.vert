uniform mat4 mvp_mat;

attribute vec4 vcolor;
attribute vec4 tlst;
attribute vec4 vertex;

varying vec2 tst;
varying vec2 lst;
varying vec4 color;

void
main (void)
{
	gl_Position = mvp_mat * vertex;
	tst = tlst.st;
	lst = tlst.pq;
	color = vcolor;
}
