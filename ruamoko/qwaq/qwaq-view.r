#include "qwaq-curses.h"
#include "qwaq-view.h"

Rect
makeRect (int xpos, int ypos, int xlen, int ylen)
{
	Rect rect = {xpos, ypos, xlen, ylen};
	return rect;
}

int
rectContainsPoint (Rect *rect, Point *point)
{
	return ((point.x >= rect.xpos && point.x < rect.xpos + rect.xlen)
			&& (point.y >= rect.ypos && point.y < rect.ypos + rect.ylen));
}

@implementation View

-initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	self.rect = rect;
	self.absRect = rect;
	self.window = nil;
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	return self;
}

@end

Rect getwrect (window_t window) = #0;
