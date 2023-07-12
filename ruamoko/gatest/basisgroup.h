#ifndef __basisgroup_h
#define __basisgroup_h
#include <Object.h>

@class Metric;
@class BasisBlade;
@class Set;

@interface BasisGroup : Object
{
	int         count;
	uivec2      range;
	BasisBlade **blades;
	int        *map;
	Set        *set;
}
+(BasisGroup *) new:(int) count basis:(BasisBlade **)blades;
-(int)count;
-(uivec2)blade_range;
-(BasisBlade *) bladeAt:(int) ind;
-(BasisBlade *) blade:(unsigned) mask;
-(Set *) set;
@end

#endif//__basisgroup_h
