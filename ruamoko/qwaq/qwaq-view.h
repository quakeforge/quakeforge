#ifndef __qwaq_view_h
#define __qwaq_view_h

#include <Array.h>
#include <Object.h>

#include "qwaq-draw.h"
#include "qwaq-rect.h"

@interface View: Object <Draw>
{
	Rect        rect;
	Rect        absRect;
	Point       point;		// can't be local :(
	struct window_s *window;
}
-initWithRect: (Rect) rect;
@end

#endif//__qwaq_view_h
