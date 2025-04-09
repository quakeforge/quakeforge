#ifndef __ruamoko_cl_menu_gui_Point_h
#define __ruamoko_cl_menu_gui_Point_h

/**	\addtogroup gui */
///@{

struct Point {
	int x;
	int y;
};

typedef struct Point Point;

@extern Point makePoint (int x, int y);
@extern Point addPoint (Point a, Point b);
@extern Point subtractPoint (Point a, Point b);

///@}

#endif //__ruamoko_cl_menu_gui_Point_h
