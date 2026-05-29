#include <Array.h>
#include "ruamoko/qwaq/ui/listener.h"
#include "ruamoko/qwaq/ui/table.h"

@implementation TableColumn
-initWithName:(string)name width:(int)width
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = name;
	self.width = width;
	return self;
}

+(TableColumn *)named:(string)name
{
	return [[[self alloc] initWithName:name width:-1] autorelease];
}

+(TableColumn *)named:(string)name width:(int)width
{
	return [[[self alloc] initWithName:name width:width] autorelease];
}

-setGrowMode: (bool) mode
{
	growWidth = mode;
	return self;
}

-(bool)growMode
{
	return growWidth;
}

-(string)name
{
	return name;
}

-(int)width
{
	return width;
}

-setWidth:(int)width
{
	self.width = width;
	return self;
}

-grow:(ivec2)delta
{
	if (growWidth) {
		width += delta.x;
	}
	return self;
}
@end
