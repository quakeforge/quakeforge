#include <PropertyList.h>
#include <math.h>

#include "body.h"
#include "grandorrery.h"

void printf (string fmt, ...);

@implementation Body

-initWithPList:(PLItem *)plitem orrery:(GrandOrrery *)orrery
{
	if (!(self = [super initWithPList:plitem orrery:orrery])) {
		return nil;
	}

	PLItem *axis = [plitem getObjectForKey:"axis"];
	double ra =  [[axis getObjectForKey:"ra"]  number] * M_PI / 180;
	double dec = [[axis getObjectForKey:"dec"] number] * M_PI / 180;

	double mass = [[plitem getObjectForKey:"mass"] number];
	mu = mass * G;

	radius = [[plitem getObjectForKey:"radius"] number];
	rot_speed = [[axis getObjectForKey:"W"] number];
	rot_epoch = [[axis getObjectForKey:"W???"] number];//XXX
	rot_at_epoch = [[axis getObjectForKey:"W???"] number];//XXX

	dvec2 r = axis ? sincos (ra)  : (dvec2) { 0, 1 };
	dvec2 d = axis ? sincos (dec) : (dvec2) { 1, 0 };
	double time = 0;//XXX
	dvec2 t = sincos (time);

#define s x
#define c y
	frame[0] = [-r.s*t.s + r.c*d.s*t.c,  r.c*t.s + r.s*d.s*t.c, d.c*t.c];
	frame[1] = [-r.s*t.c - r.c*d.s*t.s,  r.c*t.c - r.s*d.s*t.s, d.c*t.s];
	frame[2] = [           r.c*d.c,                r.s*d.c,     d.s    ];

	printf ("    %g %g\n", mu, radius);
	printf ("    [%g %g %g]\n", frame[0].x, frame[0].y, frame[0].z);
	printf ("    [%g %g %g]\n", frame[1].x, frame[1].y, frame[1].z);
	printf ("    [%g %g %g]\n", frame[2].x, frame[2].y, frame[2].z);

	return self;
}

+(Body *)body:(PLItem *)plitem orrery:(GrandOrrery *)orrery
{
	return [[[Body alloc] initWithPList:plitem orrery:orrery] autorelease];
}

-(double)mu
{
	return mu;
}

-(double)radius
{
	return radius;
}
@end
