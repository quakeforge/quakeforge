#ifndef __qwaq_view_h
#define __qwaq_view_h

#include <Array.h>
#include <Object.h>

#include "qwaq-rect.h"

@interface View: Object
{
	Rect        rect;
	Rect        absRect;
	Point       point;		// can't be local :(
	struct window_s *window;
}
-initWithRect: (Rect) rect;
-handleEvent: (struct qwaq_event_s *) event;
@end

#endif//__qwaq_view_h
