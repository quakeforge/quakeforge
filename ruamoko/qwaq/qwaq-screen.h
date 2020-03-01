#ifndef __qwaq_screen_h
#define __qwaq_screen_h

#include <Object.h>

#include "qwaq-rect.h"
@class View;
@class Array;

@interface Screen: Object
{
	Rect        rect;
	Array      *views;
	View       *focusedView;
	struct window_s *window;
}
+(Screen *) screen;
-add: obj;
-setBackground: (int) ch;
@end

#endif//__qwaq_screen_h
