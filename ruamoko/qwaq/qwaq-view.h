#ifndef __qwaq_view_h
#define __qwaq_view_h

#include <Array.h>
#include <Object.h>

#include "qwaq-draw.h"
#include "qwaq-rect.h"
#include "qwaq-textcontext.h"

@class Group;

enum {
	ofCanFocus      =0x0001,
	ofFirstClick    =0x0002,
	ofDontDraw      =0x0004,
	ofPreProcess    =0x0008,
	ofPostProcess   =0x0010,
	ofMakeFirst     =0x0020,
	ofTileable      =0x0040,
	ofCentered      =0x0080,

	ofCallHasObject =0x8000,
};

enum {
	sfDrawn         =0x0001,
	sfDisabled      =0x0002,
	sfInFocus       =0x0004,
	sfModal         =0x0008,
	sfLocked        =0x0010,
};

@interface View: Object
{
	union {
		Rect        rect;
		struct {
			int    xpos;
			int    ypos;
			int    xlen;
			int    ylen;
		};
	};
	Rect        absRect;
	Point       point;		// can't be local :(
	Group      *owner;
	id<TextContext> textContext;
	int         state;
	int         options;
	int         cursorState;
	Point       cursor;
}
-initWithRect: (Rect) rect;
- (void) dealloc;
-setOwner: (Group *) owner;
-(struct Rect_s *)getRect;
-draw;
-redraw;
-handleEvent: (qwaq_event_t *) event;

- (void) refresh;
- (void) mvprintf: (Point) pos, string fmt, ...;
- (void) mvvprintf: (Point) pos, string fmt, @va_list args;
- (void) mvaddch: (Point) pos, int ch;
@end

@interface View (TextContext) <TextContext>
@end

#endif//__qwaq_view_h
