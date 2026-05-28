#include <Array.h>
#include "ruamoko/qwaq/ui/listener.h"
#include "ruamoko/qwaq/ui/scrollbar.h"
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

-setGrowMode: (int) mode
{
	growMode = mode;
	return self;
}

-(int)growMode
{
	return growMode;
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

-grow:(Extent)delta
{
	if (growMode & gfGrowHiX) {
		width += delta.width;
	}
	return self;
}
@end
