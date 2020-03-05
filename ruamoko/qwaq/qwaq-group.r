#include <Array.h>
#include "event.h"
#include "qwaq-group.h"

@implementation Group
-init
{
	if (!(self = [super init])) {
		return nil;
	}
	views = [[Array array] retain];
	return self;
}

-(void)dealloc
{
	[views release];
}

-insert: (View *) view
{
	[views insertObject: view atIndex: 0];
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

-draw
{
	[views makeObjectsPerformSelector: @selector(draw)];
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	[super handleEvent: event];
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
