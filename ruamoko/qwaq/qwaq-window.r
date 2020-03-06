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
	buffer = [[TextContext alloc] initWithRect: rect];
	textContext = buffer;
	panel = create_panel ([buffer window]);
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
/*	switch (event.what) {
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
	}*/
	return self;
}

-addView: (View *) view
{
/*	[views addObject: view];
	view.absRect.xpos = view.rect.xpos + rect.xpos;
	view.absRect.ypos = view.rect.ypos + rect.ypos;
	view.window = window;
	[view setOwner: self];*/
	return self;
}

-setBackground: (int) ch
{
	[buffer bkgd: ch];
	return self;
}

-draw
{
	static box_sides_t box_sides = {
		ACS_VLINE, ACS_VLINE,
		ACS_HLINE, ACS_HLINE,
	};
	static box_corners_t box_corners = {
		ACS_ULCORNER, ACS_URCORNER,
		ACS_LLCORNER, ACS_LRCORNER,
	};
	if (box_sides.ls == ACS_VLINE) {
		int        *foo = &box_sides.ls;
		for (int i = 0; i < 8; i++) {
			foo[i] = acs_char (foo[i]);
		}
	}
	[super draw];
	[buffer border: box_sides, box_corners];
	Point pos = { 1, 1 };
	//for (int i = ACS_ULCORNER; i <= ACS_STERLING; i++) {
	for (int i = 32; i <= 127; i++) {
		int ch = acs_char (i);
		if (ch) {
			[self mvaddch: pos, ch];
		} else {
			[self mvaddch: pos, '.'];
		}
		if (++pos.x > 32) {
			pos.x = 1;
			if (++pos.y >= ylen) {
				break;
			}
		}
	}
	[self refresh];
	return self;
}

-(Rect *) getRect
{
	return &rect;
}

-setOwner: owner
{
	self.owner = owner;
	return self;
}

-redraw
{
	return [owner redraw];
}
@end
