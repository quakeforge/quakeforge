#include <string.h>
#include "ruamoko/qwaq/debugger/views/indexview.h"

@implementation IndexView

-initWithIndex:(int)index
{
	if (!(self = [super initWithDef:def type:nil])) {
		return nil;
	}
	self.index = index;
	return self;
}

+(IndexView *)withIndex:(int)index
{
	return [[[self alloc] initWithIndex:index] autorelease];
}

-draw
{
	[super draw];
	string      val = sprintf ("[%d]", index);
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

-(View *) viewAtRow:(int) row forColumn:(TableViewColumn *)column
{
	return self;
}

@end
