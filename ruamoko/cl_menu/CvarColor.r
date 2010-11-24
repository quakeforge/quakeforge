#include "cvar.h"

#include "options_util.h"
#include "CvarColor.h"

@implementation CvarColor
-(void)next
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (0, 13, 1, val, 1);
	Cvar_SetFloat (name, val);
}

-(void)prev
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (0, 13, 1, val, 0);
	Cvar_SetFloat (name, val);
}

-(integer)value
{
	return Cvar_GetInteger (name);
}
@end
