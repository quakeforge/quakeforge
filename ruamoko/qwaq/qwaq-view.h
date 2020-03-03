#ifndef __qwaq_view_h
#define __qwaq_view_h

#include <Array.h>
#include <Object.h>

#include "qwaq-draw.h"
#include "qwaq-rect.h"

@interface View: Object <Draw>
{
	@public
	Rect        rect;
	Rect        absRect;
	Point       point;		// can't be local :(
	id          parent;
	struct window_s *window;
}
-initWithRect: (Rect) rect;
@end

#endif//__qwaq_view_h
