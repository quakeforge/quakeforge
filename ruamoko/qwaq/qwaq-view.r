#include "qwaq-curses.h"
#include "qwaq-view.h"
#include "qwaq-group.h"

Rect
makeRect (int xpos, int ypos, int xlen, int ylen)
{
	Rect rect = {{xpos, ypos}, {xlen, ylen}};
	return rect;
}

int
rectContainsPoint (Rect *rect, Point *point)
{
	return ((point.x >= rect.offset.x
			 && point.x < rect.offset.x + rect.extent.width)
			&& (point.y >= rect.offset.y
				&& point.y < rect.offset.y + rect.extent.height));
}

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

- (Rect *) getRect
{
	return &rect;
}

- (void) printf: (string) fmt, ...
{
	[textContext vprintf: fmt, @args];
}

- (void) vprintf: (string) fmt, @va_list args
{
	[textContext vprintf: fmt, args];
}

- (void) refresh
{
	[textContext refresh];
}
/*
- (void) addch: (int) ch
{
	[textContext addch:ch];
}*/

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

@end

Rect getwrect (window_t window) = #0;
