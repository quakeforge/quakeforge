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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#include <stdlib.h>

#include "QF/sys.h"
#include "QF/object.h"

#include "garbage.h"

unsigned int junked = 0;
Object *junk = NULL;

void
Garbage_Junk_Object (Object *o)
{
	Sys_DPrintf ("GC: %s@%p ready for collection.\n", o->cl->name, o);
	if (!o->junked && !o->refs) {
		o->next = junk;
		junk = o;
		o->junked = true;
		junked++;
	}
}

unsigned int
Garbage_Amount (void)
{
	return junked;
}


void
Garbage_Collect (unsigned int amount)
{
	Object *o;

	if (!amount)
		return;
	Sys_DPrintf ("GC: Collecting %u objects...\n", amount);

	for (o = junk; o && amount; o = junk) {
		junk = o->next;
		if (!o->refs) {
			Sys_DPrintf("GC: Collecting %s@%p...\n", o->cl->name, o);
			Object_Delete (o);
			amount--;
		} else {
			Sys_DPrintf("GC: %s@%p gained references, not collecting...\n", o->cl->name, o);
			o->junked = false;
		}
		junked--;
	}
	junk = o;
}
