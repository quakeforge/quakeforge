#include <string.h>
#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/views/pointerview.h"
#include "ruamoko/qwaq/ui/tableview.h"

@implementation PointerView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (unsigned *)(data + def.offset);
	return self;
}

+(PointerView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(void)dealloc
{
	if (ptr_data) {
		obj_free (ptr_data);
	}
	[ptr_view release];
	[super dealloc];
}

-(string)format:(int)width
{
	string val = sprintf ("%s [0x%x]", type.encoding, data[0]);
	return sprintf ("%*.*s", width, width, val);
}

-fetchData
{
	if (!ptr_type) {
		qdb_def_t   def = { 0, 0, 0, (unsigned)type.fldptr.aux_type };
		ptr_type = type.fldptr.aux_type;
		ptr_size = [TypeEncodings typeSize:ptr_type];
		ptr_data = obj_malloc (ptr_size * sizeof (int));
		ptr_view = [[DefView withDef:def
								type:ptr_type
								  in:ptr_data
							  target:target] retain];
	}
	invalid = true;
	if (!ptr_view || ptr != data[0]) {
		invalid = qdb_get_data (target, data[0], ptr_size, ptr_data) < 0;
	}
	ptr = data[0];
	if (!invalid && ptr_view) {
		[ptr_view fetchData:ptr];
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

-(DefView *) cellAtRow:(int) row forColumn:(TableColumn *)column level:(int)level
{
	if (row == 0) {
		if ([column name] == "name") {
			string name = qdb_get_string (target, def.name);
			return [NameView withName: sprintf ("%*s%s", level, "", name)];
		}
		return self;
	}
	if (invalid) {
		if ([column name] == "name") {
			return nil;
		}
		return [NameView withName:"Invalid pointer"];
	}
	return [ptr_view cellAtRow:row - 1 forColumn:column level:level + 1];
}

@end
