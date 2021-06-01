#include <string.h>
#include "ruamoko/qwaq/debugger/views/intview.h"

@implementation IntView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def])) {
		return nil;
	}
	self.data = (int *)(data + def.offset);
	return self;
}

+(IntView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("%d", data[0]);
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

@end
