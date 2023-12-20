#include <QF/keys.h>
#include "cvar.h"

#include "CvarToggle.h"

@implementation CvarToggle
-(void)toggle
{
	Cvar_Toggle (name);
}

-(BOOL)value
{
	return Cvar_GetInteger (name);
}
@end
