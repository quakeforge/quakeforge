/* Interface for Objective C NeXT-compatible Storage object
   Copyright (C) 1993,1994 Free Software Foundation, Inc.

   Written by:  Kresten Krab Thorup <krab@iesd.auc.dk>
   Dept. of Mathematics and Computer Science, Aalborg U., Denmark

   This file is part of the Gnustep Base Library.

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

/******************************************************************
  TODO:
   Does not implement methods for archiving itself.
******************************************************************/

#ifndef __Storage_h_INCLUDE_GNU
#define __Storage_h_INCLUDE_GNU

#include <AppKit/AppKit.h>

@interface Storage: NSObject
{
	@public
	void        *dataPtr;           /* data of the Storage object */
	const char  *description;       /* Element description */
	NSUInteger  numElements;        /* Actual number of elements */
	NSUInteger  maxElements;        /* Total allocated elements */
	NSUInteger  elementSize;        /* Element size */
}

/* Creating, freeing, initializing, and emptying */

- (id) init;
- (id) initCount: (NSUInteger)numSlots
   elementSize: (NSUInteger)sizeInBytes
   description: (const char *)elemDesc;
- (void) dealloc;
- (id) empty;
- (id) copy;

/* Manipulating the elements */

- (BOOL) isEqual: anObject;
- (const char *) description;
- (NSUInteger) count;
- (void *) elementAt: (NSUInteger)index;
- (id) replaceElementAt: (NSUInteger)index
   with: (void *)anElement;

- (id) setNumSlots: (NSUInteger)numSlots;
- (id) setAvailableCapacity: (NSUInteger)numSlots;
- (id) addElement: (void *)anElement;
- (id) removeLastElement;
- (id) insertElement: (void *)anElement
   at: (NSUInteger)index;

- (id) removeElementAt: (NSUInteger)index;

/* old-style creation */

+ (id) new;
+ (id) newCount: (NSUInteger)count
   elementSize: (NSUInteger)sizeInBytes
   description: (const char *)descriptor;

@end

typedef struct {
	@defs (Storage)
} NXStorageId;

#endif /* __Storage_h_INCLUDE_GNU */
