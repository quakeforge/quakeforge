#include <string.h>
#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/defview.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"
#include "ruamoko/qwaq/ui/tableview.h"

static string meta_views[] = {
	"BasicView",
	"StructView",
	"StructView",
	"EnumView",
	"ArrayView",
	"ClassView",
	"AliasView",	// shouldn't happen, but...
};

@implementation DefView

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	return self;
}

-initWithDef:(qdb_def_t)def type:(qfot_type_t *)type
{
	if (!(self = [super init])) {
		return nil;
	}
	self.def = def;
	self.type = type;
	return self;
}

-setTarget:(qdb_target_t)target
{
	self.target = target;
	return self;
}

+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	return [self withDef:def in:data target:nil];
}

+(DefView *)withDef:(qdb_def_t)def
			   type:(qfot_type_t *)type
				 in:(void *)data
			 target:(qdb_target_t)target
{
	string metaname = nil;
	if (type.meta == ty_alias) {
		type = type.alias.aux_type;
	}
	if (type.meta >= 0
		&& type.meta < sizeof(meta_views) / sizeof (meta_views[0])) {
		metaname = meta_views[type.meta];
	}
	id class = obj_lookup_class (metaname);
	if (class) {
		return [[class withDef:def in:data type:type] setTarget:target];
	}
	return [NameView withName:"Invalid Meta"];
}

+(DefView *)withDef:(qdb_def_t)def
				 in:(void *)data
			 target:(qdb_target_t)target
{
	qfot_type_t *type = [TypeEncodings getType:def.type_encoding
									fromTarget:target];
	return [[DefView withDef:def
						type:type
						  in:data
					  target:target] retain];
}

-fetchData
{
	// most def views do not need to update themselves
	return self;
}

-(int) rows
{
	return 1;
}

-(View *) viewAtRow:(int) row forColumn:(TableViewColumn *)column
{
	if ([column name] == "name") {
		return [NameView withName:qdb_get_string (target, def.name)];
	}
	return self;
}

@end
