#include <Array.h>
#include "event.h"
#include "qwaq-draw.h"
#include "qwaq-garray.h"
#include "qwaq-group.h"
#include "qwaq-view.h"

@implementation Group

-initWithContext: (id<TextContext>) context
{
	if (!(self = [super init])) {
		return nil;
	}
	self.context = context;
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

-handleEvent: (qwaq_event_t *) event
{
	if (event.what & qe_focused) {
		if (focused >= 0) {
			[[views objectAtIndex:focused] handleEvent: event];
		}
	} else if (event.what & qe_positional) {
	} else {
		// broadcast
		[views makeObjectsPerformSelector: @selector(draw) withObject: event];
	}
	return self;
}
@end
