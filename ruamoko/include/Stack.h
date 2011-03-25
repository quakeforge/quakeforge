/*
	Stack.h

	Stack class definitions

	Copyright (C) 2003 Jeff Teunissen <deek@quakeforge.net>

	This file is part of the Ruamoko Standard Library.

	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.

	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifndef __ruamoko_Stack_h
#define __ruamoko_Stack_h

#include "Object.h"

@interface Stack: Object
{
	id		top;		// Top of the stack
	int	stackSize;
}

- (void) removeAllObjects;			// Empty the stack
- (void) addObject: (id)anObject;	// Push anObject onto the stack
- (id) pop;							// pull top object off
- (id) peek;						// Grab the top object without removing it
- (int) count;					// Number of objects on stack

@end

#endif	// __ruamoko_Stack_h
