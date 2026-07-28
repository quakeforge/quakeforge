#include <PropertyList.h>
#include <math.h>

#include "orbiter.h"
#include "orbit.h"
#include "body.h"

@implementation Orbiter

-initWithPList:(PLItem *)plitem
{
	if (!(self = [super init])) {
		return nil;
	}
	auto o = [plitem getObjectForKey:"orbit"];
	if (o) {
		orbit = [[Orbit orbit:o] retain];
	}

	return self;
}

+(Orbiter *)orbiter:(PLItem *)plitem
{
	if ([plitem getObjectForKey:"radius"]) {
		return [Body body:plitem];
	} else {
		return [[[Orbiter alloc] initWithPList:plitem] autorelease];
	}
}

-(Orbit *)orbit
{
	return orbit;
}
@end
