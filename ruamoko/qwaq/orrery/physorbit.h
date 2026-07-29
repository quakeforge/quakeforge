#ifndef __orrery_physorbit_h
#define __orrery_physorbit_h

#include <Object.h>

@class PLItem;
@class Body;
@class Orbit;

#include "../shader/conic.h"

typedef struct po_params_s {
	double e;// eccentricity
	double p;// periapsis distance
	double i;// inclination (degrees)
	double W;// longitude of ascending node (degrees)
	double w;// argument of periapsis
	double v;// true anomaly
	double t;// epoch
} po_params_t;

@interface PhysicalOrbit : Object
{
	Orbit *orbit;
	dmat3 frame;
	po_params_t params;
	double mean_motion;
}
+(PhysicalOrbit *) orbit:(PLItem *)plitem parent:(Body *)parent;
-(conic_t)conicData;
-(dvec3)position:(double)time;
@end

#endif//__orrery_physorbit_h
