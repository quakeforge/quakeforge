#ifndef __ruamoko_AutoreleasePool_h
#define __ruamoko_AutoreleasePool_h

#include "Object.h"

@class Array;

@interface AutoreleasePool: Object
{
	Array		array;
}

+ (void) addObject: (id)anObject;
- (void) addObject: (id)anObject;

@end

void ARP_FreeAllPools (void);

#endif	// __ruamoko_AutoreleasePool_h
