#include <string.h>
#include "ruamoko/qwaq/debugger/views/doubleview.h"

@implementation DoubleView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def type:type])) {
		return nil;
	}
	self.data = (double *)(data + def.offset);
	return self;
}

+(DoubleView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("%.17g", data[0]);
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

@end
