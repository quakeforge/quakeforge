/*
	Stack.r

	A general-purpose stack class

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

#include "Object.h"
#include "ListNode.h"
#include "Stack.h"

@implementation Stack

- (void) removeAllObjects
{
	while ([self count] > 0)
		[self pop];
}

- (void) addObject: (id)anObject
{
	local id oldTop = top;
	top = [[ListNode alloc] initWithObject: anObject];
	[top setNextNode: oldTop];
	stackSize++;
}

- (id) pop
{
	local id	data;
	local id	oldTop = nil;

	if (!top)
		return nil;

	oldTop = top;
	top = [top nextNode];
	stackSize--;

	data = [[oldTop object] retain];
	[oldTop release];

	return [data autorelease];
}

- (id) peek
{
	return [top object];
}

- (integer) count
{

}

@end
