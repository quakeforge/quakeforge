#ifndef __ruamoko_AutoreleasePool_h
#define __ruamoko_AutoreleasePool_h

#include "Object.h"

@interface AutoreleasePool: Object
{
	/*unsigned*/integer	count;
	id []		array;
}

+ (void) addObject: (id)anObject;
- (void) addObject: (id)anObject;

@end

#endif	// __ruamoko_AutoreleasePool_h
