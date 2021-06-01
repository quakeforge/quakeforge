#include <string.h>
#include "ruamoko/qwaq/debugger/views/stringview.h"

@implementation StringView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def])) {
		return nil;
	}
	self.data = (int *)(data + def.offset);
	return self;
}

+(StringView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-draw
{
	[super draw];
	string val = sprintf ("\"%s\"",
						  str_quote (qdb_get_string (target, data[0])));
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

@end
