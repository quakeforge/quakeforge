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
	if (!(self = [super initWithRect:getwrect (stdscr)])) {
		return nil;
	}
	textContext = stdscr;
	scrollok (textContext, 1);
	return self;
}

-setBackground: (int) ch
{
	wbkgd (textContext, ch);
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	if (event.what & qe_mouse) {
		[self printf:"%04x %2d %2d %d %08x        \r", event.what, event.mouse.x, event.mouse.y, event.mouse.click, event.mouse.buttons];
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
	wrefresh(textContext);
	doupdate ();
	return self;
}

-printf: (string) fmt, ...
{
	wvprintf (textContext, fmt, @args);
	return self;
}

-addch: (int) ch atX: (int) x Y: (int) y
{
	mvwaddch(textContext, x, y, ch);
	return self;
}

@end
