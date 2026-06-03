#include <string.h>
#include "ruamoko/qwaq/debugger/views/stringview.h"

@implementation StringView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (int *)(data + def.offset);
	return self;
}

+(StringView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string)format:(int)width
{
	string val = sprintf ("\"%s\"",
						  str_quote (qdb_get_string (target, data[0])));
	return sprintf ("%*.*s", width, width, val);
}

@end
