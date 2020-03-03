#ifndef __qwaq_screen_h
#define __qwaq_screen_h

#include <Object.h>

#include "qwaq-draw.h"
#include "qwaq-rect.h"
@class View;
@class Array;

@interface Screen: Object <HandleEvent, Draw>
{
	@public
	Rect        rect;
	Array      *views;
	Array      *event_handlers;
	View       *focusedView;
	struct window_s *window;
}
+(Screen *) screen;
-add: obj;
-setBackground: (int) ch;
@end

#endif//__qwaq_screen_h
