#include <string.h>
#include <stdlib.h>
#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/indexview.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/views/arrayview.h"
#include "ruamoko/qwaq/ui/tableview.h"

@implementation ArrayView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def type:type])) {
		return nil;
	}
	self.data = (unsigned *)(data + def.offset);
	element_views = obj_malloc (type.array.size);
	element_rows = obj_malloc (type.array.size);
	element_rows[0] = 0;
	qfot_type_t *element_type = type.array.type;
	int         element_size = [TypeEncodings typeSize:element_type];
	for (int i = 0; i < type.array.size; i++) {
		qdb_def_t   def = {
			0,	// XXX type/size not needed at this stage
			i * element_size,
			0,	// filled in by setTarget
			(unsigned)type.fldptr.aux_type
		};
		element_views[i] = [[DefView withDef:def
										type:element_type
										  in:data
									  target:target] retain];
		element_rows[i + 1] = [element_views[i] rows];
	}
	prefixsum (element_rows, type.array.size + 1);
	return self;
}

+(ArrayView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-setTarget:(qdb_target_t)target
{
	[super setTarget:target];
	for (int i = 0; i < type.array.size; i++) {
		[element_views[i] setTarget:target];
	}
	return self;
}

-(void)dealloc
{
	for (int i = 0; i < type.array.size; i++) {
		[element_views[i] release];
	}
	obj_free (element_views);
	obj_free (element_rows);
}

-draw
{
	[super draw];
	string      val = sprintf ("%s[%d..%d]", type.array.type.encoding,
							   type.array.base,
							   type.array.base + type.array.size - 1);
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

-fetchData
{
	[super fetchData];
	element_rows[0] = 0;
	for (int i = 0; i < type.array.size; i++) {
		[element_views[i] fetchData];
		element_rows[i + 1] = [element_views[i] rows];
	}
	prefixsum (element_rows, type.array.size + 1);
	return self;
}

-(int) rows
{
	return 1 + element_rows[type.array.size];
}

-(View *) viewAtRow:(int)row forColumn:(TableViewColumn *)column
{
	if (row == 0) {
		if ([column name] == "name") {
			return [NameView withName:qdb_get_string (target, def.name)];
		}
		return self;
	}

	row -= 1;

	View      *view = nil;
	int       *index = fbsearch (&row, element_rows, type.array.size, 1, nil);

	if ([column name] == "name") {
		return [IndexView withIndex:index - element_rows + type.array.base];
	}

	if (index) {
		DefView    *dv = element_views[index - element_rows];
		int         r = row - *index;
		view = [dv viewAtRow: r forColumn:column];
	}
	return view;
}

@end
