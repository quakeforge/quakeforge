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

+viewFromWindow: (window_t) window
{
	return [[[self alloc] initFromWindow: window] autorelease];
}

-initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	self.rect = rect;
	self.absRect = rect;
	self.window = create_window (rect.xpos, rect.ypos, rect.xlen, rect.ylen);
	self.panel = create_panel (self.window);
	return self;
}

-initFromWindow: (window_t) window
{
	if (!(self = [super init])) {
		return nil;
	}
	rect = getwrect (window);
	self.window = window;
	self.panel = nil;
	return self;
}

-initWithPanel: (panel_t) panel
{
	if (!(self = [super init])) {
		return nil;
	}
	self.panel = panel;
	self.window = panel_window (panel);
	rect = getwrect (window);
	return self;
}

-addView: (View *) view
{
	[views addObject: view];
	return self;
}

-setBackground: (int) ch
{
	wbkgd (window, ch);
	if (!panel) {
		wrefresh (window);
	}
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	switch (event.event_type) {
		case qe_mouse:
			point.x = event.e.mouse.x;
			point.y = event.e.mouse.y;
			for (int i = [views count]; i--> 0; ) {
				View       *v = [views objectAtIndex: i];
				if (rectContainsPoint (&v.absRect, &point)) {
					[v handleEvent: event];
					break;
				}
			}
			break;
		case qe_key:
		case qe_command:
			if (focusedView) {
				[focusedView handleEvent: event];
				for (int i = [views count];
					 event.event_type != qe_none && i--> 0; ) {
					View       *v = [views objectAtIndex: i];
					 [v handleEvent: event];
				}
			}
			break;
		case qe_none:
			break;
	}
	return self;
}

@end

Rect getwrect (window_t window) = #0;
