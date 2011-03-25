#include "cvar.h"

#include "CrosshairCvar.h"

@implementation CrosshairCvar
-(void) next
{
	local int val = Cvar_GetInteger (name);
	Cvar_SetInteger (name, (val + 1) % 4);
}

-(int) crosshair
{
	return Cvar_GetInteger (name);
}
@end
