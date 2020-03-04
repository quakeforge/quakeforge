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
	views = [[Array array] retain];
	event_handlers = [[Array array] retain];
	focused_handlers = [[Array array] retain];
	mouse_handlers = [[Array array] retain];
	mouse_handler_rects = [[Array array] retain];
	window = stdscr;
	scrollok (window, 1);
	rect = getwrect (window);
	return self;
}

-add: obj
{
	if ([obj conformsToProtocol: @protocol (Draw)]) {
		// "top" objects are drawn last
		[views addObject: obj];
		[obj setParent: self];
	}
	if ([obj conformsToProtocol: @protocol (HandleFocusedEvent)]) {
		// want "top" objects to respond first
		[focused_handlers insertObject: obj atIndex: 0];
	}
	if ([obj conformsToProtocol: @protocol (HandleMouseEvent)]) {
		// "top" objects respond first, but the array is searched in reverse
		[mouse_handlers addObject: obj];
		[mouse_handler_rects addObject: (id) [obj getRect]];
	}
	if ([obj conformsToProtocol: @protocol (HandleEvent)]) {
		// want "top" objects to respond first
		[event_handlers insertObject: obj atIndex: 0];
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
	switch (event.event_type) {
		case qe_none:
			break;
		case qe_key:
		case qe_command:
			[focused_handlers
				makeObjectsPerformSelector: @selector(handleEvent:)
				withObject: (id) event];
			break;
		case qe_mouse:
			Point p = { event.mouse.x, event.mouse.y };
			for (int i = [mouse_handler_rects count]; i-->0; ) {
				//if (rectContainsPoint((Rect*)mouse_handler_rects._objs[i], &p)) {
				//	[mouse_handlers._objs[i] handleEvent: event];
				//	break;
				//}
			}
			break;
	}
	return self;
}

-draw
{
	[views makeObjectsPerformSelector: @selector (draw)];
	update_panels ();
	doupdate ();
	return self;
}

-redraw
{
	[self error:"hmm"];
	update_panels ();
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

-setParent: parent
{
	return self;
}

@end
