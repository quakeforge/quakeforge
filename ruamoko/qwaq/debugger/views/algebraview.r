#include <string.h>
#include "ruamoko/qwaq/debugger/views/algebraview.h"
#include "ruamoko/qwaq/debugger/views/nameview.h"

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

@implementation AlgebraView

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	return self;
}

+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	string typename = nil;
	if (type.type == ty_alias) {
		type = type.alias.aux_type;
	}
	if (type.type >= 0
		&& type.type < sizeof(type_views) / sizeof (type_views[0])) {
		typename = type_views[type.type];
	}
	//printf ("AlgebraView: %s\n", typename);
	id class = obj_lookup_class (typename);
	if (class) {
		return [class withDef:def in:data type:type];
	}
	return [NameView withName:"Invalid Type"];
}
#if 0
+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type
{
	string typename = nil;
	if (type.type == ty_alias) {
		type = type.alias.aux_type;
	}
	if (type.type >= 0
		&& type.type < sizeof(type_views) / sizeof (type_views[0])) {
		typename = type_views[type.type];
	}
	printf ("AlgebraView: %s\n", typename);
	id class = obj_lookup_class (typename);
	if (class) {
		return [class withDef:def in:data type:type];
	}
	return [NameView withName:"Invalid Type"];
}
#endif
@end
