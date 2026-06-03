#include <string.h>
#include <stdlib.h>

typedef int pr_int_t;
typedef uint pr_uint_t;
typedef string pr_string_t;
typedef void * pr_ptr_t;
typedef void (*pr_func_t)();
#include <QF/progs/pr_obj.h>

#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/debugger/views/classview.h"
#include "ruamoko/qwaq/ui/tableview.h"

@implementation ClassView

static bvec2//x = in heirarcy y = possibly legit (no oob access (FIXME loops))
is_valid_class (classptr_t type_class, classptr_t class, qdb_target_t target)
{
	while (class) {
		if (class == type_class) {
			return {true, true};
		}
		uint super;
		if (qdb_get_data (target, (uint) class + 1, 1, &super) < 0) {
			return {false, false};
		}
		class = (classptr_t) super;
	}
	return {false, true};
}

static int
count_ivars (classptr_t class, qdb_target_t target)
{
	int count = 0;
	while (class) {
		uint ivar_list;
		if (qdb_get_data (target, (uint) class + 6, 1, &ivar_list) < 0) {
			return -1;
		}

		if (ivar_list) {
			int c;
			if (qdb_get_data (target, ivar_list, 1, &c) < 0) {
				return -1;
			}
			count += c;
		}

		uint super;
		// if we could read the ivars pointer, we can count the super_class
		// pointer;
		qdb_get_data (target, (uint) class + 1, 1, &super);
		class = (classptr_t) super;
	}
	return count;
}

static bool
collect_ivars (ClassView *self, classptr_t class)
{
	pr_class_t cls;
	if (qdb_get_data (self.target, (uint) class, sizeof (cls) / sizeof (int),
					  &cls) < 0) {
		return false;
	}
	if (cls.super_class) {
		if (!collect_ivars (self, (classptr_t) (uint) cls.super_class)) {
			return false;
		}
	}

	uint ivar_list = (uint) cls.ivars;
	int count;
	if (qdb_get_data (self.target, ivar_list, 1, &count) < 0) {
		return false;
	}

	for (int ind = 0; ind < count; ind++) {
		pr_ivar_t ivar;
		if (qdb_get_data (self.target, ivar_list + 1 + 3 * ind, 3, &ivar) < 0) {
			return false;
		}

		qfot_type_t *ivar_type = [TypeEncodings getType:(uint)ivar.ivar_type
											 fromTarget:self.target];
		qdb_def_t   def = {
			.type_size = 0,	// XXX type/size not needed at this stage
			.offset = ivar.ivar_offset,
			.name = @bitcast (uint, ivar.ivar_name),
			.type_encoding = (uint)ivar.ivar_type,
		};

		int i = self.ivar_count++;
		self.ivar_views[i] = [[DefView withDef:def
										  type:ivar_type
											in:self.instance_data
										target:self.target] retain];
		self.ivar_rows[i + 1] = 0;
		string ivar_name = qdb_get_string (self.target, ivar.ivar_name);
		if (str_mid (ivar_name, 0, 11) != ".anonymous.") {
			self.ivar_rows[i + 1] = [self.ivar_views[i] rows];
		}
	}
	return true;
}

-build_ivars
{
	valid = is_valid_class (type_class, instance_class, target);
	instance_name = 0;
	if (!valid) {
		return self;
	}
	pr_class_t class;
	qdb_get_data (target, (uint) instance_class, sizeof(class)/sizeof(int),
				  &class);
	instance_name = @bitcast (int, class.name);
	if (class.instance_size > (int) instance_size) {
		instance_size = class.instance_size;

		obj_free (instance_data);
		instance_data = obj_malloc (instance_size);
	}
	int count = count_ivars (instance_class, target);
	if (count < 1) {
		valid = {false, false};
		return self;
	}

	// FIXME find a way to reuse views?
	for (int i = 0; i < ivar_count; i++) {
		[ivar_views[i] release];
	}
	if (count > max_ivars) {
		max_ivars = count;
		obj_free (ivar_views);
		obj_free (ivar_rows);
		ivar_views = obj_malloc (max_ivars * sizeof (DefView *));
		ivar_rows = obj_malloc ((max_ivars + 1) * sizeof (int));
	}
	ivar_count = 0;
	ivar_rows[0] = 0;
	collect_ivars (self, instance_class);
	prefixsum (ivar_rows, ivar_count + 1);

	return self;
}

-initWithDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	if (!(self = [super initWithDef:def type:type target:target])) {
		return nil;
	}
	self.data = (classptr_t *) (data + def.offset);

	auto class_def = qdb_find_global (target, "_OBJ_CLASS_" + type.class);
	pr_class_t class = {};
	type_class = (classptr_t) class_def.offset;
	return self;
}

+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target
{
	return [[[self alloc] initWithDef:def in:data type:type target:target] autorelease];
}

-(void)dealloc
{
	for (int i = 0; i < type.strct.num_fields; i++) {
		[ivar_views[i] release];
	}
	obj_free (instance_data);
	obj_free (ivar_views);
	obj_free (ivar_rows);
	[super dealloc];
}

-(string)format:(int)width
{
	string      val = sprintf ("%s", type.strct.tag);
	return sprintf ("%*.*s", width, width, val);
}

-fetchData:(uint)ptr
{
	[super fetchData];
	if (instance_class != *data) {
		instance_class = *data;
		[self build_ivars];
	}
	if (@horiz(| valid)) {
		qdb_get_data (target, ptr, instance_size / sizeof (int), instance_data);
	}
	return self;
}

-(int) rows
{
	return 1 + ivar_rows[ivar_count];
}

-(DefView *) cellAtRow:(int) row forColumn:(TableColumn *)column level:(int)level
{
	if (row == 0) {
		if ([column name] == "name") {
			string name = qdb_get_string (target, instance_name);
			return [NameView withName: sprintf ("%*s%s", level, "", name)];
		}
		return [NameView withName: "{}"];
	}

	row -= 1;

	DefView   *cell = nil;
	int *index = fbsearch (&row, ivar_rows, ivar_count, 1, nil);

	if (index) {
		DefView    *dv = ivar_views[index - ivar_rows];
		int         r = row - *index;
		cell = [dv cellAtRow: r forColumn:column level:level + 1];
	}
	return cell;
}

@end
