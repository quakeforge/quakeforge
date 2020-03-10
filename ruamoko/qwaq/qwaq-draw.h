#ifndef __qwaq_draw_h
#define __qwaq_draw_h

#include <Object.h>

#include "qwaq-rect.h"

@interface DrawBuffer : Object
{
	int        *buffer;
	Extent      size;
	Point       cursor;
}
+ (DrawBuffer *) buffer: (Extent) size;
- initWithSize: (Extent) size;

- (Extent) size;
- (int *) buffer;

- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect;

- (void) printf: (string) fmt, ...;
- (void) vprintf: (string) fmt, @va_list args;
- (void) addch: (int) ch;
- (void) addstr: (string) str;
- (void) mvprintf: (Point) pos, string fmt, ...;
- (void) mvvprintf: (Point) pos, string fmt, @va_list args;
- (void) mvaddch: (Point) pos, int ch;
- (void) mvaddstr: (Point) pos, string str;
@end

#endif
