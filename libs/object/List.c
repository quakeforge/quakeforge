/*
	List.c

	Abstract class for lists

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#include "QF/classes/List.h"

static Object *
List_Remove_f (Collection *self, Object *o)
{
	unsigned int i = 0;
	Object *test;
	Iterator *iter = methodCall(self, iterator);

	while ((test = methodCall(iter, next))) {
		if (test == o) {
			methodCall(LIST(self), removeAt, i);
			return o;
		}
		i++;
	}
	return NULL;
}

static Object *
List_Init_f (Object *self, Class *type, Collection *source)
{
	superInit(List, self, type, source);
	COLLECTION(self)->remove = List_Remove_f;
	LIST(self)->smods = 0;
	return self;
}

static void
List_Deinit_f (Object *self)
{
}

classInitFunc(List) {
	classObj (List) = new (Class, 
			"List", sizeof(List), classObj(Collection), 
			List_Init_f, List_Deinit_f, true);
}
