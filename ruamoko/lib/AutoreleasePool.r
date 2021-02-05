#include <AutoreleasePool.h>

#include <Array.h>
#include "Array+Private.h"

@static Array	*poolStack;

@implementation AutoreleasePool

- (id) init
{
	if (!(self = [super init]))
		return nil;

	if (!poolStack)
		poolStack = [[Array alloc] initWithCapacity: 1];

	[poolStack addObjectNoRetain: self];

	array = [Array new];
	return self;
}

+ (void) addObject: (id)anObject
{
	if (!poolStack || ![poolStack count])
		[[AutoreleasePool alloc] init];

	[[poolStack lastObject] addObject: anObject];
}

- (void) addObject: (id)anObject
{
	[array addObjectNoRetain: anObject];
}

- (id) retain
{
	[self error: "Don't send -retain to an autorelease pool."];
	return self;
}

- (id) autorelease
{
	[self error: "Don't send -autorelease to an autorelease pool."];
	return self;
}

- (void) dealloc
{
	[array release];
	[poolStack removeObjectNoRelease: self];
	[super dealloc];
}

@end

void
ARP_FreeAllPools (void)
{
	[poolStack removeAllObjects];
}
