#include <string.h>
#include "ruamoko/qwaq/debugger/views/vectorview.h"

@implementation VectorView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (vector *)(data + def.offset);
	return self;
}

+(VectorView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string)format:(int)width
{
	string val = sprintf ("%.9v", data[0]);
	return sprintf ("%*.*s", width, width, val);
}

@end
