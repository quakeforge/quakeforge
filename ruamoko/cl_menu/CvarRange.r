#include "cvar.h"

#include "options_util.h"
#include "CvarRange.h"

@implementation CvarRange

-(id)initWithCvar:(string)cvname min:(float)_min max:(float)_max step:(float)_step
{
	self = [super initWithCvar: cvname];

	min = _min;
	max = _max;
	step = _step;

	return self;
}

-(void)inc
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (min, max, step, val, 1);
	Cvar_SetFloat (name, val);
}

-(void)dec
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (min, max, step, val, 0);
	Cvar_SetFloat (name, val);
}

-(float)value
{
	return Cvar_GetFloat (name);
}

-(integer)percentage
{
	return to_percentage(min, max, Cvar_GetFloat (name));
}

@end
