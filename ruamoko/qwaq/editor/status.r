#include "ruamoko/qwaq/editor/status.h"

static int cursor_modes[] = { 'I', 'O', 'i', 'o' };

@implementation EditStatus

-initWithRect:(Rect)rect
{
	if (!(self = [super initWithRect: rect])) {
		return nil;
	}
	growMode = gfGrowNone;
	return self;
}

+(EditStatus *)withRect:(Rect)rect
{
	return [[self alloc] initWithRect:rect];
}

-setCursorMode:(CursorMode)mode
{
	cursorMode = mode;
	return self;
}

-setModified:(int)modified
{
	self.modified = modified;
	return self;
}

-draw
{
	[super draw];
	if (modified) {
		[self mvaddch:{0, 0}, '*'];
	}
	[self mvaddch:{1, 0}, cursor_modes[cursorMode]];
	return self;
}

@end
