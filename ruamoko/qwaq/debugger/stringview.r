#include <string.h>
#include "debugger/stringview.h"

@implementation StringView

-initWithType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	if (!(self = [super initWithType:type])) {
		return nil;
	}
	self.data = (int *)(data + offset);
	return self;
}

+(StringView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	return [[[self alloc] initWithType:type at:offset in:data] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("FIXME %s", qdb_get_string (target, data[0]));// quote string
	[self mvaddstr:{0, 0}, str_mid (val, 0, xlen)];
	return self;
}

@end
