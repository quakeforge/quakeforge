#include <string.h>
#include <types.h>
#include "debugger/defview.h"
#include "debugger/nameview.h"
#include "debugger/localsdata.h"
#include "debugger/typeencodings.h"

@implementation LocalsData

-initWithTarget:(qdb_target_t) target
{
	if (!(self = [super init])) {
		return nil;
	}
	self.target = target;

	qdb_def_t   encodings_def = qdb_find_global (target, ".type_encodings");
	qdb_get_data (target, encodings_def.offset, sizeof(target_encodings),
				  &target_encodings);

	return self;
}

+(LocalsData *)withTarget:(qdb_target_t)target
{
	return [[[self alloc] initWithTarget:target] autorelease];
}

-(void)dealloc
{
	if (defs) {
		obj_free (defs);
		defs = nil;
	}
	if (data) {
		obj_free (data);
		data = nil;
	}
}

-setFunction:(unsigned) fnum
{
	if (current_fnum == fnum) {
		return self;
	}
	current_fnum =fnum;

	if (defs) {
		obj_free (defs);
		defs = nil;
	}
	if (data) {
		obj_free (data);
		data = nil;
	}
	func = qdb_get_function (target, fnum);
	aux_func = qdb_get_auxfunction (target, fnum);
	if (aux_func) {
		defs = qdb_get_local_defs (target, fnum);
	}
	if (func) {
		data = obj_malloc (func.local_size);
	}
	return self;
}

-fetchData
{
	qdb_get_data (target, func.local_data, func.local_size, data);
	return self;
}

-(int)numberOfRows:(TableView *)tableview
{
	if (aux_func) {
		return aux_func.num_locals;
	} else if (func) {
		return (func.local_size + 3) / 4;
	}
	return 0;
}

-(View *)tableView:(TableView *)tableview
		 forColumn:(TableViewColumn *)column
			   row:(int)row
{
	View      *view;

	if ([column name] == "name") {
		view = [NameView withName:qdb_get_string (target, defs[row].name)];
	} else {
		qfot_type_t *type = [TypeEncodings getType:defs[row].type_encoding
										fromTarget:target];
		unsigned    offset = defs[row].offset;
		view = [DefView withType:type at:offset in:data];
	}
	[view resizeTo:{[column width], 1}];
	return view;
}

@end
