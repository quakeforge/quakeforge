uniform mat4 mvp_mat;
/** Vertex position.

	x, y, cx, cy

	\a vertex provides the onscreen location at which to draw the character
	(\a x, \a y) and which corner of the character cell this vertex
	represents (\a cx, \a cy). \a cx and \a cy must be either 0 or 1, or
	wierd things will happen with the character cell.
*/
attribute vec4 vertex;

/** Vectex color.

	r, g, b, a
*/
attribute vec4 vcolor;

/** The character to draw.

	The quake character map supports only 256 characters, 0-255. Any other
	value will give interesting results.
*/
attribute float dchar;

/** Coordinate in character map texture.
*/
varying vec4 color;
varying vec2 st;

void
main (void)
{
	float       row, col;
	vec2        pos, corner, uv;
	const vec2  inset = vec2 (0.03125, 0.03125);
	const vec2  size = vec2 (0.0625, 0.0625);

	row = floor (dchar / 16.0);
	col = mod (dchar, 16.0);

	pos = vertex.xy;
	corner = vertex.zw;
	uv = vec2 (col, row) + inset * (1.0 - 2.0 * corner) + corner;
	uv *= size;
	gl_Position = mvp_mat * vec4 (pos + corner * 8.0, 0.0, 1.0);
	st = uv;
	color = vcolor;
}
