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

static const char rcsid[] =
        "$Id$";

#include <stdlib.h> 
#include <stdarg.h>

#include "QF/sys.h"       
#include "QF/cbuf.h"
#include "QF/gib_parse.h"
#include "QF/gib_thread.h"
#include "QF/gib_function.h"
#include "QF/dstring.h"

gib_thread_t *gib_threads;
static unsigned long int nextid = 0;

void
GIB_Thread_Add (gib_thread_t *thread)
{
	thread->prev = 0;
	thread->next = gib_threads;
	if (gib_threads)
		gib_threads->prev = thread;
	gib_threads = thread;
}

void
GIB_Thread_Remove (gib_thread_t *thread) 
{
	if (thread == gib_threads) {
		gib_threads = gib_threads->next;
		if (gib_threads)
			gib_threads->prev = 0;
	} else {
		thread->prev->next = thread->next;
		if (thread->next)
			thread->next->prev = thread->prev;
	}
}

gib_thread_t *
GIB_Thread_Find (unsigned long int id)
{
	gib_thread_t *cur;
	
	for (cur = gib_threads; cur; cur=cur->next)
		if (cur->id == id)
			return cur;
	return 0;
}

gib_thread_t *
GIB_Thread_New (void)
{
	gib_thread_t *new = calloc (1, sizeof(gib_thread_t));
	new->cbuf = Cbuf_New (&gib_interp);
	new->id = nextid;
	nextid++;
	return new;
}

void
GIB_Thread_Delete (gib_thread_t *thread)
{
	Cbuf_DeleteStack (thread->cbuf);
	free (thread);
}

void
GIB_Thread_Execute (void)
{
	gib_thread_t *cur, *tmp;
	if (!gib_threads)
		return;
	
	for (cur = gib_threads; cur->next; cur = cur->next);
	for (; cur; cur = tmp) {
		tmp = cur->prev;
		if (!cur->cbuf->buf->str[0] && !cur->cbuf->down) {
			GIB_Thread_Remove (cur);
			GIB_Thread_Delete (cur);
		} else
			Cbuf_Execute_Stack (cur->cbuf);
	}
}

void
GIB_Thread_Callback (const char *func, unsigned int argc, ...)
{
	gib_function_t *f = GIB_Function_Find (func);
	gib_thread_t *thread;
	cbuf_args_t *args;
	va_list ap;
	unsigned int i;
	
	if (!f)
		return;
		
	thread = GIB_Thread_New ();
	args = Cbuf_ArgsNew ();
	
	va_start (ap, argc);
	
	Cbuf_ArgsAdd (args, func);
	for (i = 0; i < argc; i++)
			Cbuf_ArgsAdd (args, va_arg (ap, const char *));
			
	va_end (ap);
	
	GIB_Function_Execute (thread->cbuf, f, args);
	GIB_Thread_Add (thread);
	Cbuf_ArgsDelete (args);
}
