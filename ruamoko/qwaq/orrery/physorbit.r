#include <PropertyList.h>
#include <math.h>

#include "orbit.h"
#include "physorbit.h"

void printf (string fmt, ...);

@implementation PhysicalOrbit

-initWithPList:(PLItem *)plitem
{
	if (!(self = [super init])) {
		return nil;
	}
	params = {
		.e = [[plitem getObjectForKey:"EC"] number],
		.p = [[plitem getObjectForKey:"QR"] number] * 0.2,//FIXME
		.i = [[plitem getObjectForKey:"IN"] number],
		.W = [[plitem getObjectForKey:"OM"] number],
		.w = [[plitem getObjectForKey: "W"] number],
		.t = [[plitem getObjectForKey:"TP"] number],
	};

	orbit = [[Orbit orbit:params.e] retain];

	dvec2 i = sincos (params.i * (M_PI / 180));
	dvec2 a = sincos (params.W * (M_PI / 180));
	dvec2 p = sincos (params.w * (M_PI / 180));

#define s x
#define c y
	frame[0] = [ p.c*a.c - p.s*a.s*i.c, a.s*p.c + p.s*a.c*i.c, p.s*i.s];
	frame[1] = [-p.s*a.c - p.c*a.s*i.c,-a.s*p.s + p.c*a.c*i.c, p.c*i.s];
	frame[2] = [               a.s*i.s,              -a.c*i.s,     i.c];
	mean_motion = 1;//XXX need the body
	//period = 0;//XXX need the body
	printf ("    %g %g %g\n", params.p, params.e, params.t);
	printf ("    %g %g %g\n", params.W, params.w, params.i);
	printf ("    [%g %g] [%g %g] [%g %g]\n", a.c, a.s, p.c, p.s, i.c, i.s);
	printf ("    [%g %g %g]\n", frame[0].x, frame[0].y, frame[0].z);
	printf ("    [%g %g %g]\n", frame[1].x, frame[1].y, frame[1].z);
	printf ("    [%g %g %g]\n", frame[2].x, frame[2].y, frame[2].z);
#undef s
#undef c
	return self;
}

+(PhysicalOrbit *)orbit:(PLItem *)plitem
{
	return [[[PhysicalOrbit alloc] initWithPList:plitem] autorelease];
}

-(conic_t)conicData
{
	@algebra (PGA) {
		return {
			.point = e021 + e123,
			.s = point_t (frame[0], 0),
			.t = point_t (frame[1], 0),
			.slr = (float) (params.p * (1 + params.e)),
			.ecc = (float) params.e,
			.width = 2,
			.color = 0xfffffff,	//white
		};
	}
}

-(dvec3)position:(double)time
{
	double M = (time - params.t) * mean_motion;
	auto sc = [orbit ta_sincos:M];
	double r = params.p * (1 + params.e) / (1 + params.e * sc.y);
	// sc is sin, cos, so need to swap the frame vectors
	auto p = dmat2x3(frame[1], frame[0]) * sc * r;
	return p;
}

@end
