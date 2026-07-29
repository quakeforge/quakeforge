#ifndef __orrery_orbit_h
#define __orrery_orbit_h

#include <Object.h>

#include "../shader/conic.h"

@interface Orbit : Object
{
	@private
	double eccentricity;
}
+(Orbit *) orbit:(double)e;
-(conic_t) conicData;
-(dvec2)ta_sincos:(double)M;
@end

#endif//__orrery_orbit_h
