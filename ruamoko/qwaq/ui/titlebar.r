#include <string.h>

#include "ruamoko/qwaq/ui/group.h"
#include "ruamoko/qwaq/ui/titlebar.h"

@implementation TitleBar

+(TitleBar *)withTitle:(string)title
{
	return [[[self alloc] initWithTitle:title] autorelease];
}

-initWithTitle:(string) title
{
	if (!(self = [super init])) {
		return nil;
	}
	self.title = title;
	length = strlen (title);
	growMode = gfGrowHiX;
	return self;
}

-setTitle:(string) newTitle
{
	title = newTitle;
	length = strlen (title);
	[self redraw];
	return self;
}

-setOwner: (Group *) owner
{
	[super setOwner: owner];
	size = [owner size];
	return self;
}

-draw
{
	[super draw];
	[self mvaddstr: { (xlen - length) / 2, 0}, title];
	return self;
}

@end
