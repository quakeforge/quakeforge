#ifndef __qwaq_rect_h
#define __qwaq_rect_h

typedef struct Rect_s {
	int         xpos;
	int         ypos;
	int         xlen;
	int         ylen;
} Rect;

typedef struct {
	int         x;
	int         y;
} Point;

@extern Rect makeRect (int xpos, int ypos, int xlen, int ylen);
//XXX will not work if point or rect point to a local variabl
@extern int rectContainsPoint (Rect *rect, Point *point);
@extern Rect getwrect (struct window_s *window);

#endif//__qwaq_rect_h
