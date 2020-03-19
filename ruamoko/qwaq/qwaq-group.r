#include <Array.h>
#include "event.h"
#include "qwaq-draw.h"
#include "qwaq-garray.h"
#include "qwaq-group.h"
#include "qwaq-view.h"

@implementation Group

-initWithContext: (id<TextContext>) context owner: (View *) owner
{
	if (!(self = [super init])) {
		return nil;
	}
	self.owner = owner;
	self.context = context;
	focused = -1;
	views = [[Array array] retain];
	return self;
}

-(void)dealloc
{
	[views release];
}

-insert: (View *) view
{
	[views addObject: view];
	[view setContext: context];
	return self;
}

-remove: (View *) view
{
	int         index = [views indexOfObject: view];
	if (index != NotFound) {
		if (focused == index) {
			focused = -1;
		} else if (focused > index) {
			focused--;
		}
		[views removeObjectAtIndex: index];
	}
	return self;
}

static BOOL
not_dont_draw (id aView, void *aGroup)
{
	View       *view = aView;
	Group      *group = (Group *) aGroup;

	return !([view options] & ofDontDraw);
}

-draw
{
	[views makeObjectsPerformSelector: @selector(draw)
								   if: not_dont_draw
								 with: self];
	return self;
}

-redraw
{
	return self;
}

static View *
find_mouse_view(Group *group, Point pos)
{
	for (int i = [group.views count]; i--; ) {
		View       *v = [group.views objectAtIndex: i];
		if ([v containsPoint: pos]) {
			return v;
		}
	}
	return nil;
}

-handleEvent: (qwaq_event_t *) event
{
	if (event.what & qe_focused) {
		if (focused >= 0) {
			[[views objectAtIndex:focused] handleEvent: event];
		}
	} else if (event.what & qe_positional) {
		Point       pos = {event.mouse.x, event.mouse.y};
		if (mouse_grabbed) {
			[mouse_grabbed handleEvent: event];
		} else {
			if (mouse_within && ![mouse_within containsPoint: pos]) {
				[mouse_within onMouseLeave: pos];
				mouse_within = nil;
			}
			if (!mouse_within) {
				mouse_within = find_mouse_view (self, pos);
				if (mouse_within) {
					[mouse_within onMouseEnter: pos];
				}
			}
			if (mouse_within) {
				[mouse_within handleEvent: event];
			}
		}
	} else {
		// broadcast
		[views makeObjectsPerformSelector: @selector(draw) withObject: event];
	}
	return self;
}

-(void) grabMouse
{
	mouse_grabbed = mouse_within;
	[owner grabMouse];
}

-(void) releaseMouse
{
	mouse_grabbed = mouse_within;
	[owner grabMouse];
}

@end
