#include "cvar.h"

#include "CvarString.h"

@implementation CvarString
-(void)setString: (string) str
{
	Cvar_SetString (name, str);
}

-(string)value
{
	return Cvar_GetString (name);
}
@end
