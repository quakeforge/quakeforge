#include <PropertyList.h>

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
		printf ("%s\n", [[obj_item getObjectForKey:"name"] string]);
		Orbiter *obj = [Orbiter orbiter:obj_item];
		[objects addObject:obj];
	}
	return self;
}

+(GrandOrrery *)orrery:(PLItem *)plitem
{
	return [[[GrandOrrery alloc] initWithPList:plitem] autorelease];
}

-update:(double) time
{
	uint num_conics = 0;
	int count = [objects count];
	for (int i = 0; i < count; i++) {
		PhysicalOrbit *orbit = [[objects objectAtIndex:i] orbit];
		if (orbit) {
			conic_t conic = [orbit conicData];
			Render_UpdateBuffer ("orrery", num_conics * sizeof(conic_t),
								 &conic, sizeof (conic));
			num_conics++;

			//FIXME qfcc uses 3 convs instead of a vector
			vec3 pos = vec3 ([orbit position:time]);
			Gizmo_AddSphere ((vec4) conic.point + vec4(pos, 0), 0.1,
							 { 0.5, 0.5, 1, 0.9});
		}
	}
	ulong addr = Render_BufferAddress ("orrery");
	Render_SetBlackboardVar ("num_conics", num_conics);
	Render_SetBlackboardVar ("conics", addr);
	return self;
}

@end
