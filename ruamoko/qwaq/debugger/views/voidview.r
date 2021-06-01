#include <string.h>
#include "ruamoko/qwaq/debugger/views/voidview.h"

@implementation VoidView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def])) {
		return nil;
	}
	self.data = (unsigned *) (data + def.offset);
	return self;
}

+(VoidView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("%08x %08x %08x %08x",
						  data[0], data[1], data[2], data[3]);
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

@end
