/*
	object.h

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

	$Id$
*/

#ifndef __object_h
#define __object_h

#include "QF/qtypes.h"

#ifdef __GNUC__
#define superInit(cl, obj, args...) (cl##_class->parent->init ((obj), ##args))
#define new(cl, args...) ((void *) (cl##_class->abstract ? NULL : retain(cl##_class->init (Object_Create(cl##_class), ##args))))
#define newFloat(cl, args...) ((void *) (cl##_class->abstract ? NULL : cl##_class->init (Object_Create(cl##_class), ##args)))
#define methodCall(obj, m, args...) ((obj)->m(obj, ##args))
#define methodDecl(type, name, args...) (* name) (struct type##_s *self, ##args)
#else
#define superInit(cl, obj, ...) (cl##_class->parent->init ((obj), ##__VA_ARGS__))
#define new(cl, ...) ((void *) (cl##_class->abstract ? NULL : retain(cl##_class->init (Object_Create(cl##_class), ##__VA_ARGS__))))
#define newFloat(cl, ...) ((void *) (cl##_class->abstract ? NULL : cl##_class->init (Object_Create(cl##_class), ##__VA_ARGS__)))
#define methodCall(obj, m, ...) ((obj)->m(obj, ##__VA_ARGS__))
#define methodDecl(type, name, ...) (* name) (struct type##_s *self, ##__VA_ARGS__)
#endif

#define classDecl(name,extends,def) typedef struct name##_s {struct extends##_s base; def} name; extern Class * name##_class
#define retain(obj) (Object_Retain((Object *)obj))
#define release(obj) (Object_Release((Object *)obj))

typedef struct Object_s {
	struct Class_s *cl;
	int refs;
	struct String_s *(*toString)(struct Object_s *obj);
} Object;
#define OBJECT(o) ((Object *)(o))


typedef Object *(*Object_Init_t) (Object *obj, ...);
typedef void (*Object_Deinit_t) (Object *obj);

classDecl (Class, Object,
	qboolean abstract;
	unsigned int size;
	const char *name;
	struct Class_s *parent;
	Object_Init_t init;
	Object_Deinit_t deinit;
);
#define CLASS(o) ((Class *)(o))

classDecl (String, Object, 
	const char *str;
);
#define STRING(o) ((String *)(o))

classDecl (List, Object,
	unsigned int count;
	Object * methodDecl (List, get, unsigned int index);
	qboolean methodDecl (List, add, Object *o);
);
#define LIST(o) ((List *)(o))

classDecl (ArrayList, List,
	Object **elements;
);
#define ARRAYLIST(o) ((ArrayList *)(o))

classDecl (Number, Object,
	int methodDecl (Number, intValue);
	double methodDecl (Number, doubleValue);
);
#define NUMBER(o) ((Number *)(o))

classDecl (Integer, Number,
	int value;
);
#define INTEGER(o) ((Integer *)(o))

Object *Object_Create (Class *cl);
void Object_Delete (Object *obj);
Object *Object_Retain (Object *obj);
Object *Object_Release (Object *obj);

void Object_Init (void);

#endif
