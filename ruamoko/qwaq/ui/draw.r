#include <string.h>

#include "ui/curses.h"
#include "ui/draw.h"

@implementation DrawBuffer

+(DrawBuffer *)buffer:(Extent)size
{
	return [[[self alloc] initWithSize: size] autorelease];
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

-(void)dealloc
{
	obj_free (buffer);
	[super dealloc];
}

- (Extent) size
{
	return size;
}

- (void) resizeTo: (Extent) newSize
{
	size = newSize;
	buffer = obj_realloc (buffer, size.width * size.height);
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

	wprintf (stdscr, "src: %p\n", srcBuffer);
	wprintf (stdscr, "srcSize: %d %d\n", srcSize.width, srcSize.height);

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
			// github issue #3
			 *d++ = *s++;
		}
	}
	return self;
}

- clearReact: (Rect) rect
{
	Point       pos = rect.offset;
	int         len = rect.extent.width;
	int         count = rect.extent.height;

	if (pos.x + len > size.width) {
		len = size.width - pos.x;
	}
	if (pos.x < 0) {
		len += pos.x;
		pos.x = 0;
	}
	if (len < 1) {
		return self;
	}
	if (pos.y + count > size.height) {
		count = size.height - pos.y;
	}
	if (pos.y < 0) {
		count += pos.y;
		pos.y = 0;
	}
	if (count < 1) {
		return self;
	}
	int         width = size.width;
	int         ch = background;
	if (!(ch & 0xff)) {
		ch |= ' ';
	}
	while (count-- > 0) {
		int        *p = buffer + pos.y * width + pos.x;
		for (int i = 0; i < len; i++) {
			*p++ = ch;
		}
		pos.y++;
	}
	return self;
}

- (void) bkgd: (int) ch
{
	background = ch;
}

- (void) clear
{
	int         ch = background;
	int        *dst = buffer;
	int        *end = buffer + size.width * size.height;

	if (ch && !(ch & 0xff)) {
		ch |= ' ';
	}
	while (dst < end) {
		*dst++ = ch;
	}
	cursor = {0, 0};
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
		if (!(ch & ~0xff)) {
			ch |= background & ~0xff;
		}
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
