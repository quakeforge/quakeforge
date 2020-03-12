#include "qwaq-rect.h"

Rect
clipRect (Rect clipRect, Rect rect)
{
	if (rect.offset.x < clipRect.offset.x) {
		int         dist = clipRect.offset.x - rect.offset.x;
		rect.offset.x += dist;
		rect.extent.width -= dist;
	}
	if (rect.offset.y < clipRect.offset.y) {
		int         dist = clipRect.offset.y - rect.offset.y;
		rect.offset.y += dist;
		rect.extent.height -= dist;
	}
	if (rect.offset.x + rect.extent.width > clipRect.extent.width) {
		rect.extent.width = clipRect.extent.width - rect.offset.x;
	}
	if (rect.offset.y + rect.extent.height > clipRect.extent.height) {
		rect.extent.height = clipRect.extent.height - rect.offset.y;
	}
	return rect;
}

Rect
makeRect (int xpos, int ypos, int xlen, int ylen)
{
	Rect rect = {{xpos, ypos}, {xlen, ylen}};
	return rect;
}

Point makePoint (int x, int y)
{
	Point p = {x, y};
	return p;
}

Extent makeExtent (int width, int height)
{
	Extent e = {width, height};
	return e;
}

int
rectContainsPoint (Rect *rect, Point *point)
{
	return ((point.x >= rect.offset.x
			 && point.x < rect.offset.x + rect.extent.width)
			&& (point.y >= rect.offset.y
				&& point.y < rect.offset.y + rect.extent.height));
}
