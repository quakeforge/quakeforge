/*
	ArrayList.h

	Implements List by storing elements in a dynamic array

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

#ifndef __ArrayList_h
#define __ArrayList_h

#include "QF/classes/List.h"

classDecl (ArrayList, List,
	unsigned int realsize;
	Object **elements;
	ObjRefs_t allrefs;
);
#define ARRAYLIST(o) ((ArrayList *)(o))

classDecl (ArrayListIterator, Iterator,
	ArrayList *list;
	unsigned int pos;
	unsigned int smods;
	bool alive;
	ObjRefs_t allrefs;
);
#define ARRAYLISTITERATOR(o) ((ArrayListIterator *)(o))

#endif
