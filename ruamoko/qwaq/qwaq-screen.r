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
	textContext = [TextContext screen];
	[textContext scrollok: 1];
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
	return self;
}

-redraw
{
	//update_panels ();
	[TextContext refresh];
	//[TextContext doupdate];
	return self;
}

@end
