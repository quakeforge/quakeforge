/*
	object.c

	Provides a primitive object framework to back objects
	in higher level languages of QF so that they can share
	objects.  For example, Ruamoko and GIB would be able to
	pass String objects to each other, even if the higher
	-level implementations of String in each language differ.

	Copyright (C) 2003 Brian Koropoff

	Author: Brian Koropoff
	Date: November 28, 2003

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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/sys.h"
#include "QF/object.h"
#include "QF/va.h"
#include "garbage.h"

#include "QF/classes/ArrayList.h"
#include "QF/classes/Integer.h"
#include "QF/classes/Double.h"

Object *allObjs = NULL;
ArrayList *rootObj = NULL;

static qboolean
Object_Finalize_f (Object *self)
{
	return false;
}

static String *
Object_ToString_f (Object *self) 
{
	return newFloat(String, va("%s@%p", self->cl->name, self));
}

static void
Object_Init_f (Object *self) 
{
	self->allRefs = NULL;
	self->toString = Object_ToString_f;
	self->finalize = Object_Finalize_f;
	Sys_DPrintf("%s@%p initing...\n", self->cl->name, self);
}

static void
Object_Deinit_f (Object *self)
{
	Sys_DPrintf("%s@%p deiniting...\n", self->cl->name, self);
	free (self);
}

static String *
Class_ToString_f (Object *self)
{
	return newFloat(String, CLASS(self)->name);
}	

static Object *
Class_Init_f (Object *self, const char *name, unsigned int size, Class *parent, void *init, void *deinit, qboolean abstract)
{
	superInit(Class, self);
	CLASS(self)->name = strdup(name);
	CLASS(self)->size = size;
	CLASS(self)->parent = parent;
	CLASS(self)->init = (Object_Init_t) init;
	CLASS(self)->deinit = (Object_Deinit_t) deinit;
	self->toString = Class_ToString_f;
	return self;
}

static void
Class_Deinit_f (Object *self) {
	free ((void *)CLASS(self)->name);
}


Object *
Object_Create (Class *cl, qboolean floating)
{
	Object *new = calloc (1, cl->size);
	new->cl = cl;
	if (floating) {
		new->refs = 0;
	} else
		new->refs = 1;
	new->marked = false;
	new->finalized = false;
	new->next = allObjs;
	allObjs = new;
	return new;
}

void
Object_Delete (Object *obj)
{
	Class *c;
	for (c = obj->cl; c; c = c->parent)
		c->deinit (obj);
}

Object *
Object_Retain (Object *obj)
{
	obj->refs++;
	return obj;
}

Object *
Object_Release (Object *obj)
{
	if (obj->refs)
		obj->refs--;
	return obj;
}

qboolean
Object_InstanceOf (Object *obj, Class *cl)
{
	Class *c;
	for (c = obj->cl; c; c = c->parent)
		if (c == cl)
			return true;
	return false;
}

void
Object_AddToRoot (Object *obj)
{
	methodCall(COLLECTION(rootObj), add, obj);
}

void
Object_RemoveFromRoot (Object *obj)
{
	methodCall(COLLECTION(rootObj), remove, obj);
}

static void
Object_Test (void)
{
	String *liststr;
	Collection *list = newFloat(ArrayList, classObj(Object), NULL);
	
	methodCall(list, add, newFloat(String, "Testing..."));
	methodCall(list, add, newFloat(String, "One"));
	methodCall(list, add, newFloat(Integer, 2));
	methodCall(list, add, newFloat(Double, 3.0));
	
	liststr = methodCall(OBJECT(list), toString);
	Sys_DPrintf("List: %s\n", liststr->str);
	methodCall(LIST(list), removeAt, 2);
	liststr = methodCall(OBJECT(list), toString);
	Sys_DPrintf("List: %s\n", liststr->str);
	methodCall(LIST(list), insertAt, 2, newFloat(String, "Mr. Two!"));
	liststr = methodCall(OBJECT(list), toString);
	Sys_DPrintf("List: %s\n", liststr->str);

	list = newFloat(ArrayList, classObj(Object), NULL);
	methodCall(list, add, newFloat(String, "Don't free me!"));
	methodCall(list, add, newFloat(Integer, 5));
	methodCall(list, add, newFloat(Double, 3.14));
	Object_AddToRoot (OBJECT(methodCall(list, iterator)));
}

Class *Object_class;
Class *Class_class;

void
Object_Init (void)
{
	/* There is somewhat of a chicken and egg problem
	   here.
	*/
	Object_class = calloc (1, sizeof (struct Class_s));
	Class_class = calloc (1, sizeof (struct Class_s));
	OBJECT(Object_class)->cl = Class_class;
	OBJECT(Class_class)->cl = Class_class;
	Class_class->parent = Object_class;
	Object_class->init = (Object_Init_t) Object_Init_f; 
	
	Class_Init_f (OBJECT(Object_class), "Object", sizeof(Object), NULL, Object_Init_f, Object_Deinit_f, true);
	Class_Init_f (OBJECT(Class_class), "Class", sizeof(Class), Object_class, Class_Init_f, Class_Deinit_f, false);
	retain(Object_class);
	retain(Class_class);
	/* Phew... Object and Class are now bootstrapped,
	   classes can now be created by instantiating
	   Class
	*/

	/* Initialize standard classes */
	classInit(String);
	classInit(Number);
		classInit(Integer);
		classInit(Double);
	classInit(Iterator);
	classInit(Collection);
		classInit(List);
			classInit(ArrayList);
	
	rootObj = new(ArrayList, classObj(Object), NULL);

	/* Run test */
	Object_Test();
}

void
Object_Garbage_Collect (void)
{
	static unsigned int frames = 0;

	frames++;

	if (frames % 2000 == 0) {
		Object *all, *last;
		Sys_DPrintf("GC: Marking...\n");
		Garbage_Do_Mark (OBJECT(rootObj));
		Sys_DPrintf("GC: Sweeping...\n");
		all = allObjs;
		allObjs = NULL;
		last = Garbage_Do_Sweep (&all);
		last->next = allObjs;
		allObjs = all;
	}
	if (frames % 50 == 0 && Garbage_Pending()) {
		Sys_DPrintf("GC: Disposing...\n");
		Garbage_Dispose (&allObjs, Garbage_Pending()/2 + 1);
	}
}
