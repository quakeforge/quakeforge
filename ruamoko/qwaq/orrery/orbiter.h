#ifndef __qwaq_orrery_orbiter_h
#define __qwaq_orrery_orbiter_h

#include <Object.h>

@class Body;
@class Orbit;
@class Array;
@class PLItem;

@interface Orbiter : Object
{
	Body *parent;
	Array *children;

	Orbit *orbit;
}
+(Orbiter *)orbiter:(PLItem *)plitem;
-(Orbit *)orbit;
@end

#endif//__qwaq_orrery_orbiter_h
