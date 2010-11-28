#include "cvar.h"

#include "MouseToggle.h"

@implementation MouseToggle
-(void)toggle
{
	if (Cvar_GetFloat ("m_pitch") < 0) {
		Cvar_SetFloat ("m_pitch", 0.022);
	} else {
		Cvar_SetFloat ("m_pitch", -0.022);
	}
}

-(BOOL)value
{
	return Cvar_GetFloat ("m_pitch") < 0;
}
@end
