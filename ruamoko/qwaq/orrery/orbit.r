#include <PropertyList.h>
#include <math.h>

#include "orbit.h"

void printf (string fmt, ...);

@implementation Orbit

-initWithPList:(PLItem *)plitem
{
	if (!(self = [super init])) {
		return nil;
	}
	periapsis =    [[plitem getObjectForKey:"QR"] number];
	eccentricity = [[plitem getObjectForKey:"EC"] number];;
	epoch =        [[plitem getObjectForKey:"TP"] number];

	double lan =  [[plitem getObjectForKey:"OM"] number] * M_PI / 180;
	double argp = [[plitem getObjectForKey: "W"] number] * M_PI / 180;
	double inc =  [[plitem getObjectForKey:"IN"] number] * M_PI / 180;

	dvec2 i = sincos (inc);
	dvec2 a = sincos (lan);
	dvec2 p = sincos (argp);

#define s x
#define c y
	frame[0] = [ p.c*a.c - p.s*a.s*i.c, a.s*p.c + p.s*a.c*i.c, p.s*i.s];
	frame[1] = [-p.s*a.c - p.c*a.s*i.c,-a.s*p.s + p.c*a.c*i.c, p.c*i.s];
	frame[2] = [               a.s*i.s,              -a.c*i.s,     i.c];
	mean_motion = 0;//XXX need the body
	period = 0;//XXX need the body
	printf ("    %g %g %g\n", periapsis, eccentricity, epoch);
	printf ("    %g %g %g\n", lan, argp, inc);
	printf ("    [%g %g] [%g %g] [%g %g]\n", a.c, a.s, p.c, p.s, i.c, i.s);
	printf ("    [%g %g %g]\n", frame[0].x, frame[0].y, frame[0].z);
	printf ("    [%g %g %g]\n", frame[1].x, frame[1].y, frame[1].z);
	printf ("    [%g %g %g]\n", frame[2].x, frame[2].y, frame[2].z);
#undef s
#undef c
	return self;
}

+(Orbit *)orbit:(PLItem *)plitem
{
	return [[[Orbit alloc] initWithPList:plitem] autorelease];
}

-(conic_t)conicData
{
	@algebra (PGA) {
		return {
			.point = e021 + e123,
			.s = point_t (frame[0], 0),
			.t = point_t (frame[1], 0),
			.slr = (float) (periapsis * (1 + eccentricity)) * 0.2,
			.ecc = (float) eccentricity,
			.width = 2,
			.color = 0xfffffff,	//white
		};
	}
}

@end
