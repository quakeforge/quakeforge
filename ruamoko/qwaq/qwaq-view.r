#include "qwaq-curses.h"
#include "qwaq-listener.h"
#include "qwaq-view.h"
#include "qwaq-group.h"

@implementation View

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	onReceiveFocus = [[ListenerGroup alloc] init];
	onReleaseFocus = [[ListenerGroup alloc] init];
	onEvent = [[ListenerGroup alloc] init];
	return self;
}

-initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	self.rect = rect;
	self.absRect = rect;
	onReceiveFocus = [[ListenerGroup alloc] init];
	onReleaseFocus = [[ListenerGroup alloc] init];
	onEvent = [[ListenerGroup alloc] init];
	return self;
}

-initWithRect: (Rect) rect options:(int)options
{
	if (!(self = [super init])) {
		return nil;
	}
	self.rect = rect;
	self.absRect = rect;
	self.options = options;
	onReceiveFocus = [[ListenerGroup alloc] init];
	onReleaseFocus = [[ListenerGroup alloc] init];
	onEvent = [[ListenerGroup alloc] init];
	return self;
}

- (void) dealloc
{
	if (owner) {
		[owner remove:self];
	}
	[super dealloc];
}

-(id<TextContext>)context
{
	return textContext;
}

-setContext: (id<TextContext>) context
{
	textContext = context;
	return self;
}

- (int) options
{
	return options;
}

- (int) state
{
	return state;
}

static void
updateScreenCursor (View *view)
{
	// XXX this does not work
/*	while ((view.state & sfInFocus) && view.owner) {
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
	}*/
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

-setGrowMode: (int) mode
{
	growMode = mode;
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

- (void) mvaddstr: (Point) pos, string str
{
	pos.x += xpos;
	pos.y += ypos;
	[textContext mvaddstr: pos, str];
}

-move: (Point) delta
{
	xpos += delta.x;
	ypos += delta.y;
	if (xpos + xlen < 1) {
		xpos = 1 - xlen;
	}
	if (ypos < 0) {
		ypos = 0;
	}
	if (owner) {
		Extent      s = [owner size];
		if (xpos > s.width - 1) {
			xpos = s.width - 1;
		}
		if (ypos > s.height - 1) {
			ypos = s.height - 1;
		}
	}
	return self;
}

-resize: (Extent) delta
{
	xlen += delta.width;
	ylen += delta.height;
	if (xlen < 1) {
		xlen = 1;
	}
	if (ylen < 1) {
		ylen = 1;
	}
	return self;
}

-move:(Point)dpos andResize:(Extent)dsize
{
	[self move: dpos];
	[self resize: dsize];
	return self;
}

-grow: (Extent) delta
{
	Point       dpos = {};
	Extent      dsize = {};

	if (growMode & gfGrowLoX) {
		dpos.x += delta.width;
		dsize.width -= delta.width;
	}
	if (growMode & gfGrowHiX) {
		dsize.width += delta.width;
	}
	if (growMode & gfGrowLoY) {
		dpos.y += delta.height;
		dsize.height -= delta.height;
	}
	if (growMode & gfGrowHiY) {
		dsize.height += delta.height;
	}
	int save_state = state;
	state &= ~sfDrawn;
	[self move: dpos andResize: dsize];
	state = save_state;
	[self redraw];
	return self;
}

-(ListenerGroup *)onEvent
{
	return onEvent;
}

-handleEvent: (qwaq_event_t *) event
{
	// give any listeners a chance to override or extend event handling
	[onEvent respond:self withObject:event];
	if (event.what & (qe_mousedown | qe_mouseclick)
		&& options & ofCanFocus && !(state & (sfDisabled | sfInFocus))) {
		[owner selectView: self];
		if (!(options & ofFirstClick)) {
			event.what = qe_none;
		}
	}
	return self;
}

-takeFocus
{
	state |= sfInFocus;
	[onReceiveFocus respond:self];
	return self;
}

-loseFocus
{
	state &= ~sfInFocus;
	[onReleaseFocus respond:self];
	return self;
}

-(ListenerGroup *) onReceiveFocus
{
	return onReceiveFocus;
}

-(ListenerGroup *) onReleaseFocus
{
	return onReleaseFocus;
}

-raise
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
