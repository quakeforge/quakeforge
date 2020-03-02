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
	views = [[Array array] retain];
	event_handlers = [[Array array] retain];
	window = stdscr;
	rect = getwrect (window);
	return self;
}

-add: obj
{
	if ([obj conformsToProtocol: @protocol (Draw)]) {
		[views addObject: obj];
	}
	if ([obj conformsToProtocol: @protocol (HandleEvent)]) {
		[event_handlers addObject: obj];
	}
	return self;
}

-setBackground: (int) ch
{
	wbkgd (window, ch);
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	return self;
}

-draw
{
	[views makeObjectsPerformSelector: @selector (draw)];
	update_panels ();
	doupdate ();
	return self;
}

@end
