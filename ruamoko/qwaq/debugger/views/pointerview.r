#include <string.h>
#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/views/pointerview.h"
#include "ruamoko/qwaq/ui/tableview.h"

@implementation PointerView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def type:type])) {
		return nil;
	}
	self.data = (unsigned *)(data + def.offset);
	return self;
}

+(PointerView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-(void)dealloc
{
	if (ptr_data) {
		obj_free (ptr_data);
	}
	[ptr_view release];
	[super dealloc];
}

-draw
{
	[super draw];
	string val = sprintf ("%s [0x%x]", type.encoding, data[0]);
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

-fetchData
{
	if (!ptr_type) {
		qdb_def_t   def = { 0, 0, 0, (unsigned)type.fldptr.aux_type };
		ptr_type = type.fldptr.aux_type;
		ptr_size = [TypeEncodings typeSize:ptr_type];
		ptr_data = obj_malloc (ptr_size);
		ptr_view = [[DefView withDef:def
								type:ptr_type
								  in:ptr_data
							  target:target] retain];
	}
	invalid = 1;
	if (!ptr_view || ptr != (unsigned) data[0]) {
		invalid = qdb_get_data (target, data[0], ptr_size, ptr_data) < 0;
	}
	return self;
}

-(int) rows
{
	if (invalid) {
		return 2;
	}
	return 1 + [ptr_view rows];
}

-(View *) viewAtRow:(int) row forColumn:(TableViewColumn *)column
{
	if (row == 0) {
		if ([column name] == "name") {
			return [NameView withName:qdb_get_string (target, def.name)];
		}
		return self;
	}
	if (invalid) {
		if ([column name] == "name") {
			return nil;
		}
		return [NameView withName:"Invalid pointer"];
	}
	return [ptr_view viewAtRow:row - 1 forColumn:column];
}

@end
