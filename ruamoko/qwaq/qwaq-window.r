#include <Array.h>

#include "event.h"
#include "qwaq-curses.h"
#include "qwaq-group.h"
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
	textContext = [[TextContext alloc] initWithRect: rect];
	panel = create_panel ([(id)textContext window]);

	objects = [[Group alloc] initWithContext: textContext owner: self];

	buf = [DrawBuffer buffer: {3, 3}];
	[buf mvaddstr: {0, 0}, "XOX"];
	[buf mvaddstr: {0, 1}, "OXO"];
	[buf mvaddstr: {0, 2}, "XOX"];
	return self;
}

-setContext: (id<TextContext>) context
{
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	[objects handleEvent: event];
	return self;
}

-addView: (View *) view
{
	[objects insert: view];
	return self;
}

-setBackground: (int) ch
{
	[(id)textContext bkgd: ch];
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
	[(id)textContext border: box_sides, box_corners];
	[objects draw];
	return self;
}

-redraw
{
	return [owner redraw];
}

- (void) mvprintf: (Point) pos, string fmt, ...
{
	[textContext mvvprintf: pos, fmt, @args];
}

- (void) mvvprintf: (Point) pos, string fmt, @va_list args
{
	[textContext mvvprintf: pos, fmt, args];
}

- (void) mvaddch: (Point) pos, int ch
{
	[textContext mvaddch: pos, ch];
}
@end
