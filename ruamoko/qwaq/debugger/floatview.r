#include <string.h>
#include "debugger/floatview.h"

@implementation FloatView

-initWithType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	if (!(self = [super initWithType:type])) {
		return nil;
	}
	self.data = (float *)(data + offset);
	return self;
}

+(FloatView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	return [[[self alloc] initWithType:type at:offset in:data] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("%.9", data[0]);
	[self mvaddstr:{0, 0}, str_mid (val, 0, xlen)];
	return self;
}

@end
