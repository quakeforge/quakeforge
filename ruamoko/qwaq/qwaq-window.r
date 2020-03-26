#include <Array.h>
#include <string.h>

#include "event.h"
#include "qwaq-button.h"
#include "qwaq-curses.h"
#include "qwaq-group.h"
#include "qwaq-listener.h"
#include "qwaq-titlebar.h"
#include "qwaq-window.h"
#include "qwaq-view.h"

@implementation Window

+windowWithRect: (Rect) rect
{
	return [[[self alloc] initWithRect: rect] autorelease];
}

-initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	self.rect = rect;
	textContext = [[TextContext alloc] initWithRect: rect];
	panel = create_panel ([(id)textContext window]);

	objects = [[Group alloc] initWithContext: textContext owner: self];

	[self insert: titleBar = [[TitleBar alloc] initWithTitle:""]];

	topDrag         = [[Button alloc] initWithRect: {{2, 0},
													 {xlen - 4, 1}}];
	topLeftDrag     = [[Button alloc] initWithRect: {{0, 0},
													 {2, 2}}];
	topRightDrag    = [[Button alloc] initWithRect: {{xlen - 2, 0},
													 {2, 2}}];
	leftDrag        = [[Button alloc] initWithRect: {{0, 2},
													 {1, ylen - 4}}];
	rightDrag       = [[Button alloc] initWithRect: {{xlen - 1, 2},
													 {1, ylen - 4}}];
	bottomLeftDrag  = [[Button alloc] initWithRect: {{0, ylen - 2},
													 {2, 2}}];
	bottomRightDrag = [[Button alloc] initWithRect: {{xlen - 2, ylen - 2},
													 {2, 2}}];
	bottomDrag      = [[Button alloc] initWithRect: {{2, ylen - 1},
													 {xlen - 4, 1}}];
	[self insert: [topDrag         setGrowMode: gfGrowHiX]];
	[self insert: [topLeftDrag     setGrowMode: gfGrowNone]];
	[self insert: [topRightDrag    setGrowMode: gfGrowX]];
	[self insert: [leftDrag        setGrowMode: gfGrowHiY]];
	[self insert: [rightDrag       setGrowMode: gfGrowX | gfGrowHiY]];
	[self insert: [bottomLeftDrag  setGrowMode: gfGrowY]];
	[self insert: [bottomRightDrag setGrowMode: gfGrowAll]];
	[self insert: [bottomDrag      setGrowMode: gfGrowHiX | gfGrowY]];

	[[topDrag         onDrag] addListener: self : @selector(dragWindow:)];
	[[topLeftDrag     onDrag] addListener: self : @selector(dragWindow:)];
	[[topRightDrag    onDrag] addListener: self : @selector(dragWindow:)];
	[[leftDrag        onDrag] addListener: self : @selector(dragWindow:)];
	[[rightDrag       onDrag] addListener: self : @selector(dragWindow:)];
	[[bottomLeftDrag  onDrag] addListener: self : @selector(dragWindow:)];
	[[bottomRightDrag onDrag] addListener: self : @selector(dragWindow:)];
	[[bottomDrag      onDrag] addListener: self : @selector(dragWindow:)];

	buf = [DrawBuffer buffer: {3, 3}];
	[buf mvaddstr: {0, 0}, "XOX"];
	[buf mvaddstr: {0, 1}, "OXO"];
	[buf mvaddstr: {0, 2}, "XOX"];

	growMode = gfGrowHi;
	options = ofCanFocus | ofMakeFirst;
	return self;
}

-setTitle:(string) title
{
	[titleBar setTitle:title];
	return self;
}

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef bound
# define bound(a,b,c) (max(a, min(b, c)))
#endif

- (void) dragWindow: (Button *) sender
{
	Point       delta = [sender delta];
	Point       dp = nil;
	Point       ds = nil;
	Extent      b = [owner size];

	if (sender == topDrag) {
		dp.x = bound (0, xpos + delta.x, b.width  - xlen) - xpos;
		dp.y = bound (0, ypos + delta.y, b.height - ylen) - ypos;
	} else if (sender == topLeftDrag) {
		dp.x = bound (0, xpos + delta.x, xpos     + xlen - delta.x - 1) - xpos;
		dp.y = bound (0, ypos + delta.y, ypos     + ylen - delta.y - 1) - ypos;
		ds.x = bound (1, xlen - delta.x, b.width  - xpos - delta.x) - xlen;
		ds.y = bound (1, ylen - delta.y, b.height - ypos - delta.y) - ylen;
	} else if (sender == topRightDrag) {
		dp.y = bound (0, ypos + delta.y, ypos     + ylen - delta.y - 1) - ypos;
		ds.x = bound (1, xlen + delta.x, b.width  - xpos) - xlen;
		ds.y = bound (1, ylen - delta.y, b.height - ypos - delta.y) - ylen;
	} else if (sender == leftDrag) {
		dp.x = bound (0, xpos + delta.x, xpos     + xlen - delta.x - 1) - xpos;
		ds.x = bound (1, xlen - delta.x, b.width  - xpos - delta.x) - xlen;
	} else if (sender == rightDrag) {
		ds.x = bound (1, xlen + delta.x, b.width  - xpos) - xlen;
	} else if (sender == bottomLeftDrag) {
		dp.x = bound (0, xpos + delta.x, xpos     + xlen - delta.x - 1) - xpos;
		ds.x = bound (1, xlen - delta.x, b.width  - xpos - delta.x) - xlen;
		ds.y = bound (1, ylen + delta.y, b.height - ypos) - ylen;
	} else if (sender == bottomRightDrag) {
		ds.x = bound (1, xlen + delta.x, b.width  - xpos) - xlen;
		ds.y = bound (1, ylen + delta.y, b.height - ypos) - ylen;
	} else if (sender == bottomDrag) {
		ds.y = bound (1, ylen + delta.y, b.height - ypos) - ylen;
	}
	int save_state = state;
	state &= ~sfDrawn;
	[self move:dp andResize:{ds.x, ds.y}];
	state = save_state;
	[self redraw];
}

-setContext: (id<TextContext>) context
{
	return self;
}

-move:(Point)dpos andResize:(Extent)dsize
{
	int save_state = state;
	state &= ~sfDrawn;

	Point pos = self.pos;
	Extent size = self.size;
	[super resize: dsize];
	[super move: dpos];
	// need to move the panel both before and after the resize to avoid
	// HoM effects or window/panel possition errors
	move_panel (panel, xpos, ypos);
	[(id)textContext resizeTo: self.size];
	replace_panel (panel, [(id)textContext window]);
	move_panel (panel, xpos, ypos);

	dsize = {self.size.width - size.width, self.size.height - size.height};
	[objects resize:dsize];

	state = save_state;
	[self redraw];
	return self;
}

-move: (Point) delta
{
	int save_state = state;
	state &= ~sfDrawn;
	[super move: delta];
	move_panel (panel, xpos, ypos);
	state = save_state;
	[self redraw];
	return self;
}

-resize: (Extent) delta
{
	Extent size = self.size;
	[super resize:delta];
	delta = {self.size.width - size.width, self.size.height - size.height};
	[(id)textContext resizeTo: self.size];
	replace_panel (panel, [(id)textContext window]);
	[objects resize:delta];
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	[super handleEvent: event];

	int         offset = event.what & qe_positional;
	if (offset) {
		event.mouse.x -= xpos;
		event.mouse.y -= ypos;
	}
	[objects handleEvent: event];
	if (offset) {
		event.mouse.x += xpos;
		event.mouse.y += ypos;
	}
	return self;
}

-takeFocus
{
	[super takeFocus];
	[objects takeFocus];
	return self;
}

-loseFocus
{
	[super loseFocus];
	[objects loseFocus];
	return self;
}

-raise
{
	top_panel (panel);
	[self redraw];
	return self;
}

-insert: (View *) view
{
	[objects insert: view];
	return self;
}

-insertDrawn: (View *) view
{
	[objects insertDrawn: view];
	return self;
}

-insertSelected: (View *) view
{
	[objects insertSelected: view];
	return self;
}

-setBackground: (int) ch
{
	[(id)textContext bkgd: ch];
	return self;
}

-draw
{
	static box_sides_t box_sides = {
		ACS_VLINE, ACS_VLINE,
		ACS_HLINE, ACS_HLINE,
	};
	static box_corners_t box_corners = {
		ACS_ULCORNER, ACS_URCORNER,
		ACS_LLCORNER, ACS_LRCORNER,
	};
	if (box_sides.ls == ACS_VLINE) {
		int        *foo = &box_sides.ls;
		for (int i = 0; i < 8; i++) {
			foo[i] = acs_char (foo[i]);
		}
	}
	[super draw];
	[(id)textContext border: box_sides, box_corners];
	[objects draw];
	return self;
}

-redraw
{
	if (state & sfDrawn) {
		[owner redraw];
	}
	return self;
}

- (void) mvprintf: (Point) pos, string fmt, ...
{
	[textContext mvvprintf: pos, fmt, @args];
}

- (void) mvvprintf: (Point) pos, string fmt, @va_list args
{
	[textContext mvvprintf: pos, fmt, args];
}

- (void) mvaddch: (Point) pos, int ch
{
	[textContext mvaddch: pos, ch];
}
@end
