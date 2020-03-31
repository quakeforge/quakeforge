#include <string.h>
#include "debugger/uintview.h"

@implementation UIntView

-initWithType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	if (!(self = [super initWithType:type])) {
		return nil;
	}
	self.data = (unsigned *)(data + offset);
	return self;
}

+(UIntView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	return [[[self alloc] initWithType:type at:offset in:data] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("%u", data[0]);
	[self mvaddstr:{0, 0}, str_mid (val, 0, xlen)];
	return self;
}

@end
