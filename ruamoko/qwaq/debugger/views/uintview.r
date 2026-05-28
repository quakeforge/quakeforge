#include <string.h>
#include "ruamoko/qwaq/debugger/views/uintview.h"

@implementation UIntView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def type:type])) {
		return nil;
	}
	self.data = (unsigned *)(data + def.offset);
	return self;
}

+(UIntView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-(string)format:(int)width
{
	string val = sprintf ("%u", data[0]);
	for (int i = 1; i < type.basic.width; i++) {
		val = sprintf ("%s %u", val, data[i]);
	}
	return sprintf ("%*.*s", width, width, val);
}

@end
