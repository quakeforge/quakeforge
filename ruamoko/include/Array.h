#ifndef __ruamoko_Array_h
#define __ruamoko_Array_h

#include <Object.h>
#include <runtime.h>

/**
	The Array class is a general ordered collection class.

	The %Array class manages an ordered collection of objects.
	If you want to subclass Array, you need to override these methods:

	\li #count()
	\li #objectAtIndex:
	\li #addObject:
	\li #insertObject:atIndex:
	\li #removeObjectAtIndex:
	\li #removeLastObject
	\li #replaceObjectAtIndex:withObject:
*/
@interface Array: Object //<Copying>
{
	unsigned	count;			///< The number of objects currently contained
	unsigned	capacity;		///< The number of objects that can be held right now
	unsigned	granularity;	///< The number of pointers allocated at a time (based on initial capacity)
	id			*_objs;		///< A primitive array of pointers to objects
}

///\name Creating arrays
//\{
/**
	Create and return an empty array with the default initial capacity.
*/
+ (id) array;

/**
	Creates and returns an empty array with initial capacity \a cap.

	\param	cap	The initial capacity of the array.
*/
+ (id) arrayWithCapacity: (unsigned)cap;

/**
	Returns a copy of \a array, retaining its contents.
*/
+ (id) arrayWithArray: (Array *)array;

/**
	Create an array containing only \a anObject .
*/
+ (id) arrayWithObject: (id)anObject;

/**
	Create an array from a list of objects.

	\warning Due to the nature of the Ruamoko/QuakeC language, do not supply
	more than 6 objects to this method.
*/
+ (id) arrayWithObjects: (id)firstObj, ...;

/**
	Create and return an array containing the first \a count objects from
	primitive array \a objs.

	\warning Do not supply a primitive array containing fewer than \a count
	objects.
*/
+ (id) arrayWithObjects: (id *)objs
                  count: (unsigned)cnt;
//\}

///\name Initializing arrays
//\{
/**
	Initialize the receiver with capacity \a cap
*/
- (id) initWithCapacity: (unsigned)cap;

/**
	Initialize the receiver with objects from \a array.
*/
- (id) initWithArray: (Array *)array;

/**
	Initialize the receiver with objects from \a array.

	\param	array	The array to duplicate
	\param	flag 	if #YES, copies the contents instead of retaining them.
*/
- (id) initWithArray: (Array *)array
           copyItems: (BOOL)flag;

/**
	Initialize the receiver with a list of objects.

	\warning Due to the nature of the Ruamoko/QuakeC language, do not supply
	more than 6 objects to this method.
*/
- (id) initWithObjects: (id)firstObj, ...;

/**
	Initialize the receiver with a primitive array of objects.
*/
- (id) initWithObjects: (id *)objs
                 count: (unsigned)count;
//\}

///\name Querying an array
//\{
/**
	Returns #YES if the receiver contains \a anObject.

	The #isEqual: method is used to determine this, so that (for example) a
	newly-created string object can be compared to one already in the array.
*/
- (BOOL) containsObject: (id)anObject;

/**
	Returns the number of objects contained in the receiver.
*/
- (unsigned) count;

/**
	Returns the object contained at index \a index.
*/
- (id) objectAtIndex: (unsigned)index;

/**
	Returns the contained object with the highest index.
*/
- (id) lastObject;
//- (Enumerator) objectEnumerator;
//- (Enumerator) reverseObjectEnumerator;

#if 0
/**
	Copies all object references in the range \a aRange to \a aBuffer.

	\warning The destination buffer must be large enough to hold all contents.
*/
- (BOOL) getObjects: (id *)aBuffer
              range: (Range)aRange;
#endif
//\}

///\name Finding objects
//\{
/**
	Returns the lowest index of an object equal to \a anObject.

	If no object is equal, returns #NotFound.
*/
- (unsigned) indexOfObject: (id)anObject;

#if 0
/**
	Returns the lowest index, within the range \a aRange, of an object equal
	to \a anObject.

	If no object is equal, returns #NotFound.
*/
- (unsigned) indexOfObject: (id)anObject
                   inRange: (Range)aRange;
#endif

/**
	Returns the lowest index of an object with the same address as \a anObject.

	Returns #NotFound if \a anObject is not found within the array.
*/
- (unsigned) indexOfObjectIdenticalTo: (id)anObject;

#if 0
/**
	Returns the lowest index, within the range \a aRange, of an object with
	the same address as \a anObject.

	Returns #NotFound if \a anObject is not found within the range.
*/
- (unsigned) indexOfObjectIdenticalTo: (id)anObject
                              inRange: (Range)aRange;
#endif
//\}

///\name Adding objects
//\{

/**
	Adds a single object to an array
*/
- (void) addObject: (id)anObject;

/**
	Adds each object in \a array to the receiver, retaining it.
*/
- (void) addObjectsFromArray: (Array *)array;

/**
	Adds a single object to an array

	Adds \a anObject to the receiver at index \a index, pushing any objects
	with a higher index to the next higher slot.
*/
- (void) insertObject: (id)anObject
              atIndex: (unsigned)index;
//\}

///\name Replacing Objects
//\{
/**
	Removes object at \a index, replacing it with \a anObject.
*/
- (void) replaceObjectAtIndex: (unsigned)index
                   withObject: (id)anObject;

/**
	Replaces the object currently in the receiver with those from \a array.

	\note If \a array and self are the same object, this method has no effect.
*/
- (void) setArray: (Array *)array;
//\}

///\name Removing Objects
//\{

/**
	Recursively removes all objects from the receiver by sending
	#removeLastObject to \a self, until #count() returns zero.
*/
- (void) removeAllObjects;

/**
	Removes from the receiver the contained object with the highest index.
*/
- (void) removeLastObject;

/**
	Finds and removes all objects from the receiver that are equal to
	\a anObject, by sending each #isEqual: with \a anObject as the argument.
*/
- (void) removeObject: (id)anObject;

/**
	Finds and removes all objects from the receiver that are equal to any
	objects in array \a anArray.
*/
- (void) removeObjectsInArray: (Array *)anArray;

/**
	Finds and removes all objects from the receiver that are \b exactly equal
	to \a anObject, using a direct address comparison.
*/
- (void) removeObjectIdenticalTo: (id)anObject;

/**
	Removes the object located at index \a index, moving each object with a
	higher index down one position.
*/
- (void) removeObjectAtIndex: (unsigned)index;
//\}

///\name Sending Messages to Elements
//\{
/**
	Iteratively sends #performSelector: to each contained object.
*/
- (void) makeObjectsPerformSelector: (SEL)selector;

/**
	Iteratively sends #performSelector:withObject: to each contained object.
*/
- (void) makeObjectsPerformSelector: (SEL)selector
                         withObject: (id)arg;
//\}

@end

#endif//__ruamoko_Array_h
