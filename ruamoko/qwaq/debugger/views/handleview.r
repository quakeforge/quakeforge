#include <string.h>
#include "ruamoko/qwaq/debugger/views/handleview.h"

@implementation HandleView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (int *)(data + def.offset);
	return self;
}

+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(string)format
{
	string val = sprintf ("%d", data[0]);
	return val;
}

@end
