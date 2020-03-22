#include "color.h"
#include "qwaq-editor.h"

@implementation Editor

-initWithRect:(Rect) rect file:(string) filename
{
	if (!(self = [super initWithRect: rect])) {
		return nil;
	}
	buffer = [[EditBuffer alloc] initWithFile: filename];
	line_count = [buffer countLines: {0, [buffer textSize]}];
	linebuffer = [DrawBuffer buffer: { xlen, 1 }];
	return self;
}

-draw
{
	[super draw];
	unsigned lind = base_index;
	int *lbuf = [linebuffer buffer];
	for (int y = 0; y < ylen; y++) {
		lind = [buffer formatLine:lind from:x_index into:lbuf width:xlen
				highlight:selection colors: {COLOR_PAIR (1), COLOR_PAIR(2)}];
		[textContext blitFromBuffer: linebuffer to: {xpos, ypos + y}
							from: [linebuffer rect]];
	}
	return self;
}

-handleEvent:(qwaq_event_t *) event
{
	if (event.what & qe_mouse) {
		if (event.what == qe_mouseclick) {
			if (event.mouse.buttons & (1 << 3)) {
				[self scrollUp: 1];
			}
			if (event.mouse.buttons & (1 << 4)) {
				[self scrollDown: 1];
			}
			if (event.mouse.buttons & (1 << 5)) {
				[self scrollLeft: 1];
			}
			if (event.mouse.buttons & (1 << 6)) {
				[self scrollRight: 1];
			}
		}
	}
	return self;
}

-scrollUp:(unsigned) count
{
	if (count == 1) {
		base_index = [buffer prevLine: base_index];
	} else {
		base_index = [buffer prevLine: base_index :count];
	}
	[self redraw];
	return self;
}

-scrollDown:(unsigned) count
{
	if (count == 1) {
		base_index = [buffer nextLine: base_index];
	} else {
		base_index = [buffer nextLine: base_index :count];
	}
	[self redraw];
	return self;
}

-scrollLeft:(unsigned) count
{
	if (x_index > count) {
		x_index -= count;
	} else {
		x_index = 0;
	}
	[self redraw];
	return self;
}

-scrollRight:(unsigned) count
{
	if (1024 - x_index > count) {
		x_index += count;
	} else {
		x_index = 1024;
	}
	[self redraw];
	return self;
}

@end
