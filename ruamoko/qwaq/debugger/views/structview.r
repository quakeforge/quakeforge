#include <string.h>
#include <stdlib.h>
#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/views/structview.h"
#include "ruamoko/qwaq/ui/tableview.h"

@implementation StructView

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (unsigned *)(data + def.offset);
	field_views = obj_malloc (type.strct.num_fields * sizeof (int));
	field_rows = obj_malloc ((type.strct.num_fields + 1) * sizeof (int));
	field_rows[0] = 0;
	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_type_t *field_type = type.strct.fields[i].type;
		qdb_def_t   def = {
			.type_size = 0,	// XXX type/size not needed at this stage
			.offset = type.strct.fields[i].offset,
			// the field name is already in local space because the same type
			// may be defined in both progs
			.name = qdb_find_string (target, type.strct.fields[i].name),
			.type_encoding = (unsigned)type.fldptr.aux_type
		};
		field_views[i] = [[DefView withDef:def
									  type:field_type
										in:self.data
									target:target] retain];
		field_rows[i + 1] = 0;
		if (str_mid (type.strct.fields[i].name, 0, 11) != ".anonymous.") {
			field_rows[i + 1] = [field_views[i] rows];
		}
	}
	prefixsum (field_rows, type.strct.num_fields + 1);
	return self;
}

+(StructView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
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

-(string)format
{
	string      val = sprintf ("%s", type.strct.tag);
	return val;
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

-(DefView *) cellAtRow:(int) row forColumn:(TableColumn *)column level:(int)level
{
	if (row == 0) {
		if ([column name] == "name") {
			string name = qdb_get_string (target, def.name);
			return [NameView withName: sprintf ("%*s%s", level, "", name)];
		}
		return [NameView withName: "{}"];
	}

	row -= 1;

	DefView   *cell = nil;
	int *index = fbsearch (&row, field_rows, type.strct.num_fields, 1, nil);

	if (index) {
		DefView    *dv = field_views[index - field_rows];
		int         r = row - *index;
		cell = [dv cellAtRow: r forColumn:column level:level + 1];
	}
	return cell;
}

@end
