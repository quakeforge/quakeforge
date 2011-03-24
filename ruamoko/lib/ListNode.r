/*
	ListNode.h

	Stack/Queue/Linked List node class definitions

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

@implementation ListNode

+ (id) nodeWithObject: (id)anObject
{
	return [[[self alloc] initWithObject: anObject] autorelease];
}

- (id) initWithObject: (id)anObject
{
	if (!(self = [super init]))
		return nil;

	data = [anObject retain];
	nextNode = nil;
	return self;
}

- (void) dealloc
{
	[data release];
	[super dealloc];
}

- (id) nextNode
{
	return nextNode;
}

- (void) setNextNode: (id)aNode
{
	nextNode = aNode;
}

- (id) object
{
	return data;
}

@end
