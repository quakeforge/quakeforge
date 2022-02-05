#include "ruamoko/qwaq/ui/curses.h"
#include "ruamoko/qwaq/ui/listener.h"
#include "ruamoko/qwaq/ui/group.h"
#include "ruamoko/qwaq/ui/scrollbar.h"
#include "ruamoko/qwaq/ui/view.h"
#include "ruamoko/qwaq/debugger/debug.h"

@implementation View

+(View *)viewWithRect:(Rect)rect
{
	return [[[self alloc] initWithRect:rect] autorelease];
}

+(View *)viewWithRect:(Rect)rect options:(int)options
{
	return [[[self alloc] initWithRect:rect options:options] autorelease];
}

static void view_init(View *self)
{
	self.onReceiveFocus = [[ListenerGroup listener] retain];
	self.onReleaseFocus = [[ListenerGroup listener] retain];
	self.onEvent = [[ListenerGroup listener] retain];
	self.onViewScrolled = [[ListenerGroup listener] retain];
}

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	view_init (self);
	return self;
}

-initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	view_init (self);

	self.rect = rect;
	self.absRect = rect;
	return self;
}

-initWithRect: (Rect) rect options:(int)options
{
	if (!(self = [super init])) {
		return nil;
	}
	view_init (self);

	self.rect = rect;
	self.absRect = rect;
	self.options = options;
	return self;
}

- (void) dealloc
{
	[onReceiveFocus release];
	[onReleaseFocus release];
	[onEvent release];
	[onViewScrolled release];
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

-(void)onScrollBarModified:(id)sender
{
	if (sender == vScrollBar) {
		scroll.y = [sender index];
	} else if (sender == hScrollBar) {
		scroll.x = [sender index];
	}
	[onViewScrolled respond:self];
}

static void
setScrollBar (View *self, ScrollBar **sb, ScrollBar *scrollbar)
{
	SEL         sel = @selector(onScrollBarModified:);
	[scrollbar retain];
	[[*sb onScrollBarModified] removeListener:self :sel];
	[*sb release];

	*sb = scrollbar;
	[[*sb onScrollBarModified] addListener:self :sel];
}

-setVerticalScrollBar:(ScrollBar *)scrollbar
{
	setScrollBar (self, &vScrollBar, scrollbar);
	return self;
}

-setHorizontalScrollBar:(ScrollBar *)scrollbar
{
	setScrollBar (self, &hScrollBar, scrollbar);
	return self;
}

-(window_t) window
{
	return nil;
}

static void updateScreenCursor (View *view);

-updateScreenCursor
{
	updateScreenCursor (self);
	return self;
}

static void
updateScreenCursor (View *view)
{
	if (view.state & sfInFocus) {
		if (view.owner) {
			View       *owner = [view.owner owner];
			if (view.cursorPos.x >= 0 && view.cursorPos.x < view.xlen
				&& view.cursorPos.y >= 0 && view.cursorPos.y < view.ylen) {
				owner.cursorPos.x = view.cursorPos.x + view.xpos;
				owner.cursorPos.y = view.cursorPos.y + view.ypos;
				owner.cursorState = view.cursorState;
			} else {
				owner.cursorState = 0;
			}
			[owner updateScreenCursor];
		} else {
		}
	}
/*
			curs_set (cursorState);
			wmove (get_window (view), cursorPos.x, cursorPos.y);
*/
}

-hideCursor
{
	return [self setCursorVisible: 0];
}

-showCursor
{
	return [self setCursorVisible: 1];
}

-setCursorVisible: (int) visible
{
	cursorState = visible;
	if ((state & (sfInFocus | sfDrawn)) == (sfInFocus | sfDrawn)) {
		[self updateScreenCursor];
	}
	return self;
}

-moveCursor: (Point) pos
{
	cursorPos = pos;
	if ((state & (sfInFocus | sfDrawn)) == (sfInFocus | sfDrawn)) {
		[self updateScreenCursor];
	}
	return self;
}

-draw
{
	state |= sfDrawn;
	[self updateScreenCursor];
	return self;
}

-hide
{
	if (state & sfDrawn) {
		state &= ~sfDrawn;
		[self updateScreenCursor];
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

- (Rect) absRect
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
		@return nil;
	}
	if (!__obj_responds_to (textContext, sel)) {
		[self error: "no implementation for %s", sel_get_name (sel)];
	}
	@return obj_msg_sendv (textContext, sel, args);
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
	[textContext mvvprintf: pos, fmt, va_copy (@args)];
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

-clear
{
	[textContext clearReact:rect];
	return self;
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
		Rect        r = [owner absRect];
		Extent      s = [owner size];
		if (xpos > r.extent.width - 1) {
			xpos = r.extent.width - 1;
		}
		if (ypos > r.extent.height - 1) {
			ypos = r.extent.height - 1;
		}
		[self updateAbsPos: r.offset];
	} else {
		[self updateAbsPos: nil];
	}
	return self;
}

-updateAbsPos: (Point) absPos
{
	absRect.offset.x = absPos.x + xpos;
	absRect.offset.y = absPos.y + ypos;
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

-moveTo:(Point)pos
{
	self.pos = pos;
	return self;
}

-resizeTo:(Extent)size
{
	self.size = size;
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

-(ListenerGroup *) onReceiveFocus
{
	return onReceiveFocus;
}

-(ListenerGroup *) onReleaseFocus
{
	return onReleaseFocus;
}

-(ListenerGroup *)onEvent
{
	return onEvent;
}

-(ListenerGroup *) onViewScrolled
{
	return onViewScrolled;
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
	[self updateScreenCursor];
	[onReceiveFocus respond:self];
	return self;
}

-loseFocus
{
	state &= ~sfInFocus;
	[self updateScreenCursor];
	[onReleaseFocus respond:self];
	return self;
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
