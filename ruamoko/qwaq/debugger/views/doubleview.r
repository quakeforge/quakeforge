#include <string.h>
#include "ruamoko/qwaq/debugger/views/doubleview.h"

@implementation DoubleView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (double *)(data + def.offset);
	return self;
}

+(DoubleView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string)format:(int)width
{
	string val = sprintf ("%.17g", data[0]);
	for (int i = 1; i < type.basic.width; i++) {
		val = sprintf ("%s %.17g", val, data[i]);
	}
	return sprintf ("%*.*s", width, width, val);
}

@end
