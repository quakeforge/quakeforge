/*
	Collection.h

	Abstract class for collections of objects

	Copyright (C) 2003 Brian Koropoff

	Author: Brian Koropoff
	Date: December 03, 2003

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#include "QF/object.h"

#ifndef __Collection_h
#define __Collection_h

#include "QF/classes/Iterator.h"

classDecl (Collection, Object,
	unsigned int count;
	Class *type;
	bool methodDecl (Collection, add, Object *o);
	Object * methodDecl (Collection, remove, Object *o);
	bool methodDecl (Collection, contains, Object *o);
	Iterator * methodDecl (Collection, iterator);
);
#define COLLECTION(o) ((Collection *)(o))

#endif
