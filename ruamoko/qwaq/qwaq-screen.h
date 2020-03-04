#ifndef __qwaq_screen_h
#define __qwaq_screen_h

#include <Object.h>

#include "qwaq-draw.h"
#include "qwaq-rect.h"
@class View;
@class Array;

@interface Screen: Object <HandleMouseEvent, Draw>
{
	Rect        rect;
	Array      *views;
	Array      *event_handlers;
	Array      *focused_handlers;
	Array      *mouse_handlers;
	Array      *mouse_handler_rects;
	View       *focusedView;
	struct window_s *window;
}
+(Screen *) screen;
-add: obj;
-setBackground: (int) ch;
-printf: (string) fmt, ...;
@end

#endif//__qwaq_screen_h
