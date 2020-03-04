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
	views = [[Array array] retain];
	self.rect = rect;
	window = create_window (rect.xpos, rect.ypos, rect.xlen, rect.ylen);
	panel = create_panel (window);
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	switch (event.what) {
		case qe_mouse:
			mvwprintf(window, 0, 3, "%2d %2d %08x",
					  event.mouse.x, event.mouse.y, event.mouse.buttons);
			[self redraw];
			point.x = event.mouse.x;
			point.y = event.mouse.y;
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
					 event.what != qe_none && i--> 0; ) {
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
	[view setParent: self];
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
	int x = 0, y = 0;
	for (int i = ACS_ULCORNER; i <= ACS_STERLING; i++) {
		int ch = acs_char (i);
		if (ch) {
			mvwaddch (window, x, y, ch);
		} else {
			mvwaddch (window, x, y, '.');
		}
		if (++x >= rect.xlen) {
			x = 0;
			if (++y >= rect.ylen) {
				break;
			}
		}
	}
	[views makeObjectsPerformSelector: @selector (draw)];
	return self;
}

-(Rect *) getRect
{
	return &rect;
}

-setParent: parent
{
	self.parent = parent;
	return self;
}

-redraw
{
	return [parent redraw];
}
@end
