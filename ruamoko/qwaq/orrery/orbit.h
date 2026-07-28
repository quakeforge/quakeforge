#ifndef __orrery_orbit_h
#define __orrery_orbit_h

#include <Object.h>

@class PLItem;

typedef struct plitem_s plitem_t;

#include "../shader/conic.h"

@interface Orbit : Object
{
	dmat3 frame;
	double eccentricity;
	double periapsis;
	double mean_motion;
	double period;
	double epoch;
}
+(Orbit *) orbit:(PLItem *)plitem;
-(conic_t) conicData;
@end

#endif//__orrery_orbit_h
