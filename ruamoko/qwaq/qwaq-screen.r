#include <Array.h>
#include "qwaq-curses.h"
#include "qwaq-screen.h"

@implementation Screen
+(Screen *) screen
{
	return [[[self alloc] init] autorelease];
}

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	window = stdscr;
	scrollok (window, 1);
	rect = getwrect (window);
	return self;
}

-setBackground: (int) ch
{
	wbkgd (window, ch);
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	if (event.what & qe_mouse) {
		[self printf:"%04x %2d %2d %d %08x\r", event.what, event.mouse.x, event.mouse.y, event.mouse.click, event.mouse.buttons];
		[self redraw];
	}
	return self;
}

-draw
{
	update_panels ();
	doupdate ();
	return self;
}

-redraw
{
	update_panels ();
	wrefresh(window);
	doupdate ();
	return self;
}

-printf: (string) fmt, ...
{
	wvprintf (window, fmt, @args);
	return self;
}

-addch: (int) ch atX: (int) x Y: (int) y
{
	mvwaddch(window, x, y, ch);
	return self;
}

- (Rect *) getRect
{
	return &rect;
}

-setOwner: owner
{
	return self;
}

@end
