#ifndef __qwaq_ui_view_h
#define __qwaq_ui_view_h

#include <Array.h>
#include <Object.h>

#include "ui/draw.h"
#include "ui/rect.h"
#include "ui/textcontext.h"

@class Group;
@class ListenerGroup;

enum {
	ofCanFocus      = 0x0001,
	ofFirstClick    = 0x0002,
	ofMakeFirst     = 0x0004,
	ofDontDraw      = 0x0008,
	ofRelativeEvents= 0x0010,

	ofTileable      = 0x0020,
	ofCentered      = 0x0040,
};

enum {
	sfDrawn         = 0x0001,
	sfDisabled      = 0x0002,
	sfInFocus       = 0x0004,
	sfModal         = 0x0008,
	sfLocked        = 0x0010,
};

enum {
	gfGrowNone      = 0x0000,
	gfGrowLoX       = 0x0001,
	gfGrowLoY       = 0x0002,
	gfGrowHiX       = 0x0004,
	gfGrowHiY       = 0x0008,
	gfGrowRel       = 0x0010,
	gfGrowLo        = gfGrowLoX | gfGrowLoY,
	gfGrowHi        = gfGrowHiX | gfGrowHiY,
	gfGrowX         = gfGrowLoX | gfGrowHiX,
	gfGrowY         = gfGrowLoY | gfGrowHiY,
	gfGrowAll       = gfGrowX | gfGrowY,
};

@protocol View
-setOwner: (Group *) owner;
-setGrowMode: (int) mode;

-(Rect)rect;
-(Point)origin;
-(Extent)size;

-(int) containsPoint: (Point) point;
-(void) grabMouse;
-(void) releaseMouse;

-(int) options;
-(int) state;

-(id<TextContext>)context;
-setContext: (id<TextContext>) context;
-draw;
-hide;
-redraw;
-move: (Point) delta;
-resize: (Extent) delta;
-move:(Point)dpos andResize:(Extent)dsize;
-grow: (Extent) delta;
-(ListenerGroup *)onEvent;
-handleEvent: (qwaq_event_t *) event;
-takeFocus;
-loseFocus;
-(ListenerGroup *) onReceiveFocus;
-(ListenerGroup *) onReleaseFocus;
-raise;

- (void) onMouseEnter: (Point) pos;
- (void) onMouseLeave: (Point) pos;

- (void) refresh;
- (void) mvprintf: (Point) pos, string fmt, ...;
- (void) mvvprintf: (Point) pos, string fmt, @va_list args;
- (void) mvaddch: (Point) pos, int ch;
-clear;
@end

@interface View: Object <View>
{
	union {
		Rect        rect;
		struct {
			int     xpos;
			int     ypos;
			int     xlen;
			int     ylen;
		};
		struct {
			Point   pos;
			Extent  size;
		};
	};
	Rect        absRect;
	Group      *owner;
	id<TextContext> textContext;
	int         state;
	int         options;
	int         growMode;
	int         cursorState;
	Point       cursor;
	ListenerGroup *onReceiveFocus;
	ListenerGroup *onReleaseFocus;
	ListenerGroup *onEvent;
}
+(View *)viewWithRect:(Rect)rect;
+(View *)viewWithRect:(Rect)rect options:(int)options;

-initWithRect:(Rect)rect;
-initWithRect:(Rect)rect options:(int)options;
@end

@interface View (TextContext) <TextContext>
@end

#endif//__qwaq_ui_view_h
