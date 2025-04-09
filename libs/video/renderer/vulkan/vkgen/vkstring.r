#include <string.h>

#include "vkfieldstring.h"
#include "vkstring.h"
#include "vkgen.h"

@implementation String

-(string) name
{
	return "string";
}

-(FieldDef *)fielddef:(Struct *)strct field:(string)fname
{
	return [StringField fielddef:nil struct:strct field:fname];
}

-(string) cexprType
{
	return [self name] + "_type";
}

-(string) parseType
{
	return "QFString";
}
@end
