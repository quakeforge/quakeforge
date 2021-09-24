#include <string.h>

#include "ruamoko/qwaq/ui/group.h"
#include "ruamoko/qwaq/ui/stringview.h"

@implementation StringView

-initWithRect:(Rect)rect string:(string)str
{
	if (!(self = [super initWithRect:rect])) {
		return nil;
	}
	self.str = str;
	growMode = gfGrowHiX;
	return self;
}

+(StringView *)withRect:(Rect)rect
{
	return [[[self alloc] initWithRect:rect string:nil] autorelease];
}

+(StringView *)withRect:(Rect)rect string:(string)str
{
	return [[[self alloc] initWithRect:rect string:str] autorelease];
}

-setString:(string) newTitle
{
	str = newTitle;
	[self redraw];
	return self;
}

-draw
{
	[super draw];
	[self mvaddstr: { 0, 0}, str];
	return self;
}

@end
