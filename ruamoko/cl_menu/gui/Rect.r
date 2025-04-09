#include <Object.h>
#include "Point.h"
#include "Size.h"
#include "Rect.h"

Rect makeRect (int x, int y, int w, int h)
{
	//FIXME Rect r = {{x, y}, {w, h}};
	Rect r;
	r.origin = makePoint (x, y);
	r.size = makeSize (w, h);
	return r;
}

Rect makeRectFromOriginSize (Point origin, Size size)
{
	Rect r;

	r.origin = origin;
	r.size = size;

	return r;
}
