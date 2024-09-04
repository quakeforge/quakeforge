#include <string.h>

#include "vkbool.h"
#include "vkgen.h"

@implementation Bool

-(string) name
{
	return "bool";
}

-(string) cexprType
{
	return "cexpr_bool";
}

-(string) parseType
{
	return "QFString";
}
@end
