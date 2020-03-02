#include <Array.h>

#include "event.h"
#include "qwaq-curses.h"
#include "qwaq-window.h"
#include "qwaq-view.h"

@implementation Window

+windowWithRect: (Rect) rect
{
	return [[[self alloc] initWithRect: rect] autorelease];
}

-initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	self.rect = rect;
	window = create_window (rect.xpos, rect.ypos, rect.xlen, rect.ylen);
	panel = create_panel (window);
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

-addView: (View *) view
{
	[views addObject: view];
	view.absRect.xpos = view.rect.xpos + rect.xpos;
	view.absRect.ypos = view.rect.ypos + rect.ypos;
	view.window = window;
	return self;
}

-setBackground: (int) ch
{
	wbkgd (window, ch);
	return self;
}

-takeFocus
{
	return self;
}

-loseFocus
{
	return self;
}

-draw
{
	return self;
}
@end
