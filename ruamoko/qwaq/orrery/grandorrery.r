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
	uint count = [objects count];
	[central_object updatePosition:time ref:central_object];
	if (count != conic_count) {
		obj_free (conic_buffer);
		conic_buffer = obj_malloc (sizeof (conic_t) * count);
		conic_count = count;
	}
	for (uint i = 0; i < count; i++) {
		Orbiter *orbiter = [objects objectAtIndex:i];
		if ([orbiter orbit]) {
			conic_t conic = [orbiter conicData];
			conic_buffer[num_conics++] = conic;
		}

		//FIXME qfcc uses 3 convs instead of a vector
		auto pos = vec4 ([orbiter position], 1);
		double r = [orbiter radius];
		if (r) {
			r = log(r / 6370e3);
			r = (sqrt (r * r + 1) + r) / 2;
		}
		Gizmo_AddSphere (pos, 0.0001+(float)r*0.05f, [orbiter color]);
	}
	Render_UpdateBuffer ("orrery", 0, conic_buffer,
						 num_conics * sizeof (conic_t));
	ulong addr = Render_BufferAddress ("orrery");
	Render_SetBlackboardVar ("num_conics", num_conics);
	Render_SetBlackboardVar ("conics", addr);
	return self;
}

@end
