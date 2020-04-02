#include <string.h>
#include "debugger/views/basicview.h"
#include "debugger/views/nameview.h"

static string type_views[] = {
	"VoidView",
	"StringView",
	"FloatView",
	"VectorView",
	"EntityView",
	"FieldView",
	"FuncView",
	"PointerView",
	"QuatView",
	"IntView",
	"UIntView",	// uinteger
	"IntView",	// short
	"DoubleView",
};

@implementation BasicView

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	return self;
}

+(DefView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data
{
	string typename = nil;
	if (type.type == ty_alias) {
		type = type.alias.aux_type;
	}
	if (type.type >= 0
		&& type.type < sizeof(type_views) / sizeof (type_views[0])) {
		typename = type_views[type.type];
	}
	id class = obj_lookup_class (typename);
	if (class) {
		return [class withType:type at:offset in:data];
	}
	return [NameView withName:"Invalid Type"];
}

@end
