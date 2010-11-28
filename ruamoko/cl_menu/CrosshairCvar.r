#include "cvar.h"

#include "CrosshairCvar.h"

@implementation CrosshairCvar
-(void) next
{
	local integer val = Cvar_GetInteger (name);
	Cvar_SetInteger (name, (val + 1) % 4);
}

-(integer) crosshair
{
	return Cvar_GetInteger (name);
}
@end
