#include "draw.h"

#include "gui/Slider.h"
#include "gui/Rect.h"

@implementation Slider

- (id) initWithBounds: (Rect)aRect size: (integer) aSize
{
	self = [super initWithBounds:aRect];
	dir = ylen > xlen;
	size = aSize;
	index = 0;
	return self;
}

- (void) setIndex: (integer) ind
{
	index = ind;
	if (index < 0)
		index = 0;
	if (index > size)
		index = size;
}

- (void) draw
{
	local integer pos, x, y;

	if (dir) {
		pos = index * ylen / size;
		Draw_Character (xabs, yabs, 1);
		for (y = 8; y < ylen - 8; y += 8)
			Draw_Character (xabs, yabs + y, 2);
		Draw_Character (xabs, yabs + y, 3);
		Draw_Character (xabs, yabs + pos, 131);
	} else {
		pos = index * xlen / size;
		Draw_Character (xabs, yabs, 128);
		for (x = 8; x < xlen - 8; x += 8)
			Draw_Character (xabs + x, yabs, 129);
		Draw_Character (xabs + x, yabs, 130);
		Draw_Character (xabs + pos, yabs, 131);
	}
}

@end
