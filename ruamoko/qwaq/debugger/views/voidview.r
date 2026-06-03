#include <string.h>
#include "ruamoko/qwaq/debugger/views/voidview.h"

@implementation VoidView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (unsigned *) (data + def.offset);
	return self;
}

+(VoidView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string)format:(int)width
{
	string val = sprintf ("%08x %08x %08x %08x",
						  data[0], data[1], data[2], data[3]);
	return sprintf ("%*.*s", width, width, val);
}

@end
