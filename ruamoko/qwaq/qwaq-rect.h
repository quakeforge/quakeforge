#ifndef __qwaq_rect_h
#define __qwaq_rect_h

typedef struct {
	int         x;
	int         y;
} Point;

typedef struct {
	int         width;
	int         height;
} Extent;

typedef struct Rect_s {
	Point       offset;
	Extent      extent;
} Rect;

@extern Rect makeRect (int xpos, int ypos, int xlen, int ylen);
//XXX will not work if point or rect point to a local variabl
@extern int rectContainsPoint (Rect *rect, Point *point);
@extern Rect getwrect (struct window_s *window);

#endif//__qwaq_rect_h
