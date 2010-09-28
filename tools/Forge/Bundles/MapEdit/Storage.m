/* Implementation of Objective C NeXT-compatible Storage object
	 Copyright (C) 1993,1994, 1996 Free Software Foundation, Inc.

	 Written by:  Kresten Krab Thorup <krab@iesd.auc.dk>
	 Dept. of Mathematics and Computer Science, Aalborg U., Denmark

	 This file is part of the GNUstep Base Library.

	 This library is free software; you can redistribute it and/or
	 modify it under the terms of the GNU Library General Public
	 License as published by the Free Software Foundation; either
	 version 2 of the License, or (at your option) any later version.
	 
	 This library is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	 Library General Public License for more details.

	 You should have received a copy of the GNU Library General Public
	 License along with this library; if not, write to the Free
	 Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* #include <config.h> */
#include "Storage.h"
#include <string.h>
#include <strings.h>


#define GNU_STORAGE_NTH(x,N)                          \
	({ GNUStorageId* __s=(GNUStorageId*)(x);            \
	   (void*)(((char*)__s->dataPtr)+(__s->elementSize*(N))); })
#define STORAGE_NTH(N) GNU_STORAGE_NTH (self, N)

typedef struct {
	@defs (Storage)
} GNUStorageId;

@implementation Storage

+initialize
{
	if (self ==[Storage class])
		[self setVersion:0];				/* beta release */
	return self;
}

// INITIALIZING, FREEING;

-initCount: (NSUInteger) numSlots elementSize: (NSUInteger) sizeInBytes description:(const char *)
	elemDesc;
{
	[super init];
	numElements = numSlots;
	maxElements = (numSlots > 0) ? numSlots : 1;
	elementSize = sizeInBytes;
	description = elemDesc;
	dataPtr = (void *) objc_malloc (maxElements * elementSize);
	bzero (dataPtr, numElements * elementSize);
	return self;
}

-init
{
	return[self initCount: 1 elementSize:sizeof (id)
	description:@encode (id)];
}


-(void) dealloc
{
	if (dataPtr)
		free (dataPtr);
	[super dealloc];
}

-(const char *) description
{
	return description;
}


// COPYING;

-copy
{
	Storage    *c =[super copy];

	c->dataPtr = (void *) objc_malloc (maxElements * elementSize);
	memcpy (c->dataPtr, dataPtr, numElements * elementSize);
	return c;
}

// COMPARING TWO STORAGES;

-(BOOL) isEqual:anObject
{
	if ([anObject isKindOfClass:[Storage class]]
		&&[anObject count] ==[self count]
		&& !memcmp (((GNUStorageId *) anObject)->dataPtr,
					dataPtr, numElements * elementSize))
		return YES;
	else
		return NO;
}

// MANAGING THE STORAGE CAPACITY;

static inline void
_makeRoomForAnotherIfNecessary (Storage * self)
{
	if (self->numElements == self->maxElements) {
		self->maxElements *= 2;
		self->dataPtr = (void *)
			objc_realloc (self->dataPtr, self->maxElements * self->elementSize);
	}
}

static inline void
_shrinkIfDesired (Storage * self)
{
	if (self->numElements < (self->maxElements / 2)) {
		self->maxElements /= 2;
		self->dataPtr = (void *)
			objc_realloc (self->dataPtr, self->maxElements * self->elementSize);
	}
}

-setAvailableCapacity:(NSUInteger) numSlots
{
	if (numSlots > numElements) {
		maxElements = numSlots;
		dataPtr = (void *) objc_realloc (dataPtr, maxElements * elementSize);
	}
	return self;
}

-setNumSlots:(NSUInteger) numSlots
{
	if (numSlots > numElements) {
		maxElements = numSlots;
		dataPtr = (void *) objc_realloc (dataPtr, maxElements * elementSize);
		bzero (STORAGE_NTH (numElements),
				 (maxElements - numElements) * elementSize);
	} else if (numSlots < numElements) {
		numElements = numSlots;
		_shrinkIfDesired (self);
	}
	return self;
}

/* Manipulating objects by index */

#define CHECK_INDEX(IND)  if (IND >= numElements) return 0

-(NSUInteger) count
{
	return numElements;
}

-(void *) elementAt:(NSUInteger) index
{
	CHECK_INDEX (index);
	return STORAGE_NTH (index);
}

-addElement:(void *) anElement
{
	_makeRoomForAnotherIfNecessary (self);
	memcpy (STORAGE_NTH (numElements), anElement, elementSize);
	numElements++;
	return self;
}

-insertElement:(void *)
anElement   at:(NSUInteger) index
{
	NSUInteger	i;

	CHECK_INDEX (index);
	_makeRoomForAnotherIfNecessary (self);
#ifndef STABLE_MEMCPY
	for (i = numElements; i >= index; i--)
		memcpy (STORAGE_NTH (i + 1), STORAGE_NTH (i), elementSize);
#else
	memcpy (STORAGE_NTH (index + 1),
			STORAGE_NTH (index), elementSize * (numElements - index));
#endif
	memcpy (STORAGE_NTH (i), anElement, elementSize);
	numElements++;
	return self;
}

-removeElementAt:(NSUInteger) index
{
	NSUInteger i;

	CHECK_INDEX (index);
	numElements--;
#ifndef STABLE_MEMCPY
	for (i = index; i < numElements; i++)
		memcpy (STORAGE_NTH (i), STORAGE_NTH (i + 1), elementSize);
#else
	memcpy (STORAGE_NTH (index),
			STORAGE_NTH (index + 1), elementSize * (numElements - index - 1));
#endif
	_shrinkIfDesired (self);
	return self;
}

-removeLastElement
{
	if (numElements) {
		numElements--;
		_shrinkIfDesired (self);
	}
	return self;
}

-replaceElementAt:(NSUInteger)
index       with:(void *) newElement
{
	CHECK_INDEX (index);
	memcpy (STORAGE_NTH (index), newElement, elementSize);
	return self;
}

/* Emptying the Storage */

-empty
{
	numElements = 0;
	maxElements = 1;
	dataPtr = (void *) objc_realloc (dataPtr, maxElements * elementSize);
	return self;
}

+new
{
	return[[self alloc] init];
}

+newCount:(NSUInteger)
count       elementSize:(NSUInteger) sizeInBytes
	description:(const char *) descriptor
{
	return[[self alloc] initCount: count elementSize: sizeInBytes description:descriptor];
}

@end
