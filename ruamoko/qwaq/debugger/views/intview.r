#include <string.h>
#include "ruamoko/qwaq/debugger/views/intview.h"

@implementation IntView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (int *)(data + def.offset);
	return self;
}

+(IntView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string)format
{
	string val = sprintf ("%d", data[0]);
	for (int i = 1; i < type.basic.width; i++) {
		val = sprintf ("%s %d", val, data[i]);
	}
	return val;
}

@end
