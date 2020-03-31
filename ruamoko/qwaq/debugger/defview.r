#include <string.h>
#include "debugger/defview.h"
#include "debugger/nameview.h"

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

-initWithType:(qfot_type_t *)type
{
	if (!(self = [super init])) {
		return nil;
	}
	self.type = type;
	return self;
}

+(DefView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	string metaname = nil;
	//printf("%d %d %d %d\n", type.meta, type.size, type.encoding, type.type);
	if (type.meta == ty_alias) {
		type = type.alias.aux_type;
	}
	if (type.meta >= 0
		&& type.meta < sizeof(meta_views) / sizeof (meta_views[0])) {
		metaname = meta_views[type.meta];
	}
	id class = obj_lookup_class (metaname);
	if (class) {
		return [class withType:type at:offset in:data];
	}
	return [NameView withName:"Invalid Meta"];
}

@end
