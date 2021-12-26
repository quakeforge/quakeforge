#include <string.h>
#include <stdlib.h>
#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/views/structview.h"
#include "ruamoko/qwaq/ui/tableview.h"

@implementation StructView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	if (!(self = [super initWithDef:def type:type])) {
		return nil;
	}
	self.data = (unsigned *)(data + def.offset);
	field_views = obj_malloc (type.strct.num_fields);
	field_rows = obj_malloc (type.strct.num_fields);
	field_rows[0] = 0;
	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_type_t *field_type = type.strct.fields[i].type;
		qdb_def_t   def = {
			0,	// XXX type/size not needed at this stage
			type.strct.fields[i].offset,
			0,	// filled in by setTarget
			(unsigned)type.fldptr.aux_type
		};
		field_views[i] = [[DefView withDef:def
									  type:field_type
										in:data
									target:target] retain];
		field_rows[i + 1] = 0;
		if (str_mid (type.strct.fields[i].name, 0, 11) != ".anonymous.") {
			field_rows[i + 1] = [field_views[i] rows];
		}
	}
	prefixsum (field_rows, type.strct.num_fields + 1);
	return self;
}

+(StructView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [[[self alloc] initWithDef:def in:data type:type] autorelease];
}

-setTarget:(qdb_target_t)target
{
	[super setTarget:target];
	for (int i = 0; i < type.strct.num_fields; i++) {
		if (target.handle) {
			// the field name is already in local space because the same type
			// may be defined in both progs
			string name = type.strct.fields[i].name;
			field_views[i].def.name = qdb_find_string (target, name);
		}
		[field_views[i] setTarget:target];
	}
	return self;
}

-(void)dealloc
{
	for (int i = 0; i < type.strct.num_fields; i++) {
		[field_views[i] release];
	}
	obj_free (field_views);
	obj_free (field_rows);
	[super dealloc];
}

-draw
{
	[super draw];
	string      val = sprintf ("%s", type.strct.tag);
	[self mvprintf:{0, 0}, "%*.*s", xlen, xlen, val];
	return self;
}

-fetchData
{
	[super fetchData];
	field_rows[0] = 0;
	for (int i = 0; i < type.strct.num_fields; i++) {
		[field_views[i] fetchData];
		field_rows[i + 1] = 0;
		if (str_mid (type.strct.fields[i].name, 0, 11) != ".anonymous.") {
			field_rows[i + 1] = [field_views[i] rows];
		}
	}
	prefixsum (field_rows, type.strct.num_fields + 1);
	return self;
}

-(int) rows
{
	return 1 + field_rows[type.strct.num_fields];
}

-(View *) viewAtRow:(int) row forColumn:(TableViewColumn *)column
{
	if (row == 0) {
		if ([column name] == "name") {
			return [NameView withName:qdb_get_string (target, def.name)];
		}
		return self;
	}

	row -= 1;

	View      *view = nil;
	int       *index = fbsearch (&row, field_rows, type.strct.num_fields, 1, nil);

	if (index) {
		DefView    *dv = field_views[index - field_rows];
		int         r = row - *index;
		view = [dv viewAtRow: r forColumn:column];
	}
	return view;
}

@end
