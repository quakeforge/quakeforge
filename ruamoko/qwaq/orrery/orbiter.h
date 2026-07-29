#ifndef __qwaq_orrery_orbiter_h
#define __qwaq_orrery_orbiter_h

#include <Object.h>

@class Body;
@class GrandOrrery;
@class PhysicalOrbit;
@class Array;
@class PLItem;

#include "../shader/conic.h"

@interface Orbiter : Object
{
	string name;
	Body *parent;
	Array *children;

	PhysicalOrbit *orbit;

	dvec3 position;
	uint color;
}
+(Orbiter *)orbiter:(PLItem *)plitem orrery:(GrandOrrery *)orrery;
-addChild:(Orbiter *)child;
-(PhysicalOrbit *)orbit;
-(conic_t)conicData;
-updatePosition:(double)time ref:(Orbiter *)ref;
-(string)name;
-(dvec3)position;
-(vec4)color;
@end

#endif//__qwaq_orrery_orbiter_h
