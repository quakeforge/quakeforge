#ifndef __qwaq_view_h
#define __qwaq_view_h

#include <Array.h>
#include <Object.h>

#include "event.h"
#include "qwaq-curses.h"

typedef struct {
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
@extern Rect getwrect (window_t window);

@interface View: Object
{
	Rect        rect;
	Rect        absRect;
	Array      *views;
	Point       point;		// can't be local :(
	View       *focusedView;
	window_t    window;
	panel_t     panel;
}
+viewFromWindow: (window_t) window;
-initWithRect: (Rect) rect;
-handleEvent: (qwaq_event_t *) event;
-addView: (View *) view;
-setBackground: (int) ch;
@end

#endif//__qwaq_view_h
