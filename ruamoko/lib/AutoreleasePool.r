#include "AutoreleasePool.h"
//#include "Stack.h"

@static AutoreleasePool	sharedInstance;
//@static Stack			poolStack;

@interface AutoreleasePool (Private)
- (void) addItem: (id)anItem;
@end

@implementation AutoreleasePool

- (id) init
{
	if (!(self = [super init]))
		return NIL;

	//if (!poolStack)
	//	poolStack = [Stack new];

	if (!sharedInstance)
		sharedInstance = self;

	array = [[Array alloc] init];
	return self;
}

+ (void) addObject: (id)anObject
{
	if (!sharedInstance)
		[[AutoreleasePool alloc] init];
	[sharedInstance addObject: anObject];
}

- (void) addObject: (id)anObject
{
	[array addItem: anObject];
	[anObject release];		// the array retains the item, and releases when
							// dealloced
}

- (id) retain
{
	[self error: "Don't send -retain to an autorelease pool."];
}

+ (void) release
{
	[sharedInstance release];
	sharedInstance = NIL;
}

- (/*oneway*/ void) release
{
	[self dealloc];
}

- (void) dealloc
{
	//local id		tmp;

	[array release];

	/*
		This may be wrong.
		Releasing an autorelease pool should keep popping pools off the stack
		until it gets to itself.
	*/
	//do {
	//	tmp = [poolStack pop];
	//} while (tmp != self);

	sharedInstance = NIL;
	[super dealloc];
}
@end
