#include <stdlib.h>
#include <string.h>
#include <types.h>
#include "ruamoko/qwaq/debugger/views/defview.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/localsdata.h"

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

static void
free_defs (LocalsData *self)
{
	obj_free (self.defs);
	self.defs = nil;
	for (int i = 0; i < self.aux_func.num_locals; i++) {
		[self.def_views[i] release];
	}
	obj_free (self.def_views);
	self.def_views = nil;
	obj_free (self.def_rows);
	self.def_rows = nil;
}

-(void)dealloc
{
	if (defs) {
		free_defs (self);
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
		free_defs (self);
	}
	if (data) {
		obj_free (data);
		data = nil;
	}
	func = qdb_get_function (target, fnum);
	if (func && func.local_size) {
		data = obj_malloc (func.local_size);
	}
	aux_func = qdb_get_auxfunction (target, fnum);
	if (aux_func) {
		defs = qdb_get_local_defs (target, fnum);
		def_views = obj_malloc (aux_func.num_locals);
		def_rows = obj_malloc (aux_func.num_locals + 1);
		def_rows[0] = 0;
		for (int i = 0; i < aux_func.num_locals; i++) {
			def_views[i] = [[DefView withDef:defs[i] in:data target:target]
							retain];
			def_rows[i + 1] = [def_views[i] rows];
		}
		prefixsum (def_rows, aux_func.num_locals + 1);
	}
	return self;
}

-fetchData
{
	if (data && func.local_size && func.local_data) {
		qdb_get_data (target, func.local_data, func.local_size, data);
	}
	return self;
}

-(int)numberOfRows:(TableView *)tableview
{
	if (aux_func) {
		if (!aux_func.num_locals) {
			return 0;
		}
		return def_rows[aux_func.num_locals];
	} else if (func) {
		return (func.local_size + 3) / 4;
	}
	return 0;
}

-(View *)tableView:(TableView *)tableview
		 forColumn:(TableViewColumn *)column
			   row:(int)row
{
	View      *view = nil;
	int       *index = bsearch (&row, def_rows, aux_func.num_locals, 1, nil);

	if (index) {
		if ([column name] == "name") {
			view = [def_views[*index] nameViewAtRow: row - *index];
		} else {
			view = [def_views[*index] dataViewAtRow: row - *index];
		}
	}
	[view resizeTo:{[column width], 1}];
	return view;
}

@end
