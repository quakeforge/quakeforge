#include <string.h>
#include "ruamoko/qwaq/debugger/views/indexview.h"

@implementation IndexView

-initWithIndex:(int)index
{
	if (!(self = [super initWithDef:def type:nil target:nil])) {
		return nil;
	}
	self.index = index;
	return self;
}

+(IndexView *)withIndex:(int)index
{
	return [[[self alloc] initWithIndex:index] autorelease];
}

-(string)format
{
	string      val = sprintf ("[%d]", index);
	return val;
}

-(DefView *) cellAtRow:(int) row forColumn:(TableColumn *)column level:(int)level
{
	return self;
}

@end
