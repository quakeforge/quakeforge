#include "AutoreleasePool.h"
#include "Stack.h"

@static AutoreleasePool	sharedInstance;
@static Stack			poolStack;

@interface AutoreleasePool (Private)
- (void) addItem: (id)anItem;
@end

@implementation AutoreleasePool

- (id) init
{
	if (!(self = [super init]))
		return NIL;

	if (!poolStack)
		poolStack = [Stack new];

	if (!sharedInstance)
		sharedInstance = self;
}

+ (void) addObject: (id)anObject
{
}

- (void) addObject: (id)anObject
{
}

- (id) retain
{
	[self error: "Don't send -retain to an autorelease pool."];
}

- (/*oneway*/ void) release
{
	if (self == sharedInstance)
		sharedInstance = NIL;

	[self dealloc];
}

- (void) dealloc
{
	local integer	i;
	local id		tmp;

	for (i = 0; i < count; i++)
		[array[i] release];

	obj_free (array);

	/*
		This may be wrong.
		Releasing an autorelease pool should keep popping pools off the stack
		until it gets to itself.
	*/
	do {
		tmp = [poolStack pop];
	} while (tmp != self);

	[super dealloc];
}
@end
