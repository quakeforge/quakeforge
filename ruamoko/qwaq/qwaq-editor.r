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
	unsigned lind = base_index;
	int *lbuf = [linebuffer buffer];
	for (int y = 0; y < ylen; y++) {
		lind = [buffer formatLine:lind from:0 into:lbuf width:xlen
				highlight:selection colors: {COLOR_PAIR (1), COLOR_PAIR(2)}];
		[textContext blitFromBuffer: linebuffer to: {xpos, ypos + y}
							from: [linebuffer rect]];
	}
	return self;
}

@end
