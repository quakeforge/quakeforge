/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
#include <stdarg.h>
#include <string.h>

#include "QF/sys.h"
#include "QF/cbuf.h"
#include "QF/gib.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/llist.h"

#include "gib_handle.h"
#include "gib_tree.h"
#include "gib_function.h"
#include "gib_thread.h"

llist_t *gib_threads;
hashtab_t  *gib_events;

static void
GIB_Thread_Free (void *ele, void *data)
{
	Cbuf_DeleteStack ((cbuf_t *) ele);
}

cbuf_t *
GIB_Thread_New (void)
{
	cbuf_t *new = Cbuf_New (GIB_Interpreter ());
	llist_append (gib_threads, new);
	return new;
}

void
GIB_Thread_Delete (cbuf_t *thread)
{
	cbuf_t *temp;

	for (temp = thread; temp->down && temp->down->state != CBUF_STATE_JUNK; temp = temp->down);
	if (temp == cbuf_active)
		temp->state = CBUF_STATE_ERROR;
	else
		llist_remove (llist_getnode (gib_threads, thread));
}

unsigned int
GIB_Thread_Count (void)
{
	return llist_size (gib_threads);
}

static bool te_iterator (cbuf_t *cbuf, llist_node_t *node)
{
	if (GIB_DATA(cbuf)->program)
		Cbuf_Execute_Stack (cbuf);
	else
		Cbuf_DeleteStack ((cbuf_t *) llist_remove (node));
	return true;
}

VISIBLE void
GIB_Thread_Execute (void)
{
	llist_iterate (gib_threads, LLIST_ICAST (te_iterator));
}

static void
gib_thread_shutdown (void *data)
{
	llist_delete (gib_threads);
}

void
GIB_Thread_Init (void)
{
	qfZoneScoped (true);
	Sys_RegisterShutdown (gib_thread_shutdown, 0);
	gib_threads = llist_new (GIB_Thread_Free, NULL, NULL);
}

static const char *
GIB_Event_Get_Key (const void *ele, void *ptr)
{
	return ((gib_event_t *) ele)->name;
}

static void
GIB_Event_Free (void *ele, void *ptr)
{
	gib_event_t *ev = (gib_event_t *) ele;

	free ((void *) ev->name);
	free (ev);
}

VISIBLE gib_event_t *
GIB_Event_New (const char *name)
{
	gib_event_t *new;

	new = calloc (1, sizeof (gib_event_t));
	new->name = strdup (name);
	Hash_Add (gib_events, new);
	return new;
}

int
GIB_Event_Register (const char *name, gib_function_t * func)
{
	gib_event_t *ev;

	if (!(ev = Hash_Find (gib_events, name)))
		return -1;
	ev->func = func;
	return 0;
}

VISIBLE void
GIB_Event_Callback (gib_event_t * event, unsigned int argc, ...)
{
	gib_function_t *f = event->func;
	cbuf_t *thread;
	cbuf_args_t *args;
	va_list     ap;
	unsigned int i;

	if (!f)
		return;

	thread = GIB_Thread_New ();
	args = Cbuf_ArgsNew ();

	va_start (ap, argc);

	Cbuf_ArgsAdd (args, f->name);
	for (i = 0; i < argc; i++)
		Cbuf_ArgsAdd (args, va_arg (ap, const char *));

	va_end (ap);

	GIB_Function_Execute_D (thread, f, args->argv, args->argc);
	Cbuf_ArgsDelete (args);
}

static void
gib_event_shutdown (void *data)
{
	Hash_DelTable (gib_events);
}

void
GIB_Event_Init (void)
{
	qfZoneScoped (true);
	Sys_RegisterShutdown (gib_event_shutdown, 0);
	gib_events = Hash_NewTable (1024, GIB_Event_Get_Key, GIB_Event_Free, 0, 0);
}
