#include <string.h>
#include "debugger/funcview.h"

@implementation FuncView

-initWithType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	if (!(self = [super initWithType:type])) {
		return nil;
	}
	self.data = (unsigned *)(data + offset);
	return self;
}

+(FuncView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	return [[[self alloc] initWithType:type at:offset in:data] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("FIXME [%x]", data[0]);
	[self mvaddstr:{0, 0}, str_mid (val, 0, xlen)];
	return self;
}

@end
