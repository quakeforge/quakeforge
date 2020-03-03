#ifndef __ruamoko_AutoreleasePool_h
#define __ruamoko_AutoreleasePool_h

#include <Object.h>

@class Array;

/**
	The AutoreleasePool class is an assistant class for reference-counted
	object management.

	The Ruamoko standard library implements object management using a
	reference counting scheme, and sometimes it is useful to create objects
	that have a limited lifespan. For these objects, it is often useful to not
	have to explicitly free them -- thus the &ldquo;autorelease&rdquo; system.

	Autorelease is best described as a &ldquo;delayed release&rdquo;, to
	complement the Object::retain and Object::release methods. It is used
	when an object is being created on behalf of another object, where the
	client is not necessarily expected to assume ownership over the object
	created.
*/
@interface AutoreleasePool: Object
{
	Array	*array;				///< a list of objects awaiting release
}

/**
	Adds \a anObject to the currently-active (most recently created)
	autorelease pool.

	\note You should generally not use this method. Instead, send #autorelease
	to the object you want to add to the pool.
*/
+ (void) addObject: (id)anObject;

/**
	Adds \a anObject to the receiver.

	\note You should generally not use this method (there isn't much reason to
	add an object to a \a specific autorelease pool). Instead, send
	#autorelease to the object you want to add to a pool.
*/
- (void) addObject: (id)anObject;

/**
	Generates an error. Never send this message to an instance of this class.
*/
- (id) retain;

/**
	Generates an error. Never send this message to an instance of this class.
*/
- (id) autorelease;

@end

@extern void ARP_FreeAllPools (void);

#endif	// __ruamoko_AutoreleasePool_h
