#include "qwaq-curses.h"
#include "qwaq-view.h"
#include "qwaq-group.h"

@implementation View

-init
{
	return [super init];
}

-initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	self.rect = rect;
	self.absRect = rect;
	return self;
}

- (void) dealloc
{
	if (owner) {
		[owner remove:self];
	}
	[super dealloc];
}

- setContext: (id<TextContext>) context
{
	textContext = context;
	return self;
}

- (int) options
{
	return options;
}

static void
updateScreenCursor (View *view)
{
	while ((view.state & sfInFocus) && view.owner) {
		View       *owner = (View *) view.owner;
		if (view.cursor.x >= 0 && view.cursor.x < view.xlen
			&& view.cursor.y >= 0 && view.cursor.y < view.ylen) {
			owner.cursor.x = view.cursor.x + view.xpos;
			owner.cursor.y = view.cursor.y + view.ypos;
			owner.cursorState = view.cursorState;
		} else {
			owner.cursorState = 0;
		}
		view = owner;
	}
	if (view.state & sfInFocus) {
		if (view.cursor.x >= 0 && view.cursor.x < view.xlen
			&& view.cursor.y >= 0 && view.cursor.y < view.ylen) {
			curs_set (view.cursorState);
			move(view.cursor.x, view.cursor.y);
		} else {
			curs_set (0);
		}
	}
}

-draw
{
	state |= sfDrawn;
	updateScreenCursor (self);
	return self;
}

-hide
{
	if (state & sfDrawn) {
		state &= ~sfDrawn;
		updateScreenCursor (self);
	}
	return self;
}

-redraw
{
	if ((state & sfDrawn) && !(options & ofDontDraw)) {
		[self draw];
		[owner redraw];
	}
	return self;
}

-setOwner: (Group *) owner
{
	self.owner = owner;
	return self;
}

- (Rect) rect
{
	return rect;
}

-(Point)origin
{
	return pos;
}

-(Extent)size
{
	return size;
}


-(int) containsPoint: (Point) point
{
	return rectContainsPoint (rect, point);
}

-(void) grabMouse
{
	[owner grabMouse];
}

-(void) releaseMouse
{
	[owner releaseMouse];
}

- (void) forward: (SEL) sel : (@va_list) args
{
	if (!textContext) {
		return;
	}
	if (!__obj_responds_to (textContext, sel)) {
		[self error: "no implementation for %s", sel_get_name (sel)];
	}
	obj_msg_sendv (textContext, sel, args);
}

- (void) refresh
{
	if (__obj_responds_to (textContext, @selector(refresh))) {
		[(id)textContext refresh];
	}
}

- (void) mvprintf: (Point) pos, string fmt, ...
{
	pos.x += xpos;
	pos.y += ypos;
	[textContext mvvprintf: pos, fmt, @args];
}

- (void) mvvprintf: (Point) pos, string fmt, @va_list args
{
	pos.x += xpos;
	pos.y += ypos;
	[textContext mvvprintf: pos, fmt, args];
}

- (void) mvaddch: (Point) pos, int ch
{
	pos.x += xpos;
	pos.y += ypos;
	[textContext mvaddch: pos, ch];
}

-handleEvent: (qwaq_event_t *) event
{
	return self;
}

- (void) onMouseEnter: (Point) pos
{
}

- (void) onMouseLeave: (Point) pos
{
}


@end
