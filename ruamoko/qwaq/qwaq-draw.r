#include <string.h>

#include "qwaq-curses.h"
#include "qwaq-draw.h"

@implementation DrawBuffer

+ (DrawBuffer *) buffer: (Extent) size
{
	return [[self alloc] initWithSize: size];
}

- initWithSize: (Extent) size
{
	if (!(self = [super init])) {
		return nil;
	}
	buffer = obj_malloc (size.width * size.height);
	self.size = size;
	return self;
}

- (Extent) size
{
	return size;
}

- (int *) buffer
{
	return buffer;
}
- (Rect) rect
{
	Rect rect = { nil, size };
	return rect;
}

- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect
{
	Extent srcSize = srcBuffer.size;
	Rect r = { {}, size };
	Rect t = { pos, rect.extent };

	t = clipRect (r, t);
	if (t.extent.width < 0 || t.extent.height < 0) {
		return self;
	}

	rect.offset.x += t.offset.x - pos.x;
	rect.offset.y += t.offset.y - pos.y;
	rect.extent = t.extent;
	pos = t.offset;

	r.offset = nil;
	r.extent = size;

	rect = clipRect (r, rect);
	if (rect.extent.width < 0 || rect.extent.height < 0) {
		return self;
	}

	int        *dst = buffer + pos.y * size.width + pos.x;
	int        *src = srcBuffer.buffer
					  + rect.offset.y * srcSize.width + rect.offset.x;
	for (int y = 0; y < rect.extent.height; y++) {
		int        *d = dst;
		int        *s = src;
		dst += size.width;
		src += srcSize.width;
		for (int x = 0; x < rect.extent.width; x++) {
			// FIXME 1) need memcpy/memmove
			// 2) the generated code could be better
			 *d++ = *s++;
		}
	}
	return self;
}

- (void) printf: (string) fmt, ...
{
	string str = vsprintf (fmt, @args);
	[self addstr: str];
}

- (void) vprintf: (string) fmt, @va_list args
{
	string str = vsprintf (fmt, args);
	[self addstr: str];
}

- (void) addch: (int) ch
{
	if (cursor.x < 0) {
		cursor.x = 0;
	}
	if (cursor.y < 0) {
		cursor.y = 0;
	}
	if (cursor.x >= size.width && cursor.y < size.height) {
		cursor.x = 0;
		cursor.y++;
	}
	if (cursor.y >= size.height) {
		return;
	}
	if (ch == '\n') {
		cursor.x = 0;
		cursor.y++;
	} else if (ch == '\r') {
		cursor.x = 0;
	} else {
		buffer[cursor.y * size.width + cursor.x++] = ch;
	}
}

- (void) addstr: (string) str
{
	int         ind = 0;
	int         ch;

	if (cursor.y >= size.height) {
		return;
	}
	while (cursor.x < size.width && (ch = str_char (str, ind++))) {
		[self addch: ch];
	}
}

- (void) mvprintf: (Point) pos, string fmt, ...
{
	if (pos.x < 0 || pos.x >= size.width
		|| pos.y < 0 || pos.y >= size.height) {
		return;
	}
	cursor = pos;
	string str = vsprintf (fmt, @args);
	[self addstr: str];
}

- (void) mvvprintf: (Point) pos, string fmt, @va_list args
{
	if (pos.x < 0 || pos.x >= size.width
		|| pos.y < 0 || pos.y >= size.height) {
		return;
	}
	cursor = pos;
	string str = vsprintf (fmt, args);
	[self addstr: str];
}

- (void) mvaddch: (Point) pos, int ch
{
	if (pos.x < 0 || pos.x >= size.width
		|| pos.y < 0 || pos.y >= size.height) {
		return;
	}
	cursor = pos;
	[self addch: ch];
}

- (void) mvaddstr: (Point) pos, string str
{
	if (pos.x < 0 || pos.x >= size.width
		|| pos.y < 0 || pos.y >= size.height) {
		return;
	}
	cursor = pos;
	[self addstr: str];
}

@end
