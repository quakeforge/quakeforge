#ifndef __qwaq_draw_h
#define __qwaq_draw_h

#include <Object.h>

#include "qwaq-rect.h"

@class DrawBuffer;

@protocol DrawBuffer
- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect;
- (Rect) rect;
- (Extent) size;
- (int *) buffer;
@end

@protocol TextContext
- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect;
- (Extent) size;

- (void) printf: (string) fmt, ...;
- (void) vprintf: (string) fmt, @va_list args;
- (void) addch: (int) ch;
- (void) addstr: (string) str;
- (void) mvprintf: (Point) pos, string fmt, ...;
- (void) mvvprintf: (Point) pos, string fmt, @va_list args;
- (void) mvaddch: (Point) pos, int ch;
- (void) mvaddstr: (Point) pos, string str;
@end

@interface DrawBuffer : Object <DrawBuffer, TextContext>
{
	int        *buffer;
	Extent      size;
	Point       cursor;
}
+ (DrawBuffer *) buffer: (Extent) size;
- initWithSize: (Extent) size;
@end

#endif
