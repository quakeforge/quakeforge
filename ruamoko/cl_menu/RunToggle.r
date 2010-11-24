#include "cvar.h"

#include "RunToggle.h"

@implementation RunToggle
-(void)toggle
{
	if (Cvar_GetFloat ("cl_forwardspeed") < 400) {
		Cvar_SetFloat ("cl_forwardspeed", 400);
		Cvar_SetFloat ("cl_backspeed", 400);
	} else {
		Cvar_SetFloat ("cl_forwardspeed", 200);
		Cvar_SetFloat ("cl_backspeed", 200);
	}
}

-(BOOL)value
{
	return Cvar_GetFloat  ("cl_forwardspeed") >= 400;
}
@end
