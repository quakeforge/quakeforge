#include <string.h>
#include "ruamoko/qwaq/device/nameview.h"

@implementation NameView

-initWithName:(string)name
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = name;
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

-(string)format:(int)width
{
	return str_mid (name, 0, width);
}

@end
