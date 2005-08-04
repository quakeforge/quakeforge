/*
	Collection.c

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#include "QF/dstring.h"
#include "QF/classes/Collection.h"

static String *
Collection_ToString_f (Object *self)
{
	Iterator *iter = methodCall(COLLECTION(self), iterator);
	dstring_t *dstr = dstring_newstr();
	Object *o;
	String *ret;

	dstring_appendstr (dstr, "[");

	while ((o = methodCall(iter, next))) {
		String *s = methodCall(o, toString);
		dstring_appendstr (dstr, s->str);
		if (methodCall(iter, hasNext))
			dstring_appendstr (dstr, ", ");
	}

	dstring_appendstr (dstr, "]");

	ret = new(String, dstr->str);
	dstring_delete (dstr);
	
	return ret;
}	

static qboolean
Collection_Contains_f (Collection *self, Object *test)
{
	Iterator *iter = methodCall(self, iterator);
	Object *o;

	while ((o = methodCall(iter, next)))
		if (o == test)
			return true;
	return false;
}

static Object *
Collection_Init_f (Object *self, Class *type, Collection *source)
{
	superInit(Collection, self);
	COLLECTION(self)->contains = Collection_Contains_f;
	COLLECTION(self)->count = 0;
	COLLECTION(self)->type = type;
	self->toString = Collection_ToString_f;
	// FIXME allow for subclasses too
	if (source && source->type == type) {
		Iterator *iter = methodCall(source, iterator);
		Object *o;

		while ((o = methodCall(iter, next)))
			methodCall(COLLECTION(self), add, o);
	}
	return self;
}

static void
Collection_Deinit_f (Object *self)
{
}

classInitFunc(Collection) {
	classObj (Collection) = new (Class, 
			"Collection", sizeof(Collection), classObj(Object), 
			Collection_Init_f, Collection_Deinit_f, true);
}
