/*
	garbage.c

	Object system garbage collector.

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#include <stdlib.h>

#include "QF/sys.h"
#include "QF/object.h"

#include "garbage.h"

Object *junk = NULL;
unsigned int junked = 0;

void
Garbage_Do_Mark (Object *root)
{
	if (!root->marked) {
		ObjRefs_t *allrefs;
		root->marked = true;
		Sys_DPrintf ("GC: Marked %s@%p.\n", root->cl->name, root);
		if (root->allRefs)
			for (allrefs = methodCall(root, allRefs); allrefs; allrefs = allrefs->next) {
				unsigned int i;
				for (i = 0; i < allrefs->count; i++)
					Garbage_Do_Mark (allrefs->objs[i]);
			}
	}
}

Object *
Garbage_Do_Sweep (Object **allobjs)
{
	Object **prevNext;
	Object *obj;

	for (prevNext = allobjs, obj = *allobjs;; obj = *prevNext) {
		if (obj->marked) {
			obj->marked = false;
			prevNext = &obj->next;
		} else if (!obj->refs) {
			if (!obj->finalized && methodCall(obj, finalize)) {
				obj->finalized = true;
				Garbage_Do_Mark (obj);
				prevNext = &obj->next;
			} else {
				*prevNext = obj->next;
				obj->next = junk;
				junk = obj;
				junked++;
				Sys_DPrintf ("GC: %s@%p is ready for disposal...\n", obj->cl->name, obj);
			}
		} else
			*prevNext = obj->next;
		if (!*prevNext)
			return obj;
	}
}

unsigned int
Garbage_Pending (void)
{
	return junked;
}

void
Garbage_Dispose (Object **allobjs, unsigned int amount)
{
	Object *next;

	for (; junk && amount; junk = next, amount--) {
		next = junk->next;
		if (junk->marked) {
			junk->marked = false;
			junk->next = *allobjs;
			*allobjs = junk;
		} else {
			Sys_DPrintf ("GC: Disposing of %s@%p...\n", junk->cl->name, junk);
			Object_Delete (junk);
			junked--;
		}
	}
}
