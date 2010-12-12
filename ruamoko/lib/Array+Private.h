#ifndef __ruamoko_Array_Private_h
#define __ruamoko_Array_Private_h

#include <Array.h>

/**
	Internal Array methods
*/
@interface Array (Private)

///\name Private methods
//\{
/**
	Adds an object to the receiver, but without retaining it.

	This is a dangerous thing to do, and it's only done so that we can use an 
	%Array to implement the AutoreleasePool class.

	\warning Using this method can result in crashes, and is only included 
	for internal use by other classes.
*/
- (void) addObjectNoRetain: (id)anObject;

/**
	Removes an object from the receiver, but without releasing it.

	This can leak objects, and is only used so that we can use Arrays to 
	implement the autorelease system.
*/
- (void) removeObjectNoRelease: (id)anObject;
//\}

@end

#endif //__ruamoko_Array_Private_h
