#ifndef __qwaq_orrery_orbiter_h
#define __qwaq_orrery_orbiter_h

#include <Object.h>

@class Body;
@class PhysicalOrbit;
@class Array;
@class PLItem;

@interface Orbiter : Object
{
	Body *parent;
	Array *children;

	PhysicalOrbit *orbit;
}
+(Orbiter *)orbiter:(PLItem *)plitem;
-(PhysicalOrbit *)orbit;
@end

#endif//__qwaq_orrery_orbiter_h
