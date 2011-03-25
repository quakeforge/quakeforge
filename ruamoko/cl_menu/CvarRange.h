#ifndef __CvarRange_h
#define __CvarRange_h

#include "CvarObject.h"

@interface CvarRange : CvarObject
{
	float min, max, step;
}
-(id)initWithCvar:(string)cvname min:(float)_min max:(float)_max step:(float)_step;
-(void)inc;
-(void)dec;
-(float)value;
-(int)percentage;
@end

#endif//__CvarRange_h
