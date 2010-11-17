#ifndef __ruamoko_gui_Point_h
#define __ruamoko_gui_Point_h

struct Point {
	integer x;
	integer y;
};

typedef struct Point Point;

@extern Point makePoint (integer x, integer y);
@extern Point addPoint (Point a, Point b);
@extern Point subtractPoint (Point a, Point b);

#endif //__ruamoko_gui_Point_h
