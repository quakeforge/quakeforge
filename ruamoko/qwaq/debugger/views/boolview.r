#include <string.h>
#include "ruamoko/qwaq/debugger/views/boolview.h"

@implementation BoolView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = data + def.offset;
	return self;
}

+(BoolView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string) get_val:(int)ind
{
	bool b = false;
	if (type.type == ev_long) {
		b = ((long*)data)[ind] != 0;
	} else {
		b = ((int*)data)[ind] != 0;
	}
	return b ? "true" : "false";
}

-(string)format
{
	string val = sprintf ("%s", [self get_val:0]);
	for (int i = 1; i < type.basic.width; i++) {
		val = sprintf ("%s %s", val, [self get_val:i]);
	}
	return val;
}

@end
