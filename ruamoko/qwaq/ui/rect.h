#ifndef __qwaq_ui_rect_h
#define __qwaq_ui_rect_h

typedef struct Point_s {
	int         x;
	int         y;
} Point;

typedef struct Extent_s {
	int         width;
	int         height;
} Extent;

typedef struct Rect_s {
	Point       offset;
	Extent      extent;
} Rect;

#ifdef __QFCC__
Rect makeRect (int xpos, int ypos, int xlen, int ylen);
Point makePoint (int x, int y);
Extent makeExtent (int width, int height);
Extent mergeExtents (Extent a, Extent b);
int rectContainsPoint (Rect rect, Point point);
Rect clipRect (Rect clipRect, Rect rect);
#endif

#endif//__qwaq_ui_rect_h
