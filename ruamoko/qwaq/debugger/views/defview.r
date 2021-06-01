#include <string.h>
#include "ruamoko/qwaq/debugger/typeencodings.h"
#include "ruamoko/qwaq/debugger/views/defview.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"

static string meta_views[] = {
	"BasicView",
	"StructView",
	"UnionView",
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

-initWithDef:(qdb_def_t)def
{
	if (!(self = [super init])) {
		return nil;
	}
	self.def = def;
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
				 in:(void *)data
			 target:(qdb_target_t)target
{
	qfot_type_t *type = [TypeEncodings getType:def.type_encoding
									fromTarget:target];
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

-(int) rows
{
	return 1;
}

-(View *) nameViewAtRow:(int) row
{
	return [NameView withName:qdb_get_string (target, def.name)];
}

-(View *) dataViewAtRow:(int) row
{
	return self;
}

@end
