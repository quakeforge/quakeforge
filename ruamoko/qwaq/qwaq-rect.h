#ifndef __qwaq_rect_h
#define __qwaq_rect_h

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
@extern Rect makeRect (int xpos, int ypos, int xlen, int ylen);
@extern Point makePoint (int x, int y);
@extern Extent makeExtent (int width, int height);
@extern Extent mergeExtents (Extent a, Extent b);
@extern int rectContainsPoint (Rect rect, Point point);
@extern Rect clipRect (Rect clipRect, Rect rect);
#endif

#endif//__qwaq_rect_h
