#ifndef __basislayout_h
#define __basislayout_h
#include <Object.h>

@class BasisGroup;
@class Set;

@interface BasisLayout : Object
{
	int         count;
	uivec2      range;
	BasisGroup **groups;
	ivec3      *group_map;
	int        *mask_map;
	int         blade_count;
	Set        *set;
}
+(BasisLayout *) new:(int) count groups:(BasisGroup **)groups;
-(int)count;
-(int)num_components;
-(int)blade_count;
-(BasisGroup *) group:(int) ind;
-(BasisBlade *) bladeAt:(int) ind;
-(int) bladeIndex:(unsigned) mask;
-(Set *) set;
@end

#endif//__basislayout_h
