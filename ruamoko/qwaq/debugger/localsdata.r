#include <stdlib.h>
#include <string.h>
#include <types.h>
#include "ruamoko/qwaq/debugger/views/defview.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/localsdata.h"
#include "ruamoko/qwaq/ui/listener.h"

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

	self.has_stack = qdb_has_data_stack (target);

	self.onRowCountChanged = [[ListenerGroup listener] retain];
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
	for (int i = 0; i < self.num_user_defs; i++) {
		[self.def_views[i] release];
	}
	obj_free (self.def_views);
	self.def_views = nil;
	obj_free (self.def_rows);
	self.def_rows = nil;
}

-(void)dealloc
{
	[onRowCountChanged release];
	if (defs) {
		free_defs (self);
	}
	if (data) {
		obj_free (data);
		data = nil;
	}
	[super dealloc];
}

-setFunction:(unsigned) fnum
{
	if (current_fnum == fnum) {
		return self;
	}
	current_fnum = fnum;

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
	num_user_defs = 0;
	if (aux_func) {
		defs = qdb_get_local_defs (target, fnum);
		for (unsigned i = 0; i < aux_func.num_locals; i++) {
			string def_name = qdb_get_string (target, defs[i].name);
			if (str_mid (def_name, 0, 1) != ".") {
				num_user_defs++;
			}
		}
		def_views = obj_malloc (num_user_defs);
		def_rows = obj_malloc (num_user_defs + 1);
		def_rows[0] = 0;
		for (unsigned i = 0, j = 0; i < aux_func.num_locals; i++) {
			string def_name = qdb_get_string (target, defs[i].name);
			if (str_mid (def_name, 0, 1) == ".") {
				continue;
			}
			def_views[j] = [[DefView withDef:defs[i] in:data target:target]
							retain];
			def_rows[j + 1] = [def_views[j] rows];
			j++;
		}
		prefixsum (def_rows, num_user_defs + 1);
	}
	[onRowCountChanged respond:self];
	return self;
}

-fetchData
{
	if (data && func.local_size) {
		unsigned    local_data = func.local_data;
		if (has_stack) {
			unsigned    stack_ptr = qdb_get_frame_addr (target);
			local_data = 0;
			if (stack_ptr) {
				local_data = stack_ptr - func.local_data;
			}
		}
		if (local_data) {
			qdb_get_data (target, local_data, func.local_size, data);
		}
	}
	int         rowCount = def_rows[num_user_defs];
	if (aux_func) {
		def_rows[0] = 0;
		for (int i = 0; i < num_user_defs; i++) {
			[def_views[i] fetchData];
			def_rows[i + 1] = [def_views[i] rows];
		}
		prefixsum (def_rows, num_user_defs + 1);
	}
	if (rowCount != def_rows[num_user_defs]) {
		[onRowCountChanged respond:self];
	}
	return self;
}

-(ListenerGroup *)onRowCountChanged
{
	return onRowCountChanged;
}

-(int)numberOfRows:(TableView *)tableview
{
	if (aux_func) {
		if (!num_user_defs) {
			return 0;
		}
		return def_rows[num_user_defs];
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
	int       *index = fbsearch (&row, def_rows, num_user_defs, 1, nil);

	if (index) {
		DefView    *dv = def_views[index - def_rows];
		int         r = row - *index;
		view = [dv viewAtRow: r forColumn:column];
	}
	return view;
}

@end
