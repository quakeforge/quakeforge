/*
	bi_gib.c

	GIB <-> Ruamoko interface

	Copyright (C) 2003 Brian Koropoff

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

#include <stdlib.h>

#include "QF/gib.h"

#include "gib_handle.h"

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

static unsigned long int gib_next_handle, gib_next_class;
static gib_handle_t *gib_unused_handles;
static gib_handle_t **gib_handles;
static unsigned long int gib_handles_size;

unsigned long int
GIB_Handle_New (void *data, unsigned short int class)
{
	gib_handle_t *new;
	if (gib_unused_handles) {
		new = gib_unused_handles;
		gib_unused_handles = new->next;
	} else {
		unsigned long int num = gib_next_handle++;
		if (num >= gib_handles_size) {
			gib_handles_size += 256;
			gib_handles = realloc (gib_handles, sizeof(void *) * gib_handles_size);
		}
		new = calloc (1, sizeof (gib_handle_t));
		new->num = num;
	}
	new->data = data;
	new->class = class;
	gib_handles[new->num] = new;
	return new->num;
}

void
GIB_Handle_Free (unsigned long int num, unsigned short int class)
{
	gib_handle_t *hand;

	if (num >= gib_next_handle || gib_handles[num]->class != class)
		return;
	hand = gib_handles[num];
	gib_handles[num] = 0;
	hand->next = gib_unused_handles;
	gib_unused_handles = hand;
}

void *
GIB_Handle_Get (unsigned long int num, unsigned short int class)
{
	if (num >= gib_next_handle || !gib_handles[num] || gib_handles[num]->class != class)
		return 0;
	return gib_handles[num]->data;
}

unsigned short int
GIB_Handle_Class_New (void)
{
	return gib_next_class++;
}

void
GIB_Handle_Init (void)
{
	gib_handles_size = 256;
	gib_handles = calloc (gib_handles_size, sizeof (gib_handle_t *));
	gib_next_class = 0;
	gib_next_handle = 0;
	gib_unused_handles = 0;
}
