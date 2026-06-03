#include <string.h>
#include "ruamoko/qwaq/debugger/views/floatview.h"

@implementation FloatView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (float *)(data + def.offset);
	return self;
}

+(FloatView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string)format
{
	string val = sprintf ("%.9g", data[0]);
	for (int i = 1; i < type.basic.width; i++) {
		val = sprintf ("%s %.9g", val, data[i]);
	}
	return val;
}

@end
