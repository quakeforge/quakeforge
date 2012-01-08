uniform mat4 mvp_mat;

attribute vec4 tlst;
attribute vec4 vertex;

varying vec2 tst;
varying vec2 lst;

void
main (void)
{
	gl_Position = mvp_mat * vertex;
	tst = tlst.st;
	lst = tlst.pq;
}
