#include <PropertyList.h>
#include <math.h>

#include "body.h"
#include "grandorrery.h"
#include "orbiter.h"
#include "physorbit.h"

#include "../qwaq-ed.h"
#include "../gizmo.h"

@implementation GrandOrrery

-initWithPList:(PLItem *)plitem
{
	if (!(self = [super init])) {
		return nil;
	}

	objects = [[Array array] retain];

	for (int i = 0; i < [plitem count]; i++) {
		PLItem *obj_item = [plitem getObjectAtIndex:i];
		Orbiter *obj = [Orbiter orbiter:obj_item orrery:self];
		[objects addObject:obj];
	}
	central_object = [objects objectAtIndex:3];
	return self;
}

+(GrandOrrery *)orrery:(PLItem *)plitem
{
	return [[[GrandOrrery alloc] initWithPList:plitem] autorelease];
}

-(Body *)findBody:(string)name
{
	for (int i = [objects count]; i-- > 0; ) {
		Orbiter *obj = [objects objectAtIndex:i];
		if ([obj class] == [Body class] && [obj name] == name) {
			return (Body *) obj;
		}
	}
	return nil;
}

-update:(double) time
{
	uint num_conics = 0;
	int count = [objects count];
	[central_object updatePosition:time ref:central_object];
	for (int i = 0; i < count; i++) {
		Orbiter *orbiter = [objects objectAtIndex:i];
		conic_t conic = [orbiter conicData];
		Render_UpdateBuffer ("orrery", num_conics * sizeof(conic_t),
							 &conic, sizeof (conic));
		num_conics++;

		//FIXME qfcc uses 3 convs instead of a vector
		auto pos = vec4 ([orbiter position], 1);
		double r = [orbiter radius];
		if (r) {
			r = log(r / 6370e3);
			r = (sqrt (r * r + 1) + r) / 2;
		}
		Gizmo_AddSphere (pos, 0.0001+(float)r*0.05f, [orbiter color]);
	}
	ulong addr = Render_BufferAddress ("orrery");
	Render_SetBlackboardVar ("num_conics", num_conics);
	Render_SetBlackboardVar ("conics", addr);
	return self;
}

@end
