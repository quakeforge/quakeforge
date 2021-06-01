#include <Array.h>
#include "ruamoko/qwaq/ui/event.h"
#include "ruamoko/qwaq/ui/draw.h"
#include "ruamoko/qwaq/ui/garray.h"
#include "ruamoko/qwaq/ui/group.h"
#include "ruamoko/qwaq/ui/view.h"

@implementation Group

+(Group *)withContext:(id<TextContext>)context owner:(View *)owner
{
	return [[[self alloc] initWithContext:context owner:owner] autorelease];
}

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
	[super dealloc];
}

-(id<TextContext>)context
{
	return context;
}

-setContext: (id<TextContext>) context
{
	self.context = context;
	[views makeObjectsPerformSelector:@selector(setContext:)
						   withObject:context];
	return self;
}

-insert: (View *) view
{
	[views addObject: view];
	[view setOwner: self];
	[view setContext: context];
	return self;
}

-insertDrawn: (View *) view
{
	[self insert: view];
	[view draw];
	return self;
}

-insertSelected: (View *) view
{
	[self insertDrawn: view];
	[self selectView: view];
	return self;
}

-remove: (View *) view
{
	int         index = [views indexOfObject: view];
	if (index != NotFound) {
		if (focused == index) {
			[self selectPrev];
			if (focused == index) {
				focused = -1;
			}
		} else if (focused > index) {
			focused--;
		}
		[views removeObjectAtIndex: index];
		if (mouse_within == view) {
			mouse_within = nil;
		}
		if (mouse_grabbed == view) {
			[self releaseMouse];
		}
	}
	return self;
}

static int
makeFirst (Group *self, int viewIndex)
{
	View       *view = [self.views objectAtIndex: viewIndex];

	// add before remove to avoid freeing view
	[self.views addObject: view];
	[self.views removeObjectAtIndex: viewIndex];
	[view raise];
	return [self.views count] - 1;
}

static int
trySetFocus (Group *self, int viewIndex)
{
	View       *view = [self.views objectAtIndex:viewIndex];
	if (([view state] & (sfDrawn | sfDisabled)) == sfDrawn
		&& [view options] & ofCanFocus) {
		if (!self.owner || ([self.owner state] & sfInFocus)) {
			if (self.focused >= 0) {
				[[self.views objectAtIndex: self.focused] loseFocus];
			}
			self.focused = viewIndex;
			if ([view options] & ofMakeFirst) {
				self.focused = makeFirst (self, viewIndex);
			}
			[view takeFocus];
		} else {
			self.focused = viewIndex;
		}
		return 1;
	}
	return 0;
}

-takeFocus
{
	if (focused >= 0) {
		[[views objectAtIndex:focused] takeFocus];
	}
	return self;
}

-loseFocus
{
	if (focused >= 0) {
		[[views objectAtIndex:focused] loseFocus];
	}
	return self;
}

-selectNext
{
	for (int i = focused + 1; i < [views count]; i++) {
		if (trySetFocus (self, i)) {
			return self;
		}
	}
	// hit end, start again at the beginning
	for (int i = 0; i < focused; i++) {
		if (trySetFocus (self, i)) {
			return self;
		}
	}
	// did not find another view that can be focused
	return self;
}

-selectPrev
{
	for (int i = focused - 1; i >= 0; i--) {
		if (trySetFocus (self, i)) {
			return self;
		}
	}
	// hit end, start again at the beginning
	for (int i = [views count] - 1; i > focused; i++) {
		if (trySetFocus (self, i)) {
			return self;
		}
	}
	// did not find another view that can be focused
	return self;
}

-selectView:(View *)view
{
	int         index = [views indexOfObject: view];
	if (index != NotFound) {
		trySetFocus (self, index);
	}
	return self;
}

-(Rect) rect
{
	if (owner) {
		return [owner rect];
	}
	return {[self origin], [self size]};
}

-(Rect) absRect
{
	if (owner) {
		return [owner absRect];
	}
	return {[self origin], [self size]};
}

-(Point) origin
{
	if (owner) {
		return [owner origin];
	}
	return {0, 0};
}

-(Extent) size
{
	if (owner) {
		return [owner size];
	}
	return [context size];
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
	if (owner) {
		[owner redraw];
	} else {
		[self draw];
		if (__obj_responds_to (context, @selector(refresh))) {
			[(id)context refresh];
		}
	}
	return self;
}

-updateAbsPos: (Point) absPos
{
	for (int i = [views count]; i-- > 0; ) {
		[[views objectAtIndex: i] updateAbsPos: absPos];
	}
	return self;
}

-resize: (Extent) delta
{
	for (int i = [views count]; i-- > 0; ) {
		[[views objectAtIndex: i] grow: delta];
	}
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

static void
handlePositionalEvent (qwaq_event_t *event, View *view)
{
	Point       pos = [view origin];
	int         options = [view options];

	if (options & ofRelativeEvents) {
		event.mouse.x -= pos.x;
		event.mouse.y -= pos.y;
	}

	[view handleEvent: event];

	if (options & ofRelativeEvents) {
		event.mouse.x += pos.x;
		event.mouse.y += pos.y;
	}
}

-handleEvent: (qwaq_event_t *) event
{
	if (event.what & qe_focused) {
		if (focused >= 0) {
			[[views objectAtIndex:focused] handleEvent: event];
		}
	} else if (event.what & qe_positional) {
		if (mouse_grabbed) {
			handlePositionalEvent (event, mouse_grabbed);
		} else {
			Point       pos = {event.mouse.x, event.mouse.y};
			View       *mouse_view = find_mouse_view (self, pos);
			if (mouse_within != mouse_view) {
				[mouse_within onMouseLeave: pos];
				[mouse_view onMouseEnter: pos];
				mouse_within = mouse_view;
			}
			if (mouse_within) {
				handlePositionalEvent (event, mouse_within);
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
	mouse_grabbed = nil;
	[owner releaseMouse];
}

@end
