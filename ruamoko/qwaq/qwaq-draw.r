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

- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect
{
	Extent srcSize = srcBuffer.size;
	Rect r = { {}, srcBuffer.size };
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
}

- (void) vprintf: (string) fmt, @va_list args
{
}

- (void) addch: (int) ch
{
}

- (void) mvprintf: (Point) pos, string fmt, ...
{
}

- (void) mvvprintf: (Point) pos, string fmt, @va_list args
{
}

- (void) mvaddch: (Point) pos, int ch
{
}

@end
