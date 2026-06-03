#include <string.h>
#include "ruamoko/qwaq/debugger/views/nameview.h"

@implementation NameView

-initWithName:(string)name
{
	if (!(self = [super initWithDef:def type:nil target:nil])) {
		return nil;
	}
	self.name = str_hold (name);
	return self;
}

-(void)dealloc
{
	str_free (name);
	[super dealloc];
}

+(NameView *)withName:(string)name
{
	return [[[self alloc] initWithName:name] autorelease];
}

-(string)format
{
	return name;
}

-(DefView *) cellAtRow:(int) row forColumn:(TableColumn *)column level:(int)level
{
	return self;
}

@end
